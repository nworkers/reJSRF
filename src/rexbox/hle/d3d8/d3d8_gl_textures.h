// Guest textures as GL textures. The pixels stay in guest memory (the title
// writes them directly or through a lock); a GL copy is made on first use and
// remade when the header changes, after the texture was locked, or when a
// sampled hash of its level 0 changes (docs/design/20261012-i023-d3d8-fixed-function-draw.md
// section 3.3). Sampler state comes from the texture-stage globals.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_TEXTURES_H
#define REXBOX_HLE_D3D8_D3D8_GL_TEXTURES_H

#include "d3d8_ff_shader.h"

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

struct BoundTexture
{
    uint32_t texture = 0;  // GL name, 0 when nothing could be made
    SamplerKind kind = SamplerKind::kNone;
    bool unnormalized = false;  // linear: sampled with texel coordinates
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t levels = 1;
    uint32_t format = 0xFFFFFFFF;  // D3DFORMAT of the header; unknown for render targets
};

// The GL texture for the header bound to a stage (uploaded as needed).
BoundTexture ResolveTexture(GuestAddress header);

// Binds `texture` to texture unit `stage` with the stage's sampler state.
void BindStageTexture(uint32_t stage, const BoundTexture& texture);

// The title locked the texture (or a surface of it): upload again on next use.
void MarkTextureWritten(GuestAddress header);

// The header is gone (its resource was destroyed): drop the GL copy.
void ForgetTexture(GuestAddress header);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_TEXTURES_H
