// REXBOX_D3D8_TRACE=1: one line the first time each kind of thing appears
// (an FVF, a texture format, a program key, an unimplemented path), so a run
// shows what the title uses without a full call trace (diagnostic).
// Unimplemented paths are reported once even without the variable.

#ifndef REXBOX_HLE_D3D8_D3D8_TRACE_H
#define REXBOX_HLE_D3D8_D3D8_TRACE_H

#include "d3d8_profile.h"

#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

bool D3d8TraceEnabled();

// Prints "[d3d8] <text>" once per distinct `key` when tracing is on.
void TraceOnce(const std::string& key, const std::string& text);

// Prints "[d3d8] <text>" once per distinct `key` regardless of tracing: for
// paths this batch does not implement yet.
void ReportOnce(const std::string& key, const std::string& text);

// True the first time (`site`, `value`) is seen. `site` is a string literal
// naming the call site; its address is the key.
bool FirstReport(const char* site, uint64_t value);

// ReportOnce for paths taken on every draw: the message is built by `text()`
// only the first time `value` is seen at `site`, so a repeat costs one hash
// lookup instead of formatting.
template <typename Text>
void ReportOncePer(const char* site, uint64_t value, Text&& text)
{
    profile::CountMissing(site);
    if (FirstReport(site, value))
    {
        ReportOnce(std::string(site) + ":" + std::to_string(value), text());
    }
}

// printf-style formatting into a std::string.
std::string Format(const char* format, ...);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_TRACE_H
