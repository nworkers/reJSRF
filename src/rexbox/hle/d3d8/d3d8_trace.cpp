#include "d3d8_trace.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <unordered_set>
#include <utility>

namespace rexbox::hle::d3d8
{

namespace
{

std::mutex g_mutex;
std::unordered_set<std::string> g_seen;

void PrintOnce(const std::string& key, const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_seen.insert(key).second)
    {
        std::fprintf(stderr, "[d3d8] %s\n", text.c_str());
    }
}

}  // namespace

bool D3d8TraceEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REXBOX_D3D8_TRACE");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

void TraceOnce(const std::string& key, const std::string& text)
{
    if (D3d8TraceEnabled())
    {
        PrintOnce(key, text);
    }
}

bool FirstReport(const char* site, uint64_t value)
{
    // Pairs this thread already reported, direct mapped: a condition met on
    // every draw returns without the lock and the set lookup.
    struct Seen
    {
        const char* site = nullptr;
        uint64_t value = 0;
    };
    constexpr size_t kSlots = 64;
    thread_local Seen seen_here[kSlots];
    const size_t index = (reinterpret_cast<uintptr_t>(site) ^ static_cast<size_t>(value) ^
                          static_cast<size_t>(value >> 32)) % kSlots;
    Seen& slot = seen_here[index];
    if (slot.site == site && slot.value == value)
    {
        return false;
    }
    slot.site = site;
    slot.value = value;
    static std::mutex mutex;
    static std::set<std::pair<const char*, uint64_t>> seen;
    std::lock_guard<std::mutex> lock(mutex);
    return seen.emplace(site, value).second;
}

void ReportOnce(const std::string& key, const std::string& text)
{
    PrintOnce("report:" + key, text);
}

std::string Format(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

}  // namespace rexbox::hle::d3d8
