// A diagnostic: what draws with a chosen vertex-shader program see
// (docs/design/20261016-i070-outline-watch.md).
//
// REXBOX_VSH_WATCH=<handle>[:<first>-<last>][,...] names program handles
// (decimal as in REXBOX_SHADER_DUMP, or 0x hexadecimal) and the raw c[]
// range to print (default 96-103). A draw with a watched handle prints one
// line to stderr when its constants or state differ from every line printed
// for that handle before, at most 64 lines per handle. REXBOX_VSH_WATCH=all
// watches every program handle with the default range, at most 4 lines each
// (run with REXBOX_SHADER_DUMP=1 to match handles to their microcode).

#ifndef REXBOX_HLE_D3D8_D3D8_DRAW_WATCH_H
#define REXBOX_HLE_D3D8_D3D8_DRAW_WATCH_H

#include "d3d8_ff_shader.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

// Whether REXBOX_VSH_WATCH names any handle (read once).
bool DrawWatchEnabled();

// A program draw with `vertex_shader` and the fragment stage `fragment_key`
// (its stage operations when no pixel shader is set).
void WatchProgramDraw(uint32_t vertex_shader, const FixedFunctionKey& fragment_key);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_DRAW_WATCH_H
