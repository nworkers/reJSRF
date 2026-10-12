// Internal to the DSOUND HLE module: DirectSound constants and structure
// layouts, the voice model, the module state and the per-area registration
// functions.
//
// The APU (voice processor, GP/EP DSPs) that the original library drives is
// not emulated; these exports implement the API above it on a host mixer
// (docs/design/20261012-i017-dsound-hle.md). Guest structures are addressed by
// offset (docs/kb/xbox-dsound.md).

#ifndef REXBOX_HLE_DSOUND_DSOUND_INTERNAL_H
#define REXBOX_HLE_DSOUND_DSOUND_INTERNAL_H

#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_call.h"
#include "rexbox/runtime/hle_registry.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::dsound
{

using runtime::GuestAddress;
using runtime::HleCall;

// HRESULTs.
constexpr uint32_t kDsOk = 0;
constexpr uint32_t kErrFail = 0x80004005;         // E_FAIL
constexpr uint32_t kErrOutOfMemory = 0x8007000E;  // E_OUTOFMEMORY
constexpr uint32_t kErrInvalidCall = 0x88780032;  // DSERR_INVALIDCALL

// DSBUFFERDESC.dwFlags (DSBCAPS_*).
constexpr uint32_t kCaps3d = 0x00000010;     // DSBCAPS_CTRL3D (assumed)
constexpr uint32_t kCapsMixIn = 0x00002000;  // DSBCAPS_MIXIN: a submix destination, no data
constexpr uint32_t kCapsFxIn = 0x00080000;   // DSBCAPS_FXIN: fed by the effects DSP, no data

// IDirectSoundBuffer_Play flags (DSBPLAY_*).
constexpr uint32_t kPlayLooping = 0x1;
constexpr uint32_t kPlayFromStart = 0x2;

// IDirectSoundBuffer_Lock flags (DSBLOCK_*).
constexpr uint32_t kLockFromWriteCursor = 0x1;
constexpr uint32_t kLockEntireBuffer = 0x2;

// IDirectSoundBuffer_GetStatus bits (DSBSTATUS_*).
constexpr uint32_t kStatusPlaying = 0x1;
constexpr uint32_t kStatusLooping = 0x4;

// WAVEFORMATEX.wFormatTag.
constexpr uint16_t kFormatPcm = 0x0001;
constexpr uint16_t kFormatXboxAdpcm = 0x0069;

// Xbox ADPCM: per channel a 36-byte block (4-byte header, 32 bytes of
// nibbles) played as 64 samples (design section 5).
constexpr uint32_t kAdpcmBlockBytes = 36;
constexpr uint32_t kAdpcmSamplesPerBlock = 64;

constexpr uint32_t kMaxChannels = 6;
constexpr uint32_t kObjectBlockSize = 0x40;  // guest block behind each interface pointer

// The write cursor runs at least this many frames ahead of the play cursor
// (the original adds the byte size of 32 samples).
constexpr uint32_t kWriteAheadFrames = 32;

// DSMIXBIN_* speaker bins; 10 (I3DL2) and up feed the effects DSP.
constexpr uint32_t kMixBinCount = 31;

namespace layout
{
// DSBUFFERDESC (confirmed from the game's buffer-creation code).
constexpr uint32_t kDescSize = 0x00;
constexpr uint32_t kDescFlags = 0x04;
constexpr uint32_t kDescBufferBytes = 0x08;
constexpr uint32_t kDescFormat = 0x0C;
constexpr uint32_t kDescMixBins = 0x10;
constexpr uint32_t kDescInputMixBin = 0x14;

// WAVEFORMATEX.
constexpr uint32_t kWaveFormatTag = 0x00;
constexpr uint32_t kWaveChannels = 0x02;
constexpr uint32_t kWaveSamplesPerSec = 0x04;
constexpr uint32_t kWaveBlockAlign = 0x0C;
constexpr uint32_t kWaveBitsPerSample = 0x0E;

// DSMIXBINS: count, then a pointer to {DWORD dwMixBin; LONG lVolume} pairs.
constexpr uint32_t kMixBinsCount = 0x00;
constexpr uint32_t kMixBinsPairs = 0x04;
constexpr uint32_t kMixBinPairSize = 0x08;

// DSCAPS: four DWORDs.
constexpr uint32_t kCapsFree2dBuffers = 0x00;
constexpr uint32_t kCapsFree3dBuffers = 0x04;
constexpr uint32_t kCapsFreeBufferSges = 0x08;
constexpr uint32_t kCapsMemoryAllocated = 0x0C;

// DSEFFECTIMAGEDESC: count, total scratch size, then 0x20-byte maps.
constexpr uint32_t kImageDescCount = 0x00;
constexpr uint32_t kImageDescScratchSize = 0x04;
constexpr uint32_t kImageDescMaps = 0x08;
constexpr uint32_t kEffectMapSize = 0x20;
constexpr uint32_t kMapCode = 0x00;
constexpr uint32_t kMapCodeSize = 0x04;
constexpr uint32_t kMapState = 0x08;
constexpr uint32_t kMapStateSize = 0x0C;
constexpr uint32_t kMapYMemory = 0x10;
constexpr uint32_t kMapYMemorySize = 0x14;
constexpr uint32_t kMapScratch = 0x18;
constexpr uint32_t kMapScratchSize = 0x1C;

// Effects image file: a header of six DWORDs at 0x800, the body from 0x818
// (confirmed from the original download code).
constexpr uint32_t kImageHeader = 0x800;
constexpr uint32_t kImageCodeWords = 0x804;
constexpr uint32_t kImageStateWords = 0x80C;
constexpr uint32_t kImageBody = 0x818;
}  // namespace layout

// Totals GetCaps reports against (assumed: the APU's 256 voices, 64 of them
// 3D, and 2047 scatter-gather entries of one page each).
constexpr uint32_t kHardwareVoices = 256;
constexpr uint32_t kHardware3dVoices = 64;
constexpr uint32_t kHardwareSges = 2047;

struct MixBinPair
{
    uint32_t bin = 0;
    int32_t volume = 0;  // hundredths of a dB
};

struct WaveFormat
{
    uint16_t tag = 0;
    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t block_align = 0;
    uint16_t bits = 0;
};

// One IDirectSoundBuffer and the voice that plays it.
struct Buffer
{
    uint32_t refs = 1;
    uint32_t flags = 0;
    WaveFormat format;
    bool valid_format = false;  // the mixer can decode it

    GuestAddress data = 0;
    uint32_t bytes = 0;
    bool owns_data = false;  // allocated by CreateSoundBuffer, not SetBufferData

    bool playing = false;
    bool looping = false;
    double position = 0.0;  // play position in frames while playing
    uint32_t saved_bytes = 0;  // the position while stopped
    uint32_t loop_start = 0;  // bytes
    uint32_t loop_length = 0;  // bytes; 0 means to the end

    uint32_t frequency = 0;  // Hz; 0 means the format's rate
    int32_t volume = 0;  // hundredths of a dB
    uint32_t headroom = 0;  // recorded only
    bool has_mix_bins = false;
    std::vector<MixBinPair> mix_bins;
    float gain_left[kMaxChannels] = {};
    float gain_right[kMaxChannels] = {};

    // The last decoded ADPCM block.
    int64_t decoded_block = -1;
    int16_t decoded[kAdpcmSamplesPerBlock * kMaxChannels] = {};
};

struct EffectsImage
{
    GuestAddress copy = 0;  // the downloaded image
    GuestAddress descriptor = 0;
    GuestAddress y_memory = 0;
    GuestAddress scratch = 0;
    uint32_t effect_count = 0;
};

struct DsoundState
{
    std::mutex mutex;  // guards everything below; the mixer takes it per render
    DsoundOutput output = DsoundOutput::kHost;
    bool output_started = false;

    GuestAddress device = 0;  // the single IDirectSound
    uint32_t device_refs = 0;
    std::unordered_map<GuestAddress, Buffer> buffers;
    uint32_t allocated_bytes = 0;  // buffer memory the HLE allocated
    EffectsImage effects;
    bool trace = false;  // REXBOX_DSOUND_TRACE
};

DsoundState& State();
runtime::GuestMemory& Memory();
runtime::GuestHeap& Heap();

// Allocates a zeroed guest block; 0 on failure.
GuestAddress AllocateGuest(uint32_t size);

// The buffer behind an interface pointer, or null. Caller holds the mutex.
Buffer* FindBuffer(GuestAddress pointer);

// Mixer (dsound_mixer.cpp). Callers hold the mutex.
void StartOutputLocked();
uint32_t TotalFrames(const Buffer& buffer);
uint32_t BytesFromFrames(const Buffer& buffer, uint32_t frames);
uint32_t FramesFromBytes(const Buffer& buffer, uint32_t bytes);
// Re-derives the per-channel gains from volume and mix bins.
void UpdateGains(Buffer& buffer);
// Reads a DSMIXBINS into the buffer (dsound_buffer.cpp).
void ReadMixBins(Buffer& buffer, GuestAddress mix_bins);
// Mixes `frames` stereo frames into `samples` (added) and advances cursors.
void MixLocked(float* samples, uint32_t frames);

void RegisterDeviceExports(runtime::HleRegistry& registry);
void RegisterBufferExports(runtime::HleRegistry& registry);
void RegisterEffectsExports(runtime::HleRegistry& registry);

// Accepts a setting the HLE records nothing for and does not render (3D,
// envelopes, filters: design section 7). Pops kStackArgs DWORDs.
template <int kStackArgs>
void AcceptSetting()
{
    HleCall call;
    call.Return(kDsOk, kStackArgs);
}

}  // namespace rexbox::hle::dsound

#endif  // REXBOX_HLE_DSOUND_DSOUND_INTERNAL_H
