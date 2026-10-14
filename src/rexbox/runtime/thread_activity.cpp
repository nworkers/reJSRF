#include "rexbox/runtime/thread_activity.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace rexbox::runtime
{

namespace
{

using Clock = std::chrono::steady_clock;

int64_t NowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
}

constexpr size_t kMaxWaitObjects = 4;

// One per host thread that ever noted anything. Never freed: the monitor may
// read a record after its thread ended, and threads are few.
struct ThreadRecord
{
    std::mutex name_mutex;
    std::string name;
    std::atomic<bool> alive{true};
    std::atomic<const char*> hle{nullptr};
    std::atomic<uint32_t> caller{0};
    std::atomic<int64_t> hle_ns{0};
    // A wait is published by storing wait_what last and retracted by
    // clearing it; the monitor reads it first. A torn read while a thread
    // moves between waits only blurs one diagnostic line.
    std::atomic<const char*> wait_what{nullptr};
    std::atomic<uint32_t> wait_count{0};
    std::array<std::atomic<uint32_t>, kMaxWaitObjects> wait_objects{};
    std::atomic<bool> wait_all{false};
    std::atomic<int64_t> wait_timeout_ms{-1};
    std::atomic<int64_t> wait_since_ns{0};
};

std::mutex g_records_mutex;
std::vector<ThreadRecord*> g_records;

std::atomic<int64_t> g_last_present_ns{0};
std::atomic<uint32_t> g_last_frame{0};
std::atomic<WaitObjectDescriber> g_describer{nullptr};

// Marks the record dead when its thread ends.
struct RecordOwner
{
    ThreadRecord* record = nullptr;
    ~RecordOwner()
    {
        if (record != nullptr)
        {
            record->alive.store(false, std::memory_order_relaxed);
        }
    }
};

ThreadRecord& Self()
{
    thread_local RecordOwner owner;
    if (owner.record == nullptr)
    {
        owner.record = new ThreadRecord;
        std::lock_guard<std::mutex> lock(g_records_mutex);
        g_records.push_back(owner.record);
    }
    return *owner.record;
}

double Seconds(int64_t ns)
{
    return static_cast<double>(ns) / 1e9;
}

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", value);
    return text;
}

std::string DescribeObject(GuestAddress object)
{
    std::string text = Hex(object);
    if (const WaitObjectDescriber describer = g_describer.load())
    {
        const std::string described = describer(object);
        if (!described.empty())
        {
            text += " " + described;
        }
    }
    return text;
}

std::string DescribeThread(ThreadRecord& record, int64_t now)
{
    std::string name;
    {
        std::lock_guard<std::mutex> lock(record.name_mutex);
        name = record.name.empty() ? "host thread" : record.name;
    }
    std::string line = name + ": ";
    if (!record.alive.load(std::memory_order_relaxed))
    {
        return line + "ended";
    }
    if (const char* what = record.wait_what.load(std::memory_order_acquire))
    {
        char head[96];
        std::snprintf(head, sizeof(head), "waiting %.1f s in %s on ",
                      Seconds(now - record.wait_since_ns.load(std::memory_order_relaxed)), what);
        line += head;
        const uint32_t count = record.wait_count.load(std::memory_order_relaxed);
        if (count == 0)
        {
            line += "no object";
        }
        for (uint32_t i = 0; i < count && i < kMaxWaitObjects; ++i)
        {
            line += (i == 0 ? "" : ", ") + DescribeObject(record.wait_objects[i].load(std::memory_order_relaxed));
        }
        if (count > kMaxWaitObjects)
        {
            line += ", ... (" + std::to_string(count) + " objects)";
        }
        if (count > 1)
        {
            line += record.wait_all.load(std::memory_order_relaxed) ? " (all)" : " (any)";
        }
        const int64_t timeout = record.wait_timeout_ms.load(std::memory_order_relaxed);
        line += timeout < 0 ? ", no timeout" : ", timeout " + std::to_string(timeout) + " ms";
    }
    else
    {
        line += "running";
    }
    if (const char* hle = record.hle.load(std::memory_order_relaxed))
    {
        char tail[160];
        std::snprintf(tail, sizeof(tail), "; last HLE %s %.1f s ago from %s", hle,
                      Seconds(now - record.hle_ns.load(std::memory_order_relaxed)),
                      Hex(record.caller.load(std::memory_order_relaxed)).c_str());
        line += tail;
    }
    else
    {
        line += "; no HLE call yet";
    }
    return line;
}

void Report(int64_t now, int64_t since_present)
{
    std::vector<ThreadRecord*> records;
    {
        std::lock_guard<std::mutex> lock(g_records_mutex);
        records = g_records;
    }
    std::fprintf(stderr, "[stall] no present for %.1f s (last present %u); guest threads:\n", Seconds(since_present),
                 g_last_frame.load());
    for (ThreadRecord* record : records)
    {
        std::fprintf(stderr, "[stall]   %s\n", DescribeThread(*record, now).c_str());
    }
    std::fflush(stderr);
}

void Monitor(int64_t threshold_ns)
{
    bool reported = false;
    int64_t reported_present = 0;
    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        const int64_t last = g_last_present_ns.load();
        if (last == 0)
        {
            continue;  // still booting
        }
        const int64_t now = NowNs();
        if (reported)
        {
            if (last != reported_present)
            {
                std::fprintf(stderr, "[stall] presents resumed after %.1f s\n", Seconds(last - reported_present));
                std::fflush(stderr);
                reported = false;
            }
            continue;
        }
        if (now - last >= threshold_ns)
        {
            Report(now, now - last);
            reported = true;
            reported_present = last;
        }
    }
}

}  // namespace

void NoteThreadName(std::string name)
{
    ThreadRecord& record = Self();
    std::lock_guard<std::mutex> lock(record.name_mutex);
    record.name = std::move(name);
}

void NoteHleEntry(const char* name, uint32_t caller)
{
    ThreadRecord& record = Self();
    record.hle.store(name, std::memory_order_relaxed);
    record.caller.store(caller, std::memory_order_relaxed);
    record.hle_ns.store(NowNs(), std::memory_order_relaxed);
}

void NoteWaitBegin(const char* what, const GuestAddress* objects, size_t count, bool all, int64_t timeout_ms)
{
    ThreadRecord& record = Self();
    record.wait_count.store(static_cast<uint32_t>(count), std::memory_order_relaxed);
    for (size_t i = 0; i < count && i < kMaxWaitObjects; ++i)
    {
        record.wait_objects[i].store(objects[i], std::memory_order_relaxed);
    }
    record.wait_all.store(all, std::memory_order_relaxed);
    record.wait_timeout_ms.store(timeout_ms, std::memory_order_relaxed);
    record.wait_since_ns.store(NowNs(), std::memory_order_relaxed);
    record.wait_what.store(what, std::memory_order_release);
}

void NoteWaitEnd()
{
    Self().wait_what.store(nullptr, std::memory_order_release);
}

void NotePresent(uint32_t frame)
{
    g_last_frame.store(frame, std::memory_order_relaxed);
    g_last_present_ns.store(NowNs(), std::memory_order_relaxed);
}

void SetWaitObjectDescriber(WaitObjectDescriber describer)
{
    g_describer.store(describer);
}

void StartStallMonitor()
{
    const char* setting = std::getenv("REXBOX_STALL_REPORT");
    const double seconds = setting != nullptr ? std::atof(setting) : 10.0;
    if (seconds <= 0.0)
    {
        return;
    }
    std::thread(Monitor, static_cast<int64_t>(seconds * 1e9)).detach();
}

}  // namespace rexbox::runtime
