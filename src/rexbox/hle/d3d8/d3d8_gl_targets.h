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
#include <vector>

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

// A color target's pixels as RGBA8 in GL's row order (bottom row first).
void ReadRenderTarget(GuestAddress color, std::vector<uint8_t>* rgba, uint32_t* width, uint32_t* height);

// RGBA8 pixels, bottom row first, as a 24-bit BMP file. False on failure.
bool WriteBmp(const std::string& path, const uint8_t* rgba, uint32_t width, uint32_t height);

// For a texture over the memory of a color surface bound as a render target:
// a GL texture with the target's current pixels in D3D's row order (refreshed
// when the target was bound since), with the header last bound over that
// memory and the size; 0 when no target is at that memory. Targets are keyed
// by memory, not header (docs/design/20261016-i069-render-target-sampling.md,
// docs/design/20261017-i071-render-target-by-memory.md).
uint32_t SampleRenderTarget(GuestAddress data, GuestAddress* surface, uint32_t* width, uint32_t* height);

// Pixel memory at `data` was freed: textures there no longer sample a target.
void ForgetRenderTargetMemory(GuestAddress data);

// The GL texture behind a color surface used as a render target, or 0.
uint32_t RenderTargetTexture(GuestAddress color);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_TARGETS_H
