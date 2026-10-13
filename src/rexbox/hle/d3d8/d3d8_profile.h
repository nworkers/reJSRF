// REXBOX_D3D8_PROFILE=<n>: where the frame time goes, printed every n
// presents as milliseconds per frame (diagnostic). Phases are timed with a
// steady clock on the presenting thread only (other threads, such as a
// title's vblank thread, are not counted); the remainder of a frame is the
// title's own code and the HLE calls that neither draw nor wait. "cpu" is
// the processor time the presenting thread used per frame, which other
// processes on the machine disturb less than the wall time.
//
// REXBOX_SAMPLE=<first>-<last>[:<file>]: samples the presenting thread's
// instruction pointer from present <first> to present <last> and writes the
// histogram (platform/sampler.h) to <file>, default rexbox_samples.txt.

#ifndef REXBOX_HLE_D3D8_D3D8_PROFILE_H
#define REXBOX_HLE_D3D8_D3D8_PROFILE_H

#include <chrono>
#include <cstdint>

namespace rexbox::hle::d3d8::profile
{

enum class Phase : uint32_t
{
    kIndices,   // reading, converting and uploading indices
    kSetup,     // vertex layout or program lookup, texture resolution, cache keys
    kTarget,    // render-target binding
    kUpload,    // vertex data: comparing with the cached copy, uploading
    kAttributes,  // vertex attribute pointers
    kConstants,   // program and uniforms
    kTextures,    // texture and sampler binding
    kState,     // fixed per-fragment state, viewport
    kDraw,      // the GL draw call
    kPresent,   // Swap: frame dump, present, events
    kWait,      // waiting for a vertical blank (present interval, BlockUntilVerticalBlank)
    kCount,
};

bool Enabled();
void Add(Phase phase, std::chrono::steady_clock::duration elapsed);
void CountDraw(bool program);
// Bytes sent to GL buffers.
void CountUpload(size_t vertex_bytes, size_t index_bytes);
// Called once per present with the present's number; prints a report every
// n presents and starts or stops sampling.
void EndFrame(uint32_t frame);

// Charges the time since the previous mark (or construction) to a phase.
class Lap
{
public:
    Lap() : enabled_(Enabled())
    {
        if (enabled_)
        {
            last_ = std::chrono::steady_clock::now();
        }
    }
    void Mark(Phase phase)
    {
        if (enabled_)
        {
            const auto now = std::chrono::steady_clock::now();
            Add(phase, now - last_);
            last_ = now;
        }
    }

private:
    bool enabled_;
    std::chrono::steady_clock::time_point last_;
};

class Scope
{
public:
    explicit Scope(Phase phase) : phase_(phase), enabled_(Enabled())
    {
        if (enabled_)
        {
            start_ = std::chrono::steady_clock::now();
        }
    }
    ~Scope()
    {
        if (enabled_)
        {
            Add(phase_, std::chrono::steady_clock::now() - start_);
        }
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Phase phase_;
    bool enabled_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace rexbox::hle::d3d8::profile

#endif  // REXBOX_HLE_D3D8_D3D8_PROFILE_H
