#include "d3d8_profile.h"

#include "d3d8_gl.h"
#include "d3d8_trace.h"

#include "rexbox/platform/host_thread.h"
#include "rexbox/platform/sampler.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

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

// Only the presenting thread counts: the first EndFrame marks it, and other
// threads (a title's vblank thread, for one) are ignored.
thread_local bool t_presenting = false;
std::array<Clock::duration, static_cast<size_t>(Phase::kCount)> g_phases = {};
uint64_t g_draws = 0;
uint64_t g_program_draws = 0;
uint64_t g_vertex_bytes = 0;
uint64_t g_index_bytes = 0;
uint64_t g_compares = 0;
uint32_t g_programs_built = 0;
// Unimplemented report sites hit in the window (few, so a linear search).
struct MissingCount
{
    const char* site;
    uint64_t count;
};
std::array<MissingCount, 32> g_missing = {};
size_t g_missing_sites = 0;
uint32_t g_programs_cached = 0;
Clock::duration g_program_time{};
uint32_t g_programs_stored = 0;
Clock::duration g_store_time{};
uint64_t g_compare_bytes = 0;
uint32_t g_frames = 0;
Clock::time_point g_window_start;
Clock::time_point g_last_present;  // the previous EndFrame
Clock::duration g_worst_frame{};    // the longest present-to-present time in the window
uint32_t g_slow_frames = 0;         // frames longer than two vertical blanks (33.4 ms)
uint64_t g_window_cpu_ns = 0;
bool g_started = false;

struct SampleWindow
{
    uint32_t first = 0;
    uint32_t last = 0;
    std::string path = "rexbox_samples.txt";
};

const SampleWindow& Sampling()
{
    static const SampleWindow window = [] {
        SampleWindow w;
        const char* value = std::getenv("REXBOX_SAMPLE");
        if (value == nullptr)
        {
            return w;
        }
        char* end = nullptr;
        w.first = static_cast<uint32_t>(std::strtoul(value, &end, 10));
        if (end != nullptr && *end == '-')
        {
            w.last = static_cast<uint32_t>(std::strtoul(end + 1, &end, 10));
        }
        if (end != nullptr && *end == ':' && end[1] != '\0')
        {
            w.path = end + 1;
        }
        if (w.last <= w.first)
        {
            w.first = w.last = 0;
        }
        return w;
    }();
    return window;
}

void SampleAt(uint32_t frame)
{
    const SampleWindow& window = Sampling();
    if (window.last == 0)
    {
        return;
    }
    if (frame == window.first)
    {
        const bool started = platform::StartThreadSampling();
        std::fprintf(stderr, "[d3d8 profile] sampling presents %u-%u%s\n", window.first, window.last,
                     started ? "" : ": no sampler on this platform");
    }
    else if (frame == window.last)
    {
        const uint64_t samples = platform::StopThreadSampling(window.path.c_str());
        std::fprintf(stderr, "[d3d8 profile] %llu samples written to %s\n", static_cast<unsigned long long>(samples),
                     window.path.c_str());
    }
}

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
    if (!t_presenting)
    {
        return;
    }
    g_phases[static_cast<size_t>(phase)] += elapsed;
}

void CountDraw(bool program)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    ++g_draws;
    g_program_draws += program ? 1 : 0;
}

void CountUpload(size_t vertex_bytes, size_t index_bytes)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    g_vertex_bytes += vertex_bytes;
    g_index_bytes += index_bytes;
}

void CountMissing(const char* site)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    for (size_t i = 0; i < g_missing_sites; ++i)
    {
        if (g_missing[i].site == site)
        {
            ++g_missing[i].count;
            return;
        }
    }
    if (g_missing_sites < g_missing.size())
    {
        g_missing[g_missing_sites++] = {site, 1};
    }
}

void CountProgram(bool cached, Clock::duration elapsed)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    ++(cached ? g_programs_cached : g_programs_built);
    g_program_time += elapsed;
}

void CountProgramStore(Clock::duration elapsed)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    ++g_programs_stored;
    g_store_time += elapsed;
}

void CountCompare(size_t bytes)
{
    if (!Enabled() || !t_presenting)
    {
        return;
    }
    ++g_compares;
    g_compare_bytes += bytes;
}

