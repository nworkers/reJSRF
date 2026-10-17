// The display settings inside the D3D8 HLE (public side:
// include/rexbox/hle/d3d8/d3d8_display.h).

#ifndef REXBOX_HLE_D3D8_D3D8_DISPLAY_STATE_H
#define REXBOX_HLE_D3D8_D3D8_DISPLAY_STATE_H

#include "rexbox/hle/d3d8/d3d8_display.h"

namespace rexbox::hle::d3d8
{

struct GlFunctions;

// What SetDisplayDefaults gave (environment variables are applied by the
// readers).
DisplayDefaults CurrentDisplayDefaults();

// Records the driver's strings once the context exists.
void RecordRendererInfo(GlFunctions& gl);

// Tells the window the picture's shape (4:3 or 16:9 from the back buffer's
// height); no-op without a display.
void ApplyPictureAspect();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_DISPLAY_STATE_H
