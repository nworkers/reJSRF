// A diagnostic: one frame's draws on request (F9 in the window;
// docs/design/20261016-i070-outline-watch.md sections 2.2 and 2.3).
//
// After the key, the next frame is captured, in REXBOX_FRAME_DUMP_DIR
// (default: the current folder):
//   capture_<frame>.txt        every Clear and draw with the state that
//                              decides its color (viewport, raw texture and
//                              target headers, the vertices of small draws)
//   capture_<frame>.bmp        the presented picture
//   capture_<frame>_owner.bmp  per pixel of the presented picture, the call
//                              that changed it last: call index + 1 in red
//                              (low byte) and green (high byte), 0 = none
// REXBOX_CAPTURE_FRAME=<n> captures frame n without the key.

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
// `vertices` and `stride` locate stream 0 (0 for host-side vertices);
// `indices` is the 16-bit index data or 0, added to `index_base`.
void CaptureDraw(const char* name, uint32_t primitive, uint32_t count, GuestAddress vertices, uint32_t stride,
                 GuestAddress indices, uint32_t index_base);

// One Clear.
void CaptureClear(uint32_t flags, uint32_t color, float z, uint32_t stencil);

// After a captured call reached GL: which pixels of the bound target it
// changed.
void CaptureAfterCall();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_CAPTURE_H
