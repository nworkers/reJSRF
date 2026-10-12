// The host mixer: decodes playing buffers straight from guest memory (the
// original Unlock does nothing, so the game writes into buffers as they play),
// resamples them to the output rate, folds mix bins onto stereo, and advances
// the play cursors by what it rendered.

#include "dsound_internal.h"
#include "rexbox/platform/audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rexbox::hle::dsound
{

namespace
{

constexpr float kCenterGain = 0.70710678f;  // a center or LFE bin feeds both sides at -3 dB

// IMA ADPCM step sizes and index adjustments.
constexpr int16_t kAdpcmSteps[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,
    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,
    9493,  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr int8_t kAdpcmIndexAdjust[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

float VolumeGain(int32_t hundredths_db)
{
    if (hundredths_db <= -10000)
    {
        return 0.0f;
    }
    if (hundredths_db >= 0)
    {
        return 1.0f;
    }
    return std::pow(10.0f, static_cast<float>(hundredths_db) / 2000.0f);
}

uint32_t BlockAlign(const Buffer& buffer)
{
    return std::max<uint32_t>(1, buffer.format.block_align);
}

bool IsAdpcm(const Buffer& buffer)
{
    return buffer.format.tag == kFormatXboxAdpcm;
}

int DecodeNibble(int nibble, int& predictor, int& index)
{
    const int step = kAdpcmSteps[index];
    int diff = step >> 3;
    if ((nibble & 4) != 0)
    {
        diff += step;
    }
    if ((nibble & 2) != 0)
    {
        diff += step >> 1;
    }
    if ((nibble & 1) != 0)
    {
        diff += step >> 2;
    }
    predictor += (nibble & 8) != 0 ? -diff : diff;
    predictor = std::clamp(predictor, -32768, 32767);
    index = std::clamp(index + kAdpcmIndexAdjust[nibble], 0, 88);
    return predictor;
}

// Decodes ADPCM block `block` into buffer.decoded (frames interleaved by
// channel). Each channel's block starts with {int16 sample, uint8 step index,
// uint8 reserved}; the nibbles follow in 4-byte groups, channels taking turns,
// low nibble first. The header sample is frame 0.
void DecodeAdpcmBlock(Buffer& buffer, uint32_t block)
{
    const uint32_t channels = buffer.format.channels;
    const uint8_t* in = Memory().Translate(buffer.data + block * BlockAlign(buffer));
    int predictor[kMaxChannels] = {};
    int index[kMaxChannels] = {};
    for (uint32_t c = 0; c < channels; ++c)
    {
        predictor[c] = static_cast<int16_t>(in[c * 4] | (in[c * 4 + 1] << 8));
        index[c] = std::min<int>(in[c * 4 + 2], 88);
        buffer.decoded[c] = static_cast<int16_t>(predictor[c]);
    }
    const uint8_t* nibbles = in + 4 * channels;
    for (uint32_t group = 0; group < 8; ++group)
    {
        for (uint32_t c = 0; c < channels; ++c)
        {
            const uint8_t* bytes = nibbles + (group * channels + c) * 4;
            for (uint32_t k = 0; k < 8; ++k)
            {
                const uint32_t frame = 1 + group * 8 + k;
                const int nibble = (k & 1) == 0 ? (bytes[k / 2] & 0x0F) : (bytes[k / 2] >> 4);
                const int sample = DecodeNibble(nibble, predictor[c], index[c]);
                if (frame < kAdpcmSamplesPerBlock)
                {
                    buffer.decoded[frame * channels + c] = static_cast<int16_t>(sample);
                }
            }
        }
    }
    buffer.decoded_block = block;
}

float SampleAt(Buffer& buffer, uint32_t frame, uint32_t channel)
{
    if (IsAdpcm(buffer))
    {
        const uint32_t block = frame / kAdpcmSamplesPerBlock;
        if (buffer.decoded_block != static_cast<int64_t>(block))
        {
            DecodeAdpcmBlock(buffer, block);
        }
        const uint32_t index = (frame % kAdpcmSamplesPerBlock) * buffer.format.channels + channel;
        return static_cast<float>(buffer.decoded[index]) / 32768.0f;
    }
    const uint8_t* at = Memory().Translate(buffer.data + frame * BlockAlign(buffer));
    if (buffer.format.bits == 8)
    {
        return (static_cast<float>(at[channel]) - 128.0f) / 128.0f;
    }
    int16_t value = 0;
    std::memcpy(&value, at + channel * 2, 2);
    return static_cast<float>(value) / 32768.0f;
}

// Adds a mix bin's contribution for one channel of the voice.
void AddBin(Buffer& buffer, uint32_t channel, uint32_t bin, float gain)
{
    switch (bin)
    {
    case 0:  // FRONT_LEFT
    case 4:  // BACK_LEFT
    case 6:  // XTLK_FRONT_LEFT
    case 8:  // XTLK_BACK_LEFT
        buffer.gain_left[channel] += gain;
        break;
    case 1:  // FRONT_RIGHT
    case 5:  // BACK_RIGHT
    case 7:  // XTLK_FRONT_RIGHT
    case 9:  // XTLK_BACK_RIGHT
        buffer.gain_right[channel] += gain;
        break;
    case 2:  // FRONT_CENTER
    case 3:  // LOW_FREQUENCY
        buffer.gain_left[channel] += gain * kCenterGain;
        buffer.gain_right[channel] += gain * kCenterGain;
        break;
    default:  // I3DL2 and FXSEND_n feed the effects DSP, which does not run
        break;
    }
}

void MixBuffer(Buffer& buffer, float* samples, uint32_t frames)
{
    if (!buffer.playing || !buffer.valid_format || buffer.data == 0)
    {
        return;
    }
    const uint32_t total = TotalFrames(buffer);
    if (total == 0)
    {
        return;
    }
    uint32_t loop_begin = 0;
    uint32_t loop_end = total;
    if (buffer.looping)
    {
        loop_begin = std::min(FramesFromBytes(buffer, buffer.loop_start), total - 1);
        if (buffer.loop_length != 0)
        {
            loop_end = std::min<uint32_t>(loop_begin + std::max<uint32_t>(1, FramesFromBytes(buffer, buffer.loop_length)),
                                          total);
        }
    }
    const uint32_t rate = buffer.frequency != 0 ? buffer.frequency : buffer.format.rate;
    const double step = static_cast<double>(rate) / static_cast<double>(kOutputRate);
    const uint32_t channels = buffer.format.channels;
    const uint32_t end = buffer.looping ? loop_end : total;
    double position = buffer.position;
    for (uint32_t i = 0; i < frames; ++i)
    {
        if (buffer.looping)
        {
            while (position >= loop_end)
            {
                position -= static_cast<double>(loop_end - loop_begin);
            }
        }
        else if (position >= total)
        {
            // A one-shot voice stops at the end; the saved position Play
            // zeroed is what GetCurrentPosition then reports.
            buffer.playing = false;
            buffer.saved_bytes = 0;
            position = 0.0;
            break;
        }
        const uint32_t frame = static_cast<uint32_t>(position);
        const float fraction = static_cast<float>(position - frame);
        uint32_t next = frame + 1;
        if (next >= end)
        {
            next = buffer.looping ? loop_begin : frame;
        }
        float left = 0.0f;
        float right = 0.0f;
        for (uint32_t c = 0; c < channels; ++c)
        {
            const float a = SampleAt(buffer, frame, c);
            const float b = SampleAt(buffer, next, c);
            const float value = a + (b - a) * fraction;
            left += value * buffer.gain_left[c];
            right += value * buffer.gain_right[c];
        }
        samples[2 * i] += left;
        samples[2 * i + 1] += right;
        position += step;
    }
    if (buffer.playing)
    {
        if (buffer.looping)
        {
            while (position >= loop_end)
            {
                position -= static_cast<double>(loop_end - loop_begin);
            }
        }
        else if (position >= total)
        {
            buffer.playing = false;
            buffer.saved_bytes = 0;
            position = 0.0;
        }
    }
    buffer.position = position;
}

void RenderOutput(float* samples, uint32_t frames, void* /*context*/)
{
    std::memset(samples, 0, sizeof(float) * 2 * frames);
    {
        DsoundState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        MixLocked(samples, frames);
    }
    for (uint32_t i = 0; i < 2 * frames; ++i)
    {
        samples[i] = std::clamp(samples[i], -1.0f, 1.0f);
    }
}

}  // namespace

uint32_t TotalFrames(const Buffer& buffer)
{
    return FramesFromBytes(buffer, buffer.bytes);
}

uint32_t BytesFromFrames(const Buffer& buffer, uint32_t frames)
{
    if (IsAdpcm(buffer))
    {
        return (frames / kAdpcmSamplesPerBlock) * BlockAlign(buffer);
    }
    return frames * BlockAlign(buffer);
}

uint32_t FramesFromBytes(const Buffer& buffer, uint32_t bytes)
{
    if (IsAdpcm(buffer))
    {
        return (bytes / BlockAlign(buffer)) * kAdpcmSamplesPerBlock;
    }
    return bytes / BlockAlign(buffer);
}

void UpdateGains(Buffer& buffer)
{
    std::fill(std::begin(buffer.gain_left), std::end(buffer.gain_left), 0.0f);
    std::fill(std::begin(buffer.gain_right), std::end(buffer.gain_right), 0.0f);
    const uint32_t channels = std::clamp<uint32_t>(buffer.format.channels, 1, kMaxChannels);
    const float master = VolumeGain(buffer.volume);
    if (!buffer.has_mix_bins)
    {
        if (channels == 1)
        {
            buffer.gain_left[0] = master;
            buffer.gain_right[0] = master;
            return;
        }
        for (uint32_t c = 0; c < channels; ++c)
        {
            AddBin(buffer, c, c, master);
        }
        return;
    }
    for (size_t k = 0; k < buffer.mix_bins.size(); ++k)
    {
        const MixBinPair& pair = buffer.mix_bins[k];
        AddBin(buffer, static_cast<uint32_t>(k % channels), pair.bin, master * VolumeGain(pair.volume));
    }
}

void MixLocked(float* samples, uint32_t frames)
{
    for (auto& entry : State().buffers)
    {
        MixBuffer(entry.second, samples, frames);
    }
}

void StartOutputLocked()
{
    DsoundState& state = State();
    if (state.output_started || state.output == DsoundOutput::kManual)
    {
        return;
    }
    state.output_started = true;
    platform::AudioOutputConfig config;
    config.sample_rate = kOutputRate;
    config.null_device = state.output == DsoundOutput::kNull;
    platform::AudioOutputInfo info;
    // The output thread takes the mutex this caller holds only once it has
    // started rendering, which waits for the caller to release it.
    if (!platform::StartAudioOutput(config, &RenderOutput, nullptr, &info))
    {
        std::fprintf(stderr, "[dsound] no audio output (%s): play cursors do not move\n", info.reason.c_str());
        return;
    }
    if (info.host_device)
    {
        std::fprintf(stderr, "[dsound] audio output: host device, %u Hz stereo\n", info.sample_rate);
    }
    else
    {
        std::fprintf(stderr, "[dsound] audio output: none (%s); cursors run in real time\n", info.reason.c_str());
    }
}

}  // namespace rexbox::hle::dsound