void EndFrame(uint32_t frame)
{
    SampleAt(frame);
    t_presenting = true;
    const uint32_t interval = Interval();
    if (interval == 0)
    {
        return;
    }
    const Clock::time_point now = Clock::now();
    const uint64_t cpu_ns = platform::ThreadCpuTimeNanoseconds();
    if (g_started)
    {
        // Stutter shows in single frames, which a window's average hides.
        const Clock::duration since_last = now - g_last_present;
        g_worst_frame = std::max(g_worst_frame, since_last);
        g_slow_frames += since_last > std::chrono::microseconds(33400) ? 1 : 0;
    }
    g_last_present = now;
    if (!g_started)
    {
        g_started = true;
        g_window_start = now;
        g_window_cpu_ns = cpu_ns;
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
                 "[d3d8 profile] %.1f fps, %.2f ms/frame, worst %.1f (%u over 33.4), cpu %.2f: draws %.0f (%.0f program), uploads %.0f KiB vertices %.0f KiB indices, compared %.0f KiB in %.0f ranges | indices %.2f setup %.2f "
                 "target %.2f upload %.2f attributes %.2f constants %.2f textures %.2f state %.2f draw %.2f present %.2f wait %.2f | "
                 "other %.2f\n",
                 1000.0 / wall, wall, Milliseconds(g_worst_frame), g_slow_frames,
                 static_cast<double>(cpu_ns - g_window_cpu_ns) / 1e6 / frames,
                 static_cast<double>(g_draws) / frames,
                 static_cast<double>(g_program_draws) / frames, static_cast<double>(g_vertex_bytes) / frames / 1024.0,
                 static_cast<double>(g_index_bytes) / frames / 1024.0, static_cast<double>(g_compare_bytes) / frames / 1024.0,
                 static_cast<double>(g_compares) / frames, per_frame(Phase::kIndices), per_frame(Phase::kSetup),
                 per_frame(Phase::kTarget), per_frame(Phase::kUpload), per_frame(Phase::kAttributes),
                 per_frame(Phase::kConstants), per_frame(Phase::kTextures), per_frame(Phase::kState),
                 per_frame(Phase::kDraw), per_frame(Phase::kPresent), per_frame(Phase::kWait), wall - measured);
    if (g_missing_sites != 0)
    {
        std::string line = "[d3d8 profile] unimplemented per frame:";
        for (size_t i = 0; i < g_missing_sites; ++i)
        {
            line += Format(" %s %.1f", g_missing[i].site, static_cast<double>(g_missing[i].count) / frames);
            g_missing[i].count = 0;
        }
        std::fprintf(stderr, "%s\n", line.c_str());
    }
    if (g_programs_built + g_programs_cached != 0)
    {
        // Programs made in this window (not per frame): they come in bursts.
        std::fprintf(stderr,
                     "[d3d8 profile] programs: %u compiled, %u from the binary cache, %.1f ms in all; stored %u, %.1f ms\n",
                     g_programs_built, g_programs_cached, Milliseconds(g_program_time), g_programs_stored,
                     Milliseconds(g_store_time));
        g_programs_built = g_programs_cached = g_programs_stored = 0;
        g_program_time = g_store_time = {};
    }
    if (GlCallCountingEnabled())
    {
        // REXBOX_GL_CALLS=1: GL calls per frame, by entry point.
        uint64_t total = 0;
        const auto counts = TakeGlCallCounts();
        for (const auto& [name, count] : counts)
        {
            total += count;
        }
        std::string line = Format("[d3d8 profile] gl calls %.0f/frame:", static_cast<double>(total) / frames);
        for (size_t i = 0; i < counts.size() && i < 16; ++i)
        {
            line += Format(" %s %.0f", counts[i].first, static_cast<double>(counts[i].second) / frames);
        }
        std::fprintf(stderr, "%s\n", line.c_str());
    }
    g_window_start = now;
    g_window_cpu_ns = cpu_ns;
    g_worst_frame = {};
    g_slow_frames = 0;
    g_phases = {};
    g_draws = g_program_draws = 0;
    g_vertex_bytes = g_index_bytes = 0;
    g_compares = g_compare_bytes = 0;
    g_frames = 0;
}

}  // namespace rexbox::hle::d3d8::profile
