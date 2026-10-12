// A virtual vertical-blank clock at the display's refresh rate (60 Hz unless
// the title asked for another). The Xbox paces its frame loop by the TV's
// vertical blank; the host window may have no vsync (hidden, headless) or a
// different refresh, so waits are measured against this clock instead
// (docs/design/20261012-i023-d3d8-fixed-function-draw.md section 4).

#ifndef REXBOX_HLE_D3D8_D3D8_VBLANK_H
#define REXBOX_HLE_D3D8_D3D8_VBLANK_H

#include <cstdint>

namespace rexbox::hle::d3d8
{

// Sets the refresh rate in Hz (0 keeps 60).
void SetVerticalBlankRate(uint32_t hertz);

// Vertical blanks since the clock started.
uint64_t VerticalBlankCount();

// Sleeps until the next vertical blank.
void WaitForVerticalBlank();

// Sleeps until `interval` vertical blanks have passed since the previous
// present (D3DPRESENT_INTERVAL_ONE = 1, TWO = 2, ...; 0 or IMMEDIATE does
// not wait).
void WaitForPresentInterval(uint32_t interval);

// Whether the clock is inside the vertical blank right now, and the scan
// line it would be on (for D3DDevice_GetRasterStatus).
void RasterPosition(bool* in_vblank, uint32_t* scan_line, uint32_t lines);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_VBLANK_H
