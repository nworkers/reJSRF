// IDirectSoundBuffer: lifetime, data, Lock, Play/Stop, status and cursors,
// volume, frequency, loop region and mix bins. Cursor semantics follow the
// original CDirectSoundBuffer/CMcpxBuffer code (design section 4).

#include "dsound_internal.h"

#include <algorithm>

namespace rexbox::hle::dsound
{

namespace
{

// The play cursor in bytes: live while playing, saved while stopped.
uint32_t PlayCursor(const Buffer& buffer)
{
    if (!buffer.playing)
    {
        return buffer.saved_bytes;
    }
    return BytesFromFrames(buffer, static_cast<uint32_t>(buffer.position));
}

// The write cursor: play cursor plus 32 samples, folded into the loop region
// (as the original folds it: start + ahead % length) or the buffer.
uint32_t WriteCursor(const Buffer& buffer)
{
    const uint32_t play = PlayCursor(buffer);
    if (!buffer.playing)
    {
        return play;
    }
    const uint32_t ahead = std::max<uint32_t>(BytesFromFrames(buffer, kWriteAheadFrames), buffer.format.block_align);
    const uint32_t write = play + ahead;
    const uint32_t loop_start = std::min(buffer.loop_start, buffer.bytes);
    const uint32_t loop_length = buffer.loop_length != 0 ? buffer.loop_length : buffer.bytes - loop_start;
    if (buffer.looping && loop_length != 0 && play >= loop_start && play - loop_start < loop_length)
    {
        return loop_start + write % loop_length;
    }
    return buffer.bytes == 0 ? 0 : write % buffer.bytes;
}

void ReleaseBufferLocked(DsoundState& state, GuestAddress pointer, Buffer& buffer)
{
    if (buffer.owns_data)
    {
        Heap().Free(buffer.data);
        state.allocated_bytes -= std::min(state.allocated_bytes, buffer.bytes);
    }
    state.buffers.erase(pointer);
    Heap().Free(pointer);
}

// ULONG IDirectSoundBuffer_AddRef(LPDIRECTSOUNDBUFFER pThis)
void IDirectSoundBuffer_AddRef()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    call.Return(buffer == nullptr ? 0 : ++buffer->refs, 1);
}

// ULONG IDirectSoundBuffer_Release(LPDIRECTSOUNDBUFFER pThis)
void IDirectSoundBuffer_Release()
{
    HleCall call;
    const GuestAddress pointer = call.Arg(0);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(pointer);
    if (buffer == nullptr || buffer->refs == 0)
    {
        call.Return(0, 1);
        return;
    }
    const uint32_t refs = --buffer->refs;
    if (refs == 0)
    {
        ReleaseBufferLocked(state, pointer, *buffer);
    }
    call.Return(refs, 1);
}

// HRESULT IDirectSoundBuffer_SetBufferData(LPDIRECTSOUNDBUFFER pThis,
//     LPVOID pvBufferData, DWORD dwBufferBytes)
// The buffer plays the caller's memory from now on; the saved position is 0.
void IDirectSoundBuffer_SetBufferData()
{
    HleCall call;
    const GuestAddress data = call.Arg(1);
    const uint32_t bytes = call.Arg(2);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 3);
        return;
    }
    if (data != 0 && bytes != 0 && !Memory().IsValid(data, bytes))
    {
        std::fprintf(stderr, "[dsound] SetBufferData(%08X, %u): not guest memory\n", data, bytes);
        call.Return(kErrInvalidCall, 3);
        return;
    }
    if (buffer->owns_data)
    {
        Heap().Free(buffer->data);
        state.allocated_bytes -= std::min(state.allocated_bytes, buffer->bytes);
        buffer->owns_data = false;
    }
    buffer->data = bytes != 0 ? data : 0;
    buffer->bytes = data != 0 ? bytes : 0;
    buffer->saved_bytes = 0;
    buffer->decoded_block = -1;
    if (buffer->playing)
    {
        buffer->position = std::min<double>(buffer->position, TotalFrames(*buffer));
    }
    call.Return(kDsOk, 3);
}

// HRESULT IDirectSoundBuffer_Lock(LPDIRECTSOUNDBUFFER pThis, DWORD dwOffset,
//     DWORD dwBytes, LPVOID* ppvAudioPtr1, LPDWORD pdwAudioBytes1,
//     LPVOID* ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags)
// The original checks neither first-part pointer; the HLE skips a null one.
void IDirectSoundBuffer_Lock()
{
    HleCall call;
    uint32_t offset = call.Arg(1);
    uint32_t bytes = call.Arg(2);
    const GuestAddress ptr1 = call.Arg(3);
    const GuestAddress bytes1 = call.Arg(4);
    const GuestAddress ptr2 = call.Arg(5);
    const GuestAddress bytes2 = call.Arg(6);
    const uint32_t flags = call.Arg(7);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 8);
        return;
    }
    if ((flags & kLockFromWriteCursor) != 0)
    {
        offset = WriteCursor(*buffer);
    }
    if ((flags & kLockEntireBuffer) != 0)
    {
        bytes = buffer->bytes;
    }
    offset = std::min(offset, buffer->bytes);
    const uint32_t first = std::min(bytes, buffer->bytes - offset);
    runtime::GuestMemory& memory = Memory();
    if (ptr1 != 0)
    {
        memory.Write32(ptr1, buffer->data + offset);
    }
    if (bytes1 != 0)
    {
        memory.Write32(bytes1, first);
    }
    if (ptr2 != 0 && bytes2 != 0)
    {
        const bool wraps = first < bytes;
        memory.Write32(ptr2, wraps ? buffer->data : 0);
        memory.Write32(bytes2, wraps ? std::min(bytes - first, buffer->bytes) : 0);
    }
    call.Return(kDsOk, 8);
}

