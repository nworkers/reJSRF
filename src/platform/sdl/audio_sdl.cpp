// Audio contract on SDL3: one playback stream on the default device, fed by
// SDL's get callback, or a pacing thread when there is no device.
//
// The stream is opened in the contract's format (stereo float); SDL converts
// it to whatever the device uses. The callback runs on SDL's audio thread and
// asks for exactly what the device is about to consume, so what was rendered
// tracks what was heard to within one device period.

#include "rexbox/platform/audio.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace rexbox::platform
{

namespace
{

// A device period of 512 frames (about 10.7 ms at 48 kHz) keeps cursor
// reports close to what is audible.
constexpr const char* kDevicePeriodFrames = "512";
constexpr uint32_t kPacingChunkFrames = 512;
constexpr auto kPacingInterval = std::chrono::milliseconds(5);

std::mutex g_mutex;
bool g_started = false;
AudioRenderFunction g_render = nullptr;
void* g_context = nullptr;
uint32_t g_rate = 0;

SDL_AudioStream* g_stream = nullptr;
bool g_audio_subsystem = false;

std::thread g_pacer;
std::atomic<bool> g_pacer_stop{false};

void SDLCALL StreamCallback(void* /*userdata*/, SDL_AudioStream* stream, int additional_amount,
                            int /*total_amount*/)
{
    constexpr int kFrameBytes = static_cast<int>(sizeof(float) * kAudioChannels);
    if (additional_amount <= 0)
    {
        return;
    }
    thread_local std::vector<float> samples;
    const uint32_t frames = static_cast<uint32_t>((additional_amount + kFrameBytes - 1) / kFrameBytes);
    samples.resize(static_cast<size_t>(frames) * kAudioChannels);
    g_render(samples.data(), frames, g_context);
    SDL_PutAudioStreamData(stream, samples.data(), static_cast<int>(frames) * kFrameBytes);
}

// Renders at real-time pace from a steady clock and drops the samples.
void PacerMain()
{
    std::vector<float> samples(static_cast<size_t>(kPacingChunkFrames) * kAudioChannels);
    const auto start = std::chrono::steady_clock::now();
    uint64_t rendered = 0;
    while (!g_pacer_stop.load())
    {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const uint64_t due =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) * g_rate /
            1000000u;
        while (rendered < due && !g_pacer_stop.load())
        {
            const uint32_t frames = static_cast<uint32_t>(std::min<uint64_t>(due - rendered, kPacingChunkFrames));
            g_render(samples.data(), frames, g_context);
            rendered += frames;
        }
        std::this_thread::sleep_for(kPacingInterval);
    }
}

bool OpenDevice(AudioOutputInfo* info)
{
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        info->reason = std::string("SDL_InitSubSystem(AUDIO) failed: ") + SDL_GetError();
        return false;
    }
    g_audio_subsystem = true;
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, kDevicePeriodFrames);
    SDL_AudioSpec spec = {};
    spec.format = SDL_AUDIO_F32;
    spec.channels = static_cast<int>(kAudioChannels);
    spec.freq = static_cast<int>(g_rate);
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &StreamCallback, nullptr);
    if (g_stream == nullptr)
    {
        info->reason = std::string("SDL_OpenAudioDeviceStream failed: ") + SDL_GetError();
        return false;
    }
    if (!SDL_ResumeAudioStreamDevice(g_stream))
    {
        info->reason = std::string("SDL_ResumeAudioStreamDevice failed: ") + SDL_GetError();
        SDL_DestroyAudioStream(g_stream);
        g_stream = nullptr;
        return false;
    }
    return true;
}

}  // namespace

bool StartAudioOutput(const AudioOutputConfig& config, AudioRenderFunction render, void* context,
                      AudioOutputInfo* info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    *info = AudioOutputInfo{};
    if (g_started || render == nullptr || config.sample_rate == 0)
    {
        info->reason = g_started ? "the audio output is already started" : "invalid audio output configuration";
        return false;
    }
    g_render = render;
    g_context = context;
    g_rate = config.sample_rate;
    info->sample_rate = g_rate;
    if (config.null_device)
    {
        info->reason = "no host device requested";
    }
    else if (OpenDevice(info))
    {
        info->host_device = true;
        g_started = true;
        return true;
    }
    g_pacer_stop.store(false);
    try
    {
        g_pacer = std::thread(&PacerMain);
    }
    catch (const std::system_error& e)
    {
        info->reason += std::string("; the pacing thread cannot start: ") + e.what();
        return false;
    }
    g_started = true;
    return true;
}

void StopAudioOutput()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_started)
    {
        return;
    }
    if (g_stream != nullptr)
    {
        SDL_DestroyAudioStream(g_stream);  // closes the device; no callback runs after this
        g_stream = nullptr;
    }
    if (g_pacer.joinable())
    {
        g_pacer_stop.store(true);
        g_pacer.join();
    }
    if (g_audio_subsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        g_audio_subsystem = false;
    }
    g_started = false;
}

}  // namespace rexbox::platform
