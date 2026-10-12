#include "rexbox/hle/dsound/dsound_hle.h"

#include "dsound_internal.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace rexbox::hle::dsound
{

DsoundState& State()
{
    static DsoundState state;
    return state;
}

runtime::GuestMemory& Memory()
{
    return *runtime::Env().memory;
}

runtime::GuestHeap& Heap()
{
    return *runtime::Env().heap;
}

GuestAddress AllocateGuest(uint32_t size)
{
    const auto block = Heap().Allocate(size == 0 ? 1 : size, runtime::GuestHeap::kPageSize, false);
    if (!block)
    {
        return 0;
    }
    Memory().Fill(*block, 0, size);
    return *block;
}

Buffer* FindBuffer(GuestAddress pointer)
{
    auto& buffers = State().buffers;
    const auto it = buffers.find(pointer);
    return it == buffers.end() ? nullptr : &it->second;
}

void RegisterDsoundHle(runtime::HleRegistry& registry)
{
    DsoundState& state = State();
    // REXBOX_AUDIO=none runs without a host audio device (diagnostics, CI);
    // cursors still advance in real time.
    const char* audio = std::getenv("REXBOX_AUDIO");
    if (audio != nullptr && std::strcmp(audio, "none") == 0 && state.output == DsoundOutput::kHost)
    {
        state.output = DsoundOutput::kNull;
    }
    const char* trace = std::getenv("REXBOX_DSOUND_TRACE");
    state.trace = trace != nullptr && trace[0] != '\0' && std::strcmp(trace, "0") != 0;
    RegisterDeviceExports(registry);
    RegisterBufferExports(registry);
    RegisterEffectsExports(registry);
}

void SetDsoundOutput(DsoundOutput output)
{
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.output = output;
}

void RenderDsoundFrames(float* samples, uint32_t frames)
{
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (samples != nullptr)
    {
        std::memset(samples, 0, sizeof(float) * 2 * frames);
        MixLocked(samples, frames);
        return;
    }
    std::vector<float> scratch(static_cast<size_t>(frames) * 2, 0.0f);
    MixLocked(scratch.data(), frames);
}

}  // namespace rexbox::hle::dsound