// HRESULT IDirectSoundBuffer_Unlock(LPDIRECTSOUNDBUFFER pThis, LPVOID pvLock1,
//     DWORD dwLockSize1, LPVOID pvLock2, DWORD dwLockSize2)
// The original does nothing: the mixer reads guest memory as it plays.
void IDirectSoundBuffer_Unlock()
{
    HleCall call;
    call.Return(kDsOk, 5);
}

// HRESULT IDirectSoundBuffer_Play(LPDIRECTSOUNDBUFFER pThis, DWORD dwReserved1,
//     DWORD dwReserved2, DWORD dwFlags)
// Starts at 0 with DSBPLAY_FROMSTART, else at the saved position, which then
// becomes 0. Playing again only changes looping (whether the original
// restarts a playing voice is open: docs/analysis/dsound.md).
void IDirectSoundBuffer_Play()
{
    HleCall call;
    const uint32_t flags = call.Arg(3);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 4);
        return;
    }
    buffer->looping = (flags & kPlayLooping) != 0;
    if (!buffer->playing)
    {
        const uint32_t start = (flags & kPlayFromStart) != 0 ? 0 : buffer->saved_bytes;
        buffer->saved_bytes = 0;
        buffer->position = static_cast<double>(std::min(FramesFromBytes(*buffer, start), TotalFrames(*buffer)));
        buffer->playing = true;
    }
    else if ((flags & kPlayFromStart) != 0)
    {
        buffer->position = 0.0;
    }
    if (state.trace)
    {
        std::fprintf(stderr, "[dsound] Play %08X flags %X at frame %.0f\n", call.Arg(0), flags, buffer->position);
    }
    call.Return(kDsOk, 4);
}

// HRESULT IDirectSoundBuffer_Stop(LPDIRECTSOUNDBUFFER pThis)
// Saves the play position (not for submix buffers) and stops.
void IDirectSoundBuffer_Stop()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 1);
        return;
    }
    if (buffer->playing)
    {
        if ((buffer->flags & (kCapsMixIn | kCapsFxIn)) == 0)
        {
            buffer->saved_bytes = PlayCursor(*buffer);
        }
        buffer->playing = false;
    }
    call.Return(kDsOk, 1);
}

// HRESULT IDirectSoundBuffer_GetStatus(LPDIRECTSOUNDBUFFER pThis, LPDWORD pdwStatus)
void IDirectSoundBuffer_GetStatus()
{
    HleCall call;
    const GuestAddress out = call.Arg(1);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr || out == 0)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    uint32_t status = 0;
    if (buffer->playing)
    {
        status = kStatusPlaying | (buffer->looping ? kStatusLooping : 0);
    }
    Memory().Write32(out, status);
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSoundBuffer_GetCurrentPosition(LPDIRECTSOUNDBUFFER pThis,
//     LPDWORD pdwPlayCursor, LPDWORD pdwWriteCursor)
void IDirectSoundBuffer_GetCurrentPosition()
{
    HleCall call;
    const GuestAddress play = call.Arg(1);
    const GuestAddress write = call.Arg(2);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 3);
        return;
    }
    if (play != 0)
    {
        Memory().Write32(play, PlayCursor(*buffer));
    }
    if (write != 0)
    {
        Memory().Write32(write, WriteCursor(*buffer));
    }
    call.Return(kDsOk, 3);
}

// HRESULT IDirectSoundBuffer_SetCurrentPosition(LPDIRECTSOUNDBUFFER pThis,
//     DWORD dwPlayCursor)
void IDirectSoundBuffer_SetCurrentPosition()
{
    HleCall call;
    const uint32_t bytes = call.Arg(1);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    const uint32_t clamped = std::min(bytes, buffer->bytes);
    if (buffer->playing)
    {
        buffer->position = static_cast<double>(FramesFromBytes(*buffer, clamped));
    }
    else
    {
        buffer->saved_bytes = clamped;
    }
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSoundBuffer_SetLoopRegion(LPDIRECTSOUNDBUFFER pThis,
//     DWORD dwLoopStart, DWORD dwLoopLength)
void IDirectSoundBuffer_SetLoopRegion()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 3);
        return;
    }
    buffer->loop_start = call.Arg(1);
    buffer->loop_length = call.Arg(2);
    call.Return(kDsOk, 3);
}

