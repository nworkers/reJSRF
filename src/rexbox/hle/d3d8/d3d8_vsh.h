// NV2A vertex-program microcode: decoding, disassembly and translation to
// GLSL (pure, no GL; verified by rexbox_d3d8_probe).
//
// A program is what the title passes to CreateVertexShader as pFunction: a
// header DWORD (type in the low half, instruction count in the high half)
// followed by 16-byte instructions. Each instruction drives the two units of
// the NV2A vertex engine, the MAC (vector ops) and the ILU (scalar ops), from
// up to three sources (docs/kb/xbox-d3d8-api.md, vertex shaders).
//
// Field layout as (dword, lowest bit, width), the same as xboxrecomp's
// nv2a_vsh_interp.c (MIT), which checked it against real programs:
//   ILU op 1 25 3, MAC op 1 21 4, constant 1 13 8, input 1 9 4,
//   A: negate 1 8 1, swizzle 1 0 8, temp 2 28 4, mux 2 26 2,
//   B: negate 2 25 1, swizzle 2 17 8, temp 2 13 4, mux 2 11 2,
//   C: negate 2 10 1, swizzle 2 2 8, temp 2 0 2 (high) + 3 30 2 (low), mux 3 28 2,
//   MAC temp mask 3 24 4, temp 3 20 4, ILU temp mask 3 16 4, output mask 3 12 4,
//   output to o[] 3 11 1, output address 3 3 8, output from the ILU 3 2 1,
//   a0.x-relative constant 3 1 1, final 3 0 1.

