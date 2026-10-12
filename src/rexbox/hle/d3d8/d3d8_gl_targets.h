// Render targets on GL: each color surface the title draws to is a GL
// texture, each depth surface a depth/stencil renderbuffer, and every pair in
// use gets a framebuffer object. The window shows a back buffer only when
// Swap blits it (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 3.5). GL's bottom-left origin is kept inside the targets; the
// drawing path flips D3D's top-left coordinates once, in the vertex shader
// and the viewport/scissor conversion.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H
#define REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H

#include "d3d8_gamma.h"

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

// Binds the framebuffer for the color surface and depth surface (either may
// be 0) and makes later draws and clears go there. Returns false when GL
// could not build it (the reason is reported once).
bool BindRenderTarget(GuestAddress color, GuestAddress depth);

// Size of the color target last bound (or the depth target without color).
uint32_t RenderTargetWidth();
uint32_t RenderTargetHeight();

// Shows `color` (a back buffer) in the window through the display gamma ramp,
// scaled to fit with its aspect ratio kept, and rebinds the current render target.
void PresentRenderTarget(GuestAddress color, const GammaRamp& ramp);

// Writes `color`'s pixels, through the gamma ramp as on screen, to a 24-bit
// BMP file. Returns false on failure.
bool DumpRenderTarget(GuestAddress color, const std::string& path, const GammaRamp& ramp);

// The GL texture behind a color surface used as a render target, or 0.
uint32_t RenderTargetTexture(GuestAddress color);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H
