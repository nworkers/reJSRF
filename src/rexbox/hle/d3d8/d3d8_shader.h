// Shader objects the draw path reads: vertex-shader programs (declaration and
// microcode, copied at CreateVertexShader as the library does) and the
// vertex constant registers.

#ifndef REXBOX_HLE_D3D8_D3D8_SHADER_H
#define REXBOX_HLE_D3D8_D3D8_SHADER_H

#include "d3d8_psh.h"
#include "d3d8_vsh.h"

#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

struct ProgramShader
{
    vsh::Declaration declaration;
    vsh::Program program;
    bool usable = false;  // both parsed; else `error` says why
    std::string error;
    uint64_t hash = 0;    // of the microcode and the declaration's D3DCOLOR inputs (the GL program key)
};

// The program behind a CreateVertexShader handle, or nullptr (an FVF code or
// an unknown handle).
const ProgramShader* FindProgramShader(uint32_t handle);

// c[0..191] as 192 x 4 floats (D3D register r is c[r + 96]).
const float* VertexShaderConstants();
// Changes whenever any vertex shader constant is written.
uint64_t VertexShaderConstantsVersion();

// The D3DPIXELSHADERDEF behind a CreatePixelShader handle, or nullptr.
const psh::Definition* FindPixelShader(uint32_t handle);

// The SetPixelShaderConstant registers 0..15.
const float (*PixelShaderConstants())[4];

// Writes the screen-space constants c[58] and c[59] for the viewport, as the
// library does when the viewport changes (vsh::ViewportConstants).
void WriteViewportConstants(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_SHADER_H
