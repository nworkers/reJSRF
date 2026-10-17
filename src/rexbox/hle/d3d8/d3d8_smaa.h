// SMAA 1x on the presented picture: the official implementation (iryoku/smaa,
// MIT, third_party/smaa) compiled as GLSL 3.30
// (docs/design/20261017-i089-anti-aliasing.md section 2.3).

#ifndef REXBOX_HLE_D3D8_D3D8_SMAA_H
#define REXBOX_HLE_D3D8_D3D8_SMAA_H

#include "d3d8_gl.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

struct SmaaOutput
{
    GLuint texture = 0;      // 0 when SMAA could not run (reported once)
    GLuint framebuffer = 0;  // `texture` attached, for blits
};

// Runs the three SMAA passes over `input` (width x height, RGBA8) into a
// texture of the same size. Changes GL state; the caller invalidates the
// GL state cache.
SmaaOutput ApplySmaa(GLuint input, uint32_t width, uint32_t height);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_SMAA_H
