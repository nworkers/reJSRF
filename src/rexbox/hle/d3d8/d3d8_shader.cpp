// Shader objects and constants. The title's vertex shaders arrive as the XDK's
// declaration tokens and compiled microcode; the pixel shaders as a
// D3DPIXELSHADERDEF (register combiner setup). This batch keeps copies and
// hands out handles; the drawing batch translates them to GLSL.

#include "d3d8_shader.h"

#include "d3d8_internal.h"
#include "d3d8_state.h"
#include "d3d8_vsh.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// A vertex shader handle from CreateVertexShader is a pointer with bit 0 set
// (D3DFVF_RESERVED0); plain FVF codes have it clear.
constexpr uint32_t kVertexShaderHandleFlag = 0x1;
constexpr uint32_t kVertexShaderObjectSize = 0x40;
constexpr uint32_t kDeclarationEnd = 0xFFFFFFFF;
constexpr uint32_t kMaxDeclarationTokens = 256;
constexpr uint32_t kPixelShaderDefBytes = 0xF0;  // D3DPIXELSHADERDEF: 60 DWORDs
constexpr int32_t kVertexConstantBase = 96;      // registers run -96..95 in the title's constant mode
constexpr uint32_t kVertexConstantCount = 192;
constexpr uint32_t kPixelConstantCount = 16;

struct VertexShader
{
    std::vector<uint32_t> declaration;
    GuestAddress function = 0;  // the microcode's guest address at creation
    ProgramShader program;      // the copy the draw path uses
};

uint64_t Fnv1a(uint64_t hash, const void* data, size_t bytes)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < bytes; ++i)
    {
        hash = (hash ^ p[i]) * 1099511628211ull;
    }
    return hash;
}

// Copies and parses the program, as the library copies the microcode into
// the object at creation (the title may free its buffer afterwards).
void PrepareProgram(VertexShader& shader)
{
    ProgramShader& program = shader.program;
    if (!vsh::ParseDeclaration(shader.declaration, &program.declaration, &program.error))
    {
        return;
    }
    runtime::GuestMemory& memory = Memory();
    if (shader.function == 0 || !memory.IsValid(shader.function, 4))
    {
        program.error = "no microcode (fixed-function vertex processing)";
        return;
    }
    const uint32_t header = memory.Read32(shader.function);
    const uint32_t size = 4 + (header >> 16) * 16;
    if (!memory.IsValid(shader.function, size) ||
        !vsh::ReadProgram(memory.Translate(shader.function), size, &program.program, &program.error))
    {
        if (program.error.empty())
        {
            program.error = "microcode outside guest memory";
        }
        return;
    }
    uint64_t hash = 1469598103934665603ull;
    for (const auto& words : program.program.words)
    {
        hash = Fnv1a(hash, words.data(), 16);
    }
    hash = Fnv1a(hash, &program.declaration.bgra_inputs, sizeof(program.declaration.bgra_inputs));
    program.hash = hash;
    program.usable = true;
}

struct PixelShader
{
    std::vector<uint8_t> definition;
    psh::Definition words = {};
};

std::unordered_map<uint32_t, VertexShader> g_vertex_shaders;

// REXBOX_SHADER_DUMP=1: the declaration and the disassembled microcode of
// every vertex shader, and the D3DPIXELSHADERDEF of every pixel shader, the
// title creates (diagnostic).
bool ShaderDumpEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REXBOX_SHADER_DUMP");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

