#include "d3d8_vblank.h"

#include "rexbox/runtime/thread_activity.h"

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
// Added to g_start by AlignVerticalBlank (Clock ticks).
std::atomic<int64_t> g_phase{0};
std::atomic<uint32_t> g_hertz{60};
// How far a tick leads the host's blank once aligned: Swap's wait must wake
// in time for the blit and the swap request to make that blank.
constexpr auto kAlignLead = std::chrono::milliseconds(3);
std::atomic<uint64_t> g_last_present{0};

Clock::duration Period()
{
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / g_hertz.load()));
}

Clock::time_point Base()
{
    return g_start + Clock::duration(g_phase.load());
}

Clock::time_point TickTime(uint64_t tick)
{
    return Base() + Period() * static_cast<int64_t>(tick);
}

uint64_t CountAt(Clock::time_point time)
{
    return time > Base() ? static_cast<uint64_t>((time - Base()) / Period()) : 0;
}

}  // namespace

void SetVerticalBlankRate(uint32_t hertz)
{
    g_hertz.store(hertz != 0 ? hertz : 60);
}

uint32_t VerticalBlankRate()
{
    return g_hertz.load();
}

uint64_t VerticalBlankCount()
{
    return CountAt(Clock::now());
}

void AlignVerticalBlank(Clock::time_point host_blank)
{
    // Tick n (the current one) starts kAlignLead before host_blank, so tick
    // n + 1 comes kAlignLead before the host's next blank.
    const uint64_t n = CountAt(host_blank);
    const Clock::time_point base = host_blank - kAlignLead - Period() * static_cast<int64_t>(n);
    g_phase.store((base - g_start).count());
}

void WaitForVerticalBlank()
{
    const runtime::ScopedWait note("D3D8 vertical blank", nullptr, 0, false, 17);
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
    {
        const runtime::ScopedWait note("D3D8 present interval", nullptr, 0, false, 17 * static_cast<int64_t>(interval));
        std::this_thread::sleep_until(TickTime(target));
    }
    g_last_present.store(target);
}

void RasterPosition(bool* in_vblank, uint32_t* scan_line, uint32_t lines)
{
    const Clock::duration period = Period();
    const double phase = std::chrono::duration<double>((Clock::now() - Base()) % period).count() /
                         std::chrono::duration<double>(period).count();
    const double active = 1.0 - kBlankFraction;
    *in_vblank = phase >= active;
    *scan_line = *in_vblank ? 0 : static_cast<uint32_t>(phase / active * lines);
}

}  // namespace rexbox::hle::d3d8
