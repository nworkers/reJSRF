// The DirectSound object: creation, reference counts, caps, speaker
// configuration, buffer creation, and the listener settings (accepted, not
// rendered).

#include "dsound_internal.h"

#include <algorithm>

namespace rexbox::hle::dsound
{

namespace
{

// Reads a WAVEFORMATEX and tells whether the mixer can decode it.
bool ReadFormat(GuestAddress address, WaveFormat* format)
{
    runtime::GuestMemory& memory = Memory();
    format->tag = memory.Read16(address + layout::kWaveFormatTag);
    format->channels = memory.Read16(address + layout::kWaveChannels);
    format->rate = memory.Read32(address + layout::kWaveSamplesPerSec);
    format->block_align = memory.Read16(address + layout::kWaveBlockAlign);
    format->bits = memory.Read16(address + layout::kWaveBitsPerSample);
    if (format->channels == 0 || format->channels > kMaxChannels || format->rate == 0)
    {
        return false;
    }
    if (format->tag == kFormatPcm)
    {
        return (format->bits == 8 || format->bits == 16) &&
               format->block_align == format->channels * (format->bits / 8);
    }
    if (format->tag == kFormatXboxAdpcm)
    {
        return format->block_align == kAdpcmBlockBytes * format->channels;
    }
    return false;
}

// HRESULT DirectSoundCreate(LPGUID pguidDeviceId, LPDIRECTSOUND* ppDirectSound,
//     LPUNKNOWN pUnkOuter)
// There is one DirectSound object; creating it again adds a reference.
void DirectSoundCreate()
{
    HleCall call;
    const GuestAddress out = call.Arg(1);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.device == 0)
    {
        state.device = AllocateGuest(kObjectBlockSize);
        if (state.device == 0)
        {
            call.Return(kErrOutOfMemory, 3);
            return;
        }
        state.device_refs = 0;
        StartOutputLocked();
    }
    ++state.device_refs;
    if (out != 0)
    {
        Memory().Write32(out, state.device);
    }
    std::fprintf(stderr, "[dsound] DirectSoundCreate -> %08X (refs %u)\n", state.device, state.device_refs);
    call.Return(kDsOk, 3);
}

// ULONG IDirectSound_AddRef(LPDIRECTSOUND pThis)
void IDirectSound_AddRef()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (call.Arg(0) != state.device || state.device == 0)
    {
        call.Return(0, 1);
        return;
    }
    call.Return(++state.device_refs, 1);
}

// ULONG IDirectSound_Release(LPDIRECTSOUND pThis)
// The last reference frees the object; buffers live on until their own
// release (they hold their data, not the object).
void IDirectSound_Release()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (call.Arg(0) != state.device || state.device == 0 || state.device_refs == 0)
    {
        call.Return(0, 1);
        return;
    }
    const uint32_t refs = --state.device_refs;
    if (refs == 0)
    {
        Heap().Free(state.device);
        state.device = 0;
    }
    call.Return(refs, 1);
}