#ifndef REXBOX_HLE_D3D8_D3D8_VSH_H
#define REXBOX_HLE_D3D8_D3D8_VSH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rexbox::hle::d3d8::vsh
{

constexpr uint32_t kMaxInstructions = 136;  // NV2A program slots
constexpr uint32_t kConstants = 192;        // c[0..191]; D3D numbers them -96..95
constexpr uint32_t kInputs = 16;            // v0..v15
constexpr uint32_t kTemps = 12;             // R0..R11; R12 reads back oPos

// Program header types (the low half of the first DWORD).
constexpr uint16_t kTypeVertex = 0x2078;      // an ordinary vertex shader
constexpr uint16_t kTypeReadWrite = 0x7778;   // may write constants
constexpr uint16_t kTypeState = 0x7378;       // a state shader (run by RunVertexStateShader)

namespace mac
{
constexpr uint8_t kNop = 0, kMov = 1, kMul = 2, kAdd = 3, kMad = 4, kDp3 = 5, kDph = 6, kDp4 = 7, kDst = 8,
                  kMin = 9, kMax = 10, kSlt = 11, kSge = 12, kArl = 13;
}
namespace ilu
{
constexpr uint8_t kNop = 0, kMov = 1, kRcp = 2, kRcc = 3, kRsq = 4, kExp = 5, kLog = 6, kLit = 7;
}
namespace mux
{
constexpr uint8_t kTemp = 1, kInput = 2, kConstant = 3;
}

// Output addresses (o[] registers).
namespace out
{
constexpr uint8_t kPosition = 0, kDiffuse = 3, kSpecular = 4, kFog = 5, kPointSize = 6, kBackDiffuse = 7,
                  kBackSpecular = 8, kTexture0 = 9;  // .. kTexture3 = 12
constexpr uint8_t kCount = 13;
}

struct Source
{
    uint8_t mux = 0;
    uint8_t temp = 0;
    uint8_t swizzle[4] = {0, 1, 2, 3};
    bool negate = false;
};

struct Instruction
{
    uint8_t mac = 0;
    uint8_t ilu = 0;
    uint8_t constant = 0;
    uint8_t input = 0;
    Source a, b, c;
    uint8_t mac_mask = 0;  // x = 8, y = 4, z = 2, w = 1
    uint8_t ilu_mask = 0;
    uint8_t temp = 0;
    uint8_t out_mask = 0;
    bool out_to_o = false;  // false: the output goes to c[out_address]
    uint8_t out_address = 0;
    bool out_from_ilu = false;
    bool relative = false;  // the constant source is c[a0.x + constant]
    bool final = false;
};

Instruction Decode(const uint32_t words[4]);

// One line per instruction, in xsasm-like syntax ("mul R0.xyz, v0, c[3]").
std::string Disassemble(const Instruction& instruction);

struct Program
{
    uint16_t type = 0;
    std::vector<std::array<uint32_t, 4>> words;
};

// Reads a program (header and instructions) from `bytes` (`size` available).
// Stops at the final instruction or the header's count, whichever is first.
bool ReadProgram(const uint8_t* bytes, size_t size, Program* program, std::string* error);

// --- Vertex declarations (the D3DVSD tokens of CreateVertexShader) ---------

// D3DVSDT register types: the high nibble is the component count, the low
// nibble the kind (2 float, 1 normalized short, 5 short, 4 normalized unsigned
// byte); D3DCOLOR is 0x40 (B, G, R, A bytes), NORMPACKED3 0x16, NONE 0x02.
namespace vsdt
{
constexpr uint8_t kFloat1 = 0x12, kFloat2 = 0x22, kFloat3 = 0x32, kFloat4 = 0x42;
constexpr uint8_t kD3dColor = 0x40;
constexpr uint8_t kNormShort1 = 0x11, kNormShort2 = 0x21, kNormShort3 = 0x31, kNormShort4 = 0x41;
constexpr uint8_t kShort1 = 0x15, kShort2 = 0x25, kShort3 = 0x35, kShort4 = 0x45;
constexpr uint8_t kPByte1 = 0x14, kPByte2 = 0x24, kPByte3 = 0x34, kPByte4 = 0x44;
constexpr uint8_t kNormPacked3 = 0x16;
constexpr uint8_t kNone = 0x02;
}  // namespace vsdt

enum class ElementKind : uint8_t
{
    kFloat,
    kNormShort,
    kShort,
    kNormByte,
    kD3dColor,
};

struct VertexElement
{
    uint8_t reg = 0;  // v0..v15
    uint8_t stream = 0;
    uint8_t type = 0;  // the D3DVSDT value
    ElementKind kind = ElementKind::kFloat;
    uint8_t components = 0;
    uint16_t offset = 0;  // bytes into the stream's vertex
};

struct Declaration
{
    std::vector<VertexElement> elements;
    uint16_t stream_bytes[16] = {};  // the vertex size the declaration implies per stream
    uint32_t streams = 0;            // bit per stream used
    uint32_t bgra_inputs = 0;        // bit per register holding a D3DCOLOR
};

// Reads D3DVSD tokens up to D3DVSD_END. False (with a reason) for a token or
// register type this batch does not handle.
bool ParseDeclaration(const std::vector<uint32_t>& tokens, Declaration* declaration, std::string* error);

// GLSL vertex shader for `program` ("#version 330 core", or "300 es" with
// `gles`). Inputs are attributes v0..v15 at locations 0..15 (`used_inputs`
// gets bit i where the program reads vi); a D3DCOLOR input (`bgra_inputs`)
// is swizzled to R, G, B, A. Constants are the uniform array c[192]. Outputs
// feed the fixed-function fragment stage: v_diffuse, v_specular, v_tex0..3,
// v_fog. The screen-space position the Xbox runtime's epilogue writes is
// mapped back to clip space with the viewport constants c[58] (scale) and
// c[59] (offset).
std::string TranslateToGlsl(const Program& program, uint32_t bgra_inputs, bool gles, uint32_t* used_inputs);

// The viewport constants the epilogue reads, for a viewport at (x, y) of
// width x height pixels: c[58] = (w/2, -h/2, 1, 1), c[59] = (x + w/2,
// y + h/2, 0, 0). These are the HLE's own values; the hardware's also fold in
// the depth range and the depth buffer's precision, which GL applies itself,
// and the translation undoes whatever the constants hold.
void ViewportConstants(uint32_t x, uint32_t y, uint32_t width, uint32_t height, float scale[4], float offset[4]);

}  // namespace rexbox::hle::d3d8::vsh

#endif  // REXBOX_HLE_D3D8_D3D8_VSH_H
