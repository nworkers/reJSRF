// GL programs for fixed-function keys (d3d8_ff_shader.h) and for vertex-shader
// programs (d3d8_vsh.h), compiled once per key and kept. A program that fails to compile or link is reported once
// with its log and source, and its draws are skipped.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
#define REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H

#include "d3d8_ff_shader.h"
#include "d3d8_shader.h"

#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

// The values a program's uniforms were last given. Uniforms belong to the
// program, so these stay valid across program switches; a setter whose value
// is unchanged makes no GL call. Locations of -1 are ignored.
class UniformCache
{
public:
    void Set1f(int location, float x);
    void Set2f(int location, float x, float y);
    void Set4f(int location, float x, float y, float z, float w);
    void Set4fv(int location, int count, const float* values);
    void SetMatrix4(int location, const float* values);
    // A vec4 array whose element i is at locations[i] (-1 where the compiler
    // dropped it); only runs of changed elements are sent.
    void SetArray4(const std::vector<int>& locations, const float* values);

private:
    struct Entry
    {
        int location = -1;
        std::vector<float> values;
    };
    // True (and remembered) when `values` differ from what `location` holds.
    bool Changed(int location, const float* values, size_t count);

    std::vector<Entry> entries_;
    std::vector<float> array_;
};

struct FixedFunctionProgram
{
    uint32_t program = 0;
    int transform = -1;  // uniform locations, -1 when unused
    int target = -1;
    int tfactor = -1;
    int alpha_ref = -1;
    int tex_scale[kFixedFunctionStages] = {-1, -1, -1, -1};
    mutable UniformCache uniforms;
};

// nullptr when the program for `key` cannot be built.
const FixedFunctionProgram* GetFixedFunctionProgram(const FixedFunctionKey& key);

// A vertex-shader program (d3d8_vsh.h) linked with the fixed-function
// fragment stage for `fragment_key` (its vertex inputs unused).
struct VertexProgramGl
{
    uint32_t program = 0;
    uint32_t used_inputs = 0;  // bit per v register the program reads
    std::vector<int> constants;  // location of each c[] element, -1 when unused
    int tfactor = -1;
    int alpha_ref = -1;
    int tex_scale[kFixedFunctionStages] = {-1, -1, -1, -1};
    // Register-combiner fragment stage (GetVertexPixelProgram) only.
    int stage_c0 = -1;
    int stage_c1 = -1;
    int final_c0 = -1;
    int final_c1 = -1;
    int fog_color = -1;
    mutable UniformCache uniforms;
};

const VertexProgramGl* GetVertexProgram(const ProgramShader& shader, const FixedFunctionKey& fragment_key);

// A vertex-shader program linked with a register-combiner pixel shader
// (d3d8_psh.h).
const VertexProgramGl* GetVertexPixelProgram(const ProgramShader& shader, const psh::Key& pixel_key);

// glUseProgram unless `program` is already in use.
void UseProgram(uint32_t program);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_PROGRAMS_H