// HRESULT IDirectSound_GetCaps(LPDIRECTSOUND pThis, LPDSCAPS pdsc)
void IDirectSound_GetCaps()
{
    HleCall call;
    const GuestAddress caps = call.Arg(1);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (caps == 0)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    uint32_t voices = 0;
    uint32_t voices_3d = 0;
    uint32_t pages = 0;
    for (const auto& entry : state.buffers)
    {
        ++voices;
        voices_3d += (entry.second.flags & kCaps3d) != 0 ? 1 : 0;
        pages += (entry.second.bytes + runtime::GuestHeap::kPageSize - 1) / runtime::GuestHeap::kPageSize;
    }
    const uint32_t free_2d = voices < kHardwareVoices ? kHardwareVoices - voices : 0;
    const uint32_t free_3d = std::min(free_2d, voices_3d < kHardware3dVoices ? kHardware3dVoices - voices_3d : 0);
    runtime::GuestMemory& memory = Memory();
    memory.Write32(caps + layout::kCapsFree2dBuffers, free_2d);
    memory.Write32(caps + layout::kCapsFree3dBuffers, free_3d);
    memory.Write32(caps + layout::kCapsFreeBufferSges, pages < kHardwareSges ? kHardwareSges - pages : 0);
    memory.Write32(caps + layout::kCapsMemoryAllocated, state.allocated_bytes);
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSound_GetSpeakerConfig(LPDIRECTSOUND pThis, LPDWORD pdwSpeakerConfig)
// DSSPEAKER_STEREO, matching the kernel's XC_AUDIO_FLAGS and the stereo output.
void IDirectSound_GetSpeakerConfig()
{
    HleCall call;
    const GuestAddress out = call.Arg(1);
    if (out != 0)
    {
        Memory().Write32(out, 0);
    }
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSound_CreateSoundBuffer(LPDIRECTSOUND pThis, LPCDSBUFFERDESC pdsbd,
//     LPDIRECTSOUNDBUFFER* ppBuffer, LPUNKNOWN pUnkOuter)
void IDirectSound_CreateSoundBuffer()
{
    HleCall call;
    const GuestAddress desc = call.Arg(1);
    const GuestAddress out = call.Arg(2);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (desc == 0 || out == 0)
    {
        call.Return(kErrInvalidCall, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    Buffer buffer;
    buffer.flags = memory.Read32(desc + layout::kDescFlags);
    const uint32_t bytes = memory.Read32(desc + layout::kDescBufferBytes);
    const GuestAddress format = memory.Read32(desc + layout::kDescFormat);
    const GuestAddress mix_bins = memory.Read32(desc + layout::kDescMixBins);
    const bool submix = (buffer.flags & (kCapsMixIn | kCapsFxIn)) != 0;
    if (format != 0)
    {
        buffer.valid_format = ReadFormat(format, &buffer.format);
    }
    if (!buffer.valid_format && !submix)
    {
        std::fprintf(stderr,
                     "[dsound] CreateSoundBuffer: format tag %04X, %u ch, %u Hz, %u bits, align %u is not decoded; "
                     "the buffer is silent\n",
                     buffer.format.tag, buffer.format.channels, buffer.format.rate, buffer.format.bits,
                     buffer.format.block_align);
    }
    const GuestAddress object = AllocateGuest(kObjectBlockSize);
    if (object == 0)
    {
        call.Return(kErrOutOfMemory, 4);
        return;
    }
    if (bytes != 0 && !submix)
    {
        buffer.data = AllocateGuest(bytes);
        if (buffer.data == 0)
        {
            Heap().Free(object);
            call.Return(kErrOutOfMemory, 4);
            return;
        }
        buffer.bytes = bytes;
        buffer.owns_data = true;
        state.allocated_bytes += bytes;
    }
    if (mix_bins != 0)
    {
        ReadMixBins(buffer, mix_bins);
    }
    UpdateGains(buffer);
    if (state.trace)
    {
        std::fprintf(stderr,
                     "[dsound] CreateSoundBuffer -> %08X: flags %08X, %u bytes at %08X, format %04X %u ch %u Hz %u "
                     "bits align %u, %zu mix bin(s)",
                     object, buffer.flags, buffer.bytes, buffer.data, buffer.format.tag, buffer.format.channels,
                     buffer.format.rate, buffer.format.bits, buffer.format.block_align, buffer.mix_bins.size());
        for (const MixBinPair& pair : buffer.mix_bins)
        {
            std::fprintf(stderr, " {%u, %d}", pair.bin, pair.volume);
        }
        std::fprintf(stderr, "\n");
    }
    state.buffers.emplace(object, buffer);
    memory.Write32(out, object);
    call.Return(kDsOk, 4);
}

}  // namespace

void RegisterDeviceExports(runtime::HleRegistry& registry)
{
    registry.AddExport("dsound", "DirectSoundCreate", &DirectSoundCreate);
    registry.AddExport("dsound", "IDirectSound_AddRef", &IDirectSound_AddRef);
    registry.AddExport("dsound", "IDirectSound_Release", &IDirectSound_Release);
    registry.AddExport("dsound", "IDirectSound_GetCaps", &IDirectSound_GetCaps);
    registry.AddExport("dsound", "IDirectSound_GetSpeakerConfig", &IDirectSound_GetSpeakerConfig);
    registry.AddExport("dsound", "IDirectSound_CreateSoundBuffer", &IDirectSound_CreateSoundBuffer);
    // Deferred 3D settings and the library's periodic work: nothing to do.
    registry.AddExport("dsound", "IDirectSound_CommitDeferredSettings", &AcceptSetting<1>);
    registry.AddExport("dsound", "DirectSoundDoWork", &AcceptSetting<0>);
    // Listener settings (design section 7).
    registry.AddExport("dsound", "IDirectSound_SetAllParameters", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSound_SetDistanceFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSound_SetDopplerFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSound_SetOrientation", &AcceptSetting<8>);
    registry.AddExport("dsound", "IDirectSound_SetPosition", &AcceptSetting<5>);
    registry.AddExport("dsound", "IDirectSound_SetRolloffFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSound_SetVelocity", &AcceptSetting<5>);
    registry.AddExport("dsound", "IDirectSound_SetI3DL2Listener", &AcceptSetting<3>);
}

}  // namespace rexbox::hle::dsound
