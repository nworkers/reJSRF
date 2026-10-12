// Shader objects and constants. The title's vertex shaders arrive as the XDK's
// declaration tokens and compiled microcode; the pixel shaders as a
// D3DPIXELSHADERDEF (register combiner setup). This batch keeps copies and
// hands out handles; the drawing batch translates them to GLSL.

#include "d3d8_internal.h"
#include "d3d8_state.h"

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
constexpr uint32_t kDeclarationEnd = 0xFFFFFFFF;
constexpr uint32_t kMaxDeclarationTokens = 256;
constexpr uint32_t kPixelShaderDefBytes = 0xBC;  // D3DPIXELSHADERDEF (inferred size)
constexpr int32_t kVertexConstantBase = 96;      // registers run -96..95 in the title's constant mode
constexpr uint32_t kVertexConstantCount = 192;
constexpr uint32_t kPixelConstantCount = 16;

struct VertexShader
{
    std::vector<uint32_t> declaration;
    GuestAddress function = 0;  // the microcode's guest address (copied by the drawing batch)
};

struct PixelShader
{
    std::vector<uint8_t> definition;
};

std::unordered_map<uint32_t, VertexShader> g_vertex_shaders;
std::unordered_map<uint32_t, PixelShader> g_pixel_shaders;
uint32_t g_next_pixel_shader = 1;
float g_vertex_constants[kVertexConstantCount][4] = {};
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
    // The handle must be a valid guest pointer: one zeroed page stands for the object.
    const auto object = Heap().Allocate(runtime::GuestHeap::kPageSize, runtime::GuestHeap::kPageSize, false);
    if (!object)
    {
        call.Return(kD3dErrNotAvailable, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Fill(*object, 0, runtime::GuestHeap::kPageSize);
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
    const uint32_t handle = *object | kVertexShaderHandleFlag;
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
    const uint32_t handle = g_next_pixel_shader++;
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