// HRESULT IDirectSoundBuffer_SetVolume(LPDIRECTSOUNDBUFFER pThis, LONG lVolume)
void IDirectSoundBuffer_SetVolume()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    buffer->volume = static_cast<int32_t>(call.Arg(1));
    UpdateGains(*buffer);
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSoundBuffer_SetFrequency(LPDIRECTSOUNDBUFFER pThis, DWORD dwFrequency)
void IDirectSoundBuffer_SetFrequency()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    buffer->frequency = call.Arg(1);
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSoundBuffer_SetHeadroom(LPDIRECTSOUNDBUFFER pThis, DWORD dwHeadroom)
// Recorded only (design section 5).
void IDirectSoundBuffer_SetHeadroom()
{
    HleCall call;
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    buffer->headroom = call.Arg(1);
    call.Return(kDsOk, 2);
}

// HRESULT IDirectSoundBuffer_SetMixBins(LPDIRECTSOUNDBUFFER pThis, LPCDSMIXBINS pMixBins)
void IDirectSoundBuffer_SetMixBins()
{
    HleCall call;
    const GuestAddress mix_bins = call.Arg(1);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    Buffer* buffer = FindBuffer(call.Arg(0));
    if (buffer == nullptr)
    {
        call.Return(kErrInvalidCall, 2);
        return;
    }
    buffer->has_mix_bins = false;
    buffer->mix_bins.clear();
    if (mix_bins != 0)
    {
        ReadMixBins(*buffer, mix_bins);
    }
    UpdateGains(*buffer);
    call.Return(kDsOk, 2);
}

}  // namespace

void ReadMixBins(Buffer& buffer, GuestAddress mix_bins)
{
    runtime::GuestMemory& memory = Memory();
    const uint32_t count = std::min(memory.Read32(mix_bins + layout::kMixBinsCount), kMixBinCount);
    const GuestAddress pairs = memory.Read32(mix_bins + layout::kMixBinsPairs);
    buffer.mix_bins.clear();
    if (count == 0 || pairs == 0)
    {
        buffer.has_mix_bins = false;
        return;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        MixBinPair pair;
        pair.bin = memory.Read32(pairs + i * layout::kMixBinPairSize);
        pair.volume = static_cast<int32_t>(memory.Read32(pairs + i * layout::kMixBinPairSize + 4));
        buffer.mix_bins.push_back(pair);
    }
    buffer.has_mix_bins = true;
}

void RegisterBufferExports(runtime::HleRegistry& registry)
{
    registry.AddExport("dsound", "IDirectSoundBuffer_AddRef", &IDirectSoundBuffer_AddRef);
    registry.AddExport("dsound", "IDirectSoundBuffer_Release", &IDirectSoundBuffer_Release);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetBufferData", &IDirectSoundBuffer_SetBufferData);
    registry.AddExport("dsound", "IDirectSoundBuffer_Lock", &IDirectSoundBuffer_Lock);
    registry.AddExport("dsound", "IDirectSoundBuffer_Unlock", &IDirectSoundBuffer_Unlock);
    registry.AddExport("dsound", "IDirectSoundBuffer_Play", &IDirectSoundBuffer_Play);
    registry.AddExport("dsound", "IDirectSoundBuffer_Stop", &IDirectSoundBuffer_Stop);
    registry.AddExport("dsound", "IDirectSoundBuffer_GetStatus", &IDirectSoundBuffer_GetStatus);
    registry.AddExport("dsound", "IDirectSoundBuffer_GetCurrentPosition", &IDirectSoundBuffer_GetCurrentPosition);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetCurrentPosition", &IDirectSoundBuffer_SetCurrentPosition);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetLoopRegion", &IDirectSoundBuffer_SetLoopRegion);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetVolume", &IDirectSoundBuffer_SetVolume);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetFrequency", &IDirectSoundBuffer_SetFrequency);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetHeadroom", &IDirectSoundBuffer_SetHeadroom);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetMixBins", &IDirectSoundBuffer_SetMixBins);
    // Envelopes, LFOs, filters and 3D (design section 7).
    registry.AddExport("dsound", "IDirectSoundBuffer_SetEG", &AcceptSetting<2>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetLFO", &AcceptSetting<2>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetFilter", &AcceptSetting<2>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetAllParameters", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetConeAngles", &AcceptSetting<4>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetConeOrientation", &AcceptSetting<5>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetConeOutsideVolume", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetMaxDistance", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetMinDistance", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetMode", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetPosition", &AcceptSetting<5>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetVelocity", &AcceptSetting<5>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetDistanceFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetDopplerFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetRolloffFactor", &AcceptSetting<3>);
    registry.AddExport("dsound", "IDirectSoundBuffer_SetI3DL2Source", &AcceptSetting<3>);
}

}  // namespace rexbox::hle::dsound
