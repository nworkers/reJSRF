#include "d3d8_vblank.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace rexbox::hle::d3d8
{

namespace
{

using Clock = std::chrono::steady_clock;

// D3DPRESENT_INTERVAL_IMMEDIATE (Xbox and PC).
constexpr uint32_t kIntervalImmediate = 0x80000000u;
// The fraction of a field spent in the vertical blank (NTSC: about 45 of 525 lines).
constexpr double kBlankFraction = 45.0 / 525.0;

const Clock::time_point g_start = Clock::now();
std::atomic<uint32_t> g_hertz{60};
std::atomic<uint64_t> g_last_present{0};

Clock::duration Period()
{
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / g_hertz.load()));
}

Clock::time_point TickTime(uint64_t tick)
{
    return g_start + Period() * static_cast<int64_t>(tick);
}

}  // namespace

void SetVerticalBlankRate(uint32_t hertz)
{
    g_hertz.store(hertz != 0 ? hertz : 60);
}

uint64_t VerticalBlankCount()
{
    return static_cast<uint64_t>((Clock::now() - g_start) / Period());
}

void WaitForVerticalBlank()
{
    std::this_thread::sleep_until(TickTime(VerticalBlankCount() + 1));
}

void WaitForPresentInterval(uint32_t interval)
{
    if (interval == 0 || (interval & kIntervalImmediate) != 0)
    {
        g_last_present.store(VerticalBlankCount());
        return;
    }
    const uint64_t now = VerticalBlankCount();
    uint64_t target = g_last_present.load() + interval;
    if (target <= now)
    {
        // Late (or the first present): show at the next blank.
        target = now + 1;
    }
    std::this_thread::sleep_until(TickTime(target));
    g_last_present.store(target);
}

void RasterPosition(bool* in_vblank, uint32_t* scan_line, uint32_t lines)
{
    const Clock::duration period = Period();
    const double phase = std::chrono::duration<double>((Clock::now() - g_start) % period).count() /
                         std::chrono::duration<double>(period).count();
    const double active = 1.0 - kBlankFraction;
    *in_vblank = phase >= active;
    *scan_line = *in_vblank ? 0 : static_cast<uint32_t>(phase / active * lines);
}

}  // namespace rexbox::hle::d3d8
