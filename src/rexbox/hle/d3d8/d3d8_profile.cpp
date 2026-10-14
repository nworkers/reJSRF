#include "d3d8_profile.h"

#include "d3d8_gl.h"
#include "d3d8_trace.h"

#include "rexbox/platform/host_thread.h"
#include "rexbox/platform/sampler.h"

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
uint64_t g_compare_bytes = 0;
uint32_t g_frames = 0;
Clock::time_point g_window_start;
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
                 "[d3d8 profile] %.1f fps, %.2f ms/frame, cpu %.2f: draws %.0f (%.0f program), uploads %.0f KiB vertices %.0f KiB indices, compared %.0f KiB in %.0f ranges | indices %.2f setup %.2f "
                 "target %.2f upload %.2f attributes %.2f constants %.2f textures %.2f state %.2f draw %.2f present %.2f wait %.2f | "
                 "other %.2f\n",
                 1000.0 / wall, wall, static_cast<double>(cpu_ns - g_window_cpu_ns) / 1e6 / frames,
                 static_cast<double>(g_draws) / frames,
                 static_cast<double>(g_program_draws) / frames, static_cast<double>(g_vertex_bytes) / frames / 1024.0,
                 static_cast<double>(g_index_bytes) / frames / 1024.0, static_cast<double>(g_compare_bytes) / frames / 1024.0,
                 static_cast<double>(g_compares) / frames, per_frame(Phase::kIndices), per_frame(Phase::kSetup),
                 per_frame(Phase::kTarget), per_frame(Phase::kUpload), per_frame(Phase::kAttributes),
                 per_frame(Phase::kConstants), per_frame(Phase::kTextures), per_frame(Phase::kState),
                 per_frame(Phase::kDraw), per_frame(Phase::kPresent), per_frame(Phase::kWait), wall - measured);
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
    g_phases = {};
    g_draws = g_program_draws = 0;
    g_vertex_bytes = g_index_bytes = 0;
    g_compares = g_compare_bytes = 0;
    g_frames = 0;
}

}  // namespace rexbox::hle::d3d8::profile
