// A diagnostic: one frame's draws on request (F9 in the window;
// docs/design/20261016-i070-outline-watch.md section 2.2).
//
// After the key, the next frame is captured: every Clear and draw is written,
// with the state that decides its color, to capture_<frame>.txt, and the
// presented picture to capture_<frame>.bmp, both in REXBOX_FRAME_DUMP_DIR
// (default: the current folder). REXBOX_CAPTURE_FRAME=<n> captures frame n
// without the key.

#ifndef REXBOX_HLE_D3D8_D3D8_CAPTURE_H
#define REXBOX_HLE_D3D8_D3D8_CAPTURE_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

// At each present, after the window events: arms the next frame when the
// key was pressed, and finishes a capture of `frame` (writing the picture of
// `shown`).
void CaptureAtPresent(uint32_t frame, GuestAddress shown);

// Whether the current frame is being captured.
bool CaptureActive();

// One draw call as the title made it (before any path decides how to draw).
void CaptureDraw(const char* name, uint32_t primitive, uint32_t count);

// One Clear.
void CaptureClear(uint32_t flags, uint32_t color, float z, uint32_t stencil);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_CAPTURE_H