void DumpVertexShader(uint32_t handle, const VertexShader& shader)
{
    std::string text = "[d3d8] vertex shader " + std::to_string(handle) + " declaration:";
    char buffer[16];
    for (const uint32_t token : shader.declaration)
    {
        std::snprintf(buffer, sizeof(buffer), " %08X", token);
        text += buffer;
    }
    std::fprintf(stderr, "%s\n", text.c_str());
    runtime::GuestMemory& memory = Memory();
    if (shader.function == 0 || !memory.IsValid(shader.function, 4))
    {
        std::fprintf(stderr, "[d3d8]   no function (fixed-function vertex processing)\n");
        return;
    }
    const uint32_t header = memory.Read32(shader.function);
    const size_t size = 4 + static_cast<size_t>(header >> 16) * 16;
    vsh::Program program;
    std::string error;
    if (!memory.IsValid(shader.function, static_cast<uint32_t>(size)) ||
        !vsh::ReadProgram(memory.Translate(shader.function), size, &program, &error))
    {
        std::fprintf(stderr, "[d3d8]   header %08X: %s\n", header, error.c_str());
        return;
    }
    std::fprintf(stderr, "[d3d8]   header %08X: type %04X, %zu instruction(s)\n", header, program.type,
                 program.words.size());
    for (size_t i = 0; i < program.words.size(); ++i)
    {
        std::fprintf(stderr, "[d3d8]   %3zu %s\n", i, vsh::Disassemble(vsh::Decode(program.words[i].data())).c_str());
    }
}
std::unordered_map<uint32_t, PixelShader> g_pixel_shaders;
uint32_t g_next_pixel_shader = 1;
float g_vertex_constants[kVertexConstantCount][4] = {};
// Raised by every write to g_vertex_constants.
uint64_t g_vertex_constants_version = 1;
float g_pixel_constants[kPixelConstantCount][4] = {};

