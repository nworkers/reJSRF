// GL programs for fixed-function keys (d3d8_ff_shader.h), compiled once per
// key and kept. A program that fails to compile or link is reported once
// with its log and source, and its draws are skipped.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
#define REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H

#include "d3d8_ff_shader.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

struct FixedFunctionProgram
{
    uint32_t program = 0;
    int transform = -1;  // uniform locations, -1 when unused
    int target = -1;
    int tfactor = -1;
    int alpha_ref = -1;
    int tex_scale[kFixedFunctionStages] = {-1, -1, -1, -1};
};

// nullptr when the program for `key` cannot be built.
const FixedFunctionProgram* GetFixedFunctionProgram(const FixedFunctionKey& key);

// The streaming buffers every draw uploads into: one vertex array object
// with a vertex buffer and an index buffer, refilled per draw.
void BindStreamingBuffers();
void UploadVertexData(const void* data, size_t bytes);
void UploadIndexData(const void* data, size_t bytes);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
