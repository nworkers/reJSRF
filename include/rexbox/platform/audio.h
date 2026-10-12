// Platform contract: one host audio output, as plain values.
//
// The output is interleaved stereo float at a fixed rate. It pulls samples:
// whenever it needs more it calls the render function, on its own thread.
// Without a usable host device (or when asked for none) a pacing thread calls
// the same function at real-time speed and discards the samples, so a caller
// that keeps time by what it rendered sees time pass either way. The host
// backend lives in src/platform/<library>/.

#ifndef REXBOX_PLATFORM_AUDIO_H
#define REXBOX_PLATFORM_AUDIO_H

#include <cstdint>
#include <string>

namespace rexbox::platform
{

constexpr uint32_t kAudioChannels = 2;

// Fills `frames` interleaved stereo frames (2 floats each, nominally -1..1).
using AudioRenderFunction = void (*)(float* samples, uint32_t frames, void* context);

struct AudioOutputConfig
{
    uint32_t sample_rate = 48000;
    bool null_device = false;  // never open a host device; pace only
};

struct AudioOutputInfo
{
    bool host_device = false;  // samples reach a host device
    uint32_t sample_rate = 0;
    std::string reason;  // why there is no host device, when there is none
};

// Starts the output; `render` is called until StopAudioOutput. Only one
// output exists; a second start fails. Returns false (with a reason in
// `info`) only when not even the pacing thread could start.
bool StartAudioOutput(const AudioOutputConfig& config, AudioRenderFunction render, void* context,
                      AudioOutputInfo* info);

// Stops the output; `render` is not called after this returns.
void StopAudioOutput();

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_AUDIO_H