// HRESULT D3DDevice_CreateVertexShader(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage)
void D3DDevice_CreateVertexShader()
{
    HleCall call;
    const GuestAddress declaration = call.Arg(0);
    const GuestAddress function = call.Arg(1);
    const GuestAddress out = call.Arg(2);
    if (declaration == 0 || out == 0)
    {
        call.Return(kD3dErrInvalidCall, 4);
        return;
    }
    // The handle must be a valid guest pointer: a zeroed small block stands for
    // the object. Its 16-byte alignment leaves bit 0 free for the handle flag.
    const auto object = Heap().AllocateSmall(kVertexShaderObjectSize);
    if (!object)
    {
        call.Return(kD3dErrNotAvailable, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Fill(*object, 0, kVertexShaderObjectSize);
    VertexShader shader;
    for (uint32_t i = 0; i < kMaxDeclarationTokens; ++i)
    {
        const uint32_t token = memory.Read32(declaration + i * 4);
        shader.declaration.push_back(token);
        if (token == kDeclarationEnd)
        {
            break;
        }
    }
    shader.function = function;
    PrepareProgram(shader);
    const uint32_t handle = *object | kVertexShaderHandleFlag;
    if (ShaderDumpEnabled())
    {
        DumpVertexShader(handle, shader);
    }
    g_vertex_shaders[handle] = std::move(shader);
    memory.Write32(out, handle);
    call.Return(kD3dOk, 4);
}

// HRESULT D3DDevice_DeleteVertexShader(DWORD Handle)
void D3DDevice_DeleteVertexShader()
{
    HleCall call;
    const uint32_t handle = call.Arg(0);
    if (g_vertex_shaders.erase(handle) != 0)
    {
        Heap().Free(handle & ~kVertexShaderHandleFlag);
    }
    call.Return(kD3dOk, 1);
}

// HRESULT D3DDevice_LoadVertexShader(DWORD Handle, DWORD Address)
// Loads microcode into a slot of the GPU's program memory; the drawing batch
// resolves programs from the handle instead.
void D3DDevice_LoadVertexShader()
{
    HleCall call;
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_SelectVertexShader(DWORD Handle, DWORD Address)
void D3DDevice_SelectVertexShader()
{
    HleCall call;
    Pipeline().vertex_shader = call.Arg(0);
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_CreatePixelShader(const D3DPIXELSHADERDEF* pPSDef, DWORD* pHandle)
void D3DDevice_CreatePixelShader()
{
    HleCall call;
    const GuestAddress definition = call.Arg(0);
    const GuestAddress out = call.Arg(1);
    if (definition == 0 || out == 0)
    {
        call.Return(kD3dErrInvalidCall, 2);
        return;
    }
    PixelShader shader;
    shader.definition.assign(Memory().Translate(definition), Memory().Translate(definition) + kPixelShaderDefBytes);
    std::memcpy(shader.words.data(), shader.definition.data(), kPixelShaderDefBytes);
    const uint32_t handle = g_next_pixel_shader++;
    if (ShaderDumpEnabled())
    {
        std::string text = "[d3d8] pixel shader " + std::to_string(handle) + ":";
        char buffer[16];
        for (uint32_t i = 0; i < kPixelShaderDefBytes; i += 4)
        {
            uint32_t word = 0;
            std::memcpy(&word, shader.definition.data() + i, 4);
            std::snprintf(buffer, sizeof(buffer), " %08X", word);
            text += buffer;
        }
        std::fprintf(stderr, "%s\n", text.c_str());
    }
    g_pixel_shaders[handle] = std::move(shader);
    Memory().Write32(out, handle);
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_DeletePixelShader(DWORD Handle)
void D3DDevice_DeletePixelShader()
{
    HleCall call;
    g_pixel_shaders.erase(call.Arg(0));
    call.Return(kD3dOk, 1);
}

// HRESULT D3DDevice_SetVertexShaderConstant(INT Register, const void* pConstantData, DWORD ConstantCount)
void D3DDevice_SetVertexShaderConstant()
{
    HleCall call;
    const int32_t first = static_cast<int32_t>(call.Arg(0)) + kVertexConstantBase;
    const GuestAddress data = call.Arg(1);
    const uint32_t count = call.Arg(2);
    for (uint32_t i = 0; i < count; ++i)
    {
        const int32_t index = first + static_cast<int32_t>(i);
        if (index >= 0 && index < static_cast<int32_t>(kVertexConstantCount) && data != 0)
        {
            std::memcpy(g_vertex_constants[index], Memory().Translate(data + i * 16), 16);
        }
    }
    ++g_vertex_constants_version;
    call.Return(kD3dOk, 3);
}

// HRESULT D3DDevice_SetPixelShaderConstant(DWORD Register, const void* pConstantData, DWORD ConstantCount)
void D3DDevice_SetPixelShaderConstant()
{
    HleCall call;
    const uint32_t first = call.Arg(0);
    const GuestAddress data = call.Arg(1);
    const uint32_t count = call.Arg(2);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (first + i < kPixelConstantCount && data != 0)
        {
            std::memcpy(g_pixel_constants[first + i], Memory().Translate(data + i * 16), 16);
        }
    }
    call.Return(kD3dOk, 3);
}

}  // namespace

const ProgramShader* FindProgramShader(uint32_t handle)
{
    const auto it = g_vertex_shaders.find(handle);
    return it == g_vertex_shaders.end() ? nullptr : &it->second.program;
}

const float* VertexShaderConstants()
{
    return &g_vertex_constants[0][0];
}

uint64_t VertexShaderConstantsVersion()
{
    return g_vertex_constants_version;
}

const psh::Definition* FindPixelShader(uint32_t handle)
{
    const auto it = g_pixel_shaders.find(handle);
    return it == g_pixel_shaders.end() ? nullptr : &it->second.words;
}

const float (*PixelShaderConstants())[4]
{
    return g_pixel_constants;
}

void WriteViewportConstants(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    vsh::ViewportConstants(x, y, width, height, g_vertex_constants[58], g_vertex_constants[59]);
    ++g_vertex_constants_version;
}

void RegisterShaderExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_CreateVertexShader", &D3DDevice_CreateVertexShader);
    registry.AddExport("d3d8", "D3DDevice_DeleteVertexShader", &D3DDevice_DeleteVertexShader);
    registry.AddExport("d3d8", "D3DDevice_LoadVertexShader", &D3DDevice_LoadVertexShader);
    registry.AddExport("d3d8", "D3DDevice_SelectVertexShader", &D3DDevice_SelectVertexShader);
    registry.AddExport("d3d8", "D3DDevice_CreatePixelShader", &D3DDevice_CreatePixelShader);
    registry.AddExport("d3d8", "D3DDevice_DeletePixelShader", &D3DDevice_DeletePixelShader);
    registry.AddExport("d3d8", "D3DDevice_SetVertexShaderConstant", &D3DDevice_SetVertexShaderConstant);
    registry.AddExport("d3d8", "D3DDevice_SetPixelShaderConstant", &D3DDevice_SetPixelShaderConstant);
}

}  // namespace rexbox::hle::d3d8
