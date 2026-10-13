#include "d3d8_profile.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace rexbox::hle::d3d8::profile
{

namespace
{

using Clock = std::chrono::steady_clock;

uint32_t Interval()
{
    static const uint32_t interval = [] {
        const char* value = std::getenv("REXBOX_D3D8_PROFILE");
        return value != nullptr ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 0u;
    }();
    return interval;
}

// Only the drawing thread touches these.
std::array<Clock::duration, static_cast<size_t>(Phase::kCount)> g_phases = {};
uint64_t g_draws = 0;
uint64_t g_program_draws = 0;
uint64_t g_vertex_bytes = 0;
uint64_t g_index_bytes = 0;
uint32_t g_frames = 0;
Clock::time_point g_window_start;
bool g_started = false;

double Milliseconds(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

}  // namespace

bool Enabled()
{
    return Interval() != 0;
}

void Add(Phase phase, Clock::duration elapsed)
{
    g_phases[static_cast<size_t>(phase)] += elapsed;
}

void CountDraw(bool program)
{
    if (!Enabled())
    {
        return;
    }
    ++g_draws;
    g_program_draws += program ? 1 : 0;
}

void CountUpload(size_t vertex_bytes, size_t index_bytes)
{
    if (!Enabled())
    {
        return;
    }
    g_vertex_bytes += vertex_bytes;
    g_index_bytes += index_bytes;
}

void EndFrame()
{
    const uint32_t interval = Interval();
    if (interval == 0)
    {
        return;
    }
    const Clock::time_point now = Clock::now();
    if (!g_started)
    {
        g_started = true;
        g_window_start = now;
        g_phases = {};
        g_draws = g_program_draws = 0;
        g_frames = 0;
        return;
    }
    if (++g_frames < interval)
    {
        return;
    }
    const double frames = static_cast<double>(g_frames);
    const double wall = Milliseconds(now - g_window_start) / frames;
    double measured = 0.0;
    for (const Clock::duration d : g_phases)
    {
        measured += Milliseconds(d);
    }
    measured /= frames;
    const auto per_frame = [frames](Phase phase) {
        return Milliseconds(g_phases[static_cast<size_t>(phase)]) / frames;
    };
    std::fprintf(stderr,
                 "[d3d8 profile] %.1f fps, %.2f ms/frame: draws %.0f (%.0f program), uploads %.0f KiB vertices %.0f KiB indices | indices %.2f setup %.2f "
                 "target %.2f upload %.2f attributes %.2f constants %.2f textures %.2f state %.2f draw %.2f present %.2f | "
                 "other %.2f\n",
                 1000.0 / wall, wall, static_cast<double>(g_draws) / frames,
                 static_cast<double>(g_program_draws) / frames, static_cast<double>(g_vertex_bytes) / frames / 1024.0,
                 static_cast<double>(g_index_bytes) / frames / 1024.0, per_frame(Phase::kIndices), per_frame(Phase::kSetup),
                 per_frame(Phase::kTarget), per_frame(Phase::kUpload), per_frame(Phase::kAttributes),
                 per_frame(Phase::kConstants), per_frame(Phase::kTextures), per_frame(Phase::kState),
                 per_frame(Phase::kDraw), per_frame(Phase::kPresent), wall - measured);
    g_window_start = now;
    g_phases = {};
    g_draws = g_program_draws = 0;
    g_vertex_bytes = g_index_bytes = 0;
    g_frames = 0;
}

}  // namespace rexbox::hle::d3d8::profile
