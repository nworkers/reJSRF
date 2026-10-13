// GL programs for fixed-function keys (d3d8_ff_shader.h) and for vertex-shader
// programs (d3d8_vsh.h), compiled once per key and kept. A program that fails to compile or link is reported once
// with its log and source, and its draws are skipped.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
#define REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H

#include "d3d8_ff_shader.h"
#include "d3d8_shader.h"

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

// A vertex-shader program (d3d8_vsh.h) linked with the fixed-function
// fragment stage for `fragment_key` (its vertex inputs unused).
struct VertexProgramGl
{
    uint32_t program = 0;
    uint32_t used_inputs = 0;  // bit per v register the program reads
    int constants = -1;        // uniform locations, -1 when unused
    int tfactor = -1;
    int alpha_ref = -1;
    int tex_scale[kFixedFunctionStages] = {-1, -1, -1, -1};
    // Register-combiner fragment stage (GetVertexPixelProgram) only.
    int stage_c0 = -1;
    int stage_c1 = -1;
    int final_c0 = -1;
    int final_c1 = -1;
    int fog_color = -1;
};

const VertexProgramGl* GetVertexProgram(const ProgramShader& shader, const FixedFunctionKey& fragment_key);

// A vertex-shader program linked with a register-combiner pixel shader
// (d3d8_psh.h).
const VertexProgramGl* GetVertexPixelProgram(const ProgramShader& shader, const psh::Key& pixel_key);

// The streaming buffers every draw uploads into: one vertex array object
// with a vertex buffer and an index buffer, refilled per draw.
void BindStreamingBuffers();
void UploadVertexData(const void* data, size_t bytes);
void UploadIndexData(const void* data, size_t bytes);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
