// Drawing entry points. This batch accepts them without drawing (each is
// logged once with its arguments) so the title reaches its frame loop; the
// drawing batch replaces these with the GL pipeline.

#include "d3d8_internal.h"

#include <mutex>
#include <unordered_set>

namespace rexbox::hle::d3d8
{

namespace
{

std::mutex g_log_mutex;
std::unordered_set<const char*> g_logged;

void LogOnce(const char* name, const HleCall& call, int args)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_logged.insert(name).second)
    {
        return;
    }
    std::fprintf(stderr, "[d3d8] %s(", name);
    for (int i = 0; i < args; ++i)
    {
        std::fprintf(stderr, "%s%08X", i == 0 ? "" : ", ", call.Arg(i));
    }
    std::fprintf(stderr, ") accepted, not drawn yet\n");
}

// void D3DDevice_DrawVertices(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT VertexCount)
void D3DDevice_DrawVertices()
{
    HleCall call;
    LogOnce("D3DDevice_DrawVertices", call, 3);
    call.Return(kD3dOk, 3);
}

// void D3DDevice_DrawIndexedVertices(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount, const WORD* pIndexData)
void D3DDevice_DrawIndexedVertices()
{
    HleCall call;
    LogOnce("D3DDevice_DrawIndexedVertices", call, 3);
    call.Return(kD3dOk, 3);
}

// void D3DDevice_DrawVerticesUP(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount,
//     const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
void D3DDevice_DrawVerticesUP()
{
    HleCall call;
    LogOnce("D3DDevice_DrawVerticesUP", call, 4);
    call.Return(kD3dOk, 4);
}

// void D3DDevice_DrawIndexedVerticesUP(D3DPRIMITIVETYPE PrimitiveType, UINT VertexCount,
//     const WORD* pIndexData, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
void D3DDevice_DrawIndexedVerticesUP()
{
    HleCall call;
    LogOnce("D3DDevice_DrawIndexedVerticesUP", call, 5);
    call.Return(kD3dOk, 5);
}

// void D3DDevice_Begin(D3DPRIMITIVETYPE PrimitiveType)
void D3DDevice_Begin()
{
    HleCall call;
    LogOnce("D3DDevice_Begin", call, 1);
    call.Return(kD3dOk, 1);
}

// void D3DDevice_End(void)
void D3DDevice_End()
{
    HleCall call;
    call.Return(kD3dOk, 0);
}

// void D3DDevice_SetVertexData2f(int Register, float a, float b)
void D3DDevice_SetVertexData2f()
{
    HleCall call;
    call.Return(kD3dOk, 3);
}

// void D3DDevice_SetVertexData4f(int Register, float a, float b, float c, float d)
void D3DDevice_SetVertexData4f()
{
    HleCall call;
    call.Return(kD3dOk, 5);
}

// HRESULT D3DDevice_CopyRects(D3DSurface* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects,
//     D3DSurface* pDestinationSurface, const POINT* pDestPointsArray)
void D3DDevice_CopyRects()
{
    HleCall call;
    LogOnce("D3DDevice_CopyRects", call, 5);
    call.Return(kD3dOk, 5);
}

}  // namespace

void RegisterDrawExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "D3DDevice_DrawVertices", &D3DDevice_DrawVertices);
    registry.AddExport("d3d8", "D3DDevice_DrawIndexedVertices", &D3DDevice_DrawIndexedVertices);
    registry.AddExport("d3d8", "D3DDevice_DrawVerticesUP", &D3DDevice_DrawVerticesUP);
    registry.AddExport("d3d8", "D3DDevice_DrawIndexedVerticesUP", &D3DDevice_DrawIndexedVerticesUP);
    registry.AddExport("d3d8", "D3DDevice_Begin", &D3DDevice_Begin);
    registry.AddExport("d3d8", "D3DDevice_End", &D3DDevice_End);
    registry.AddExport("d3d8", "D3DDevice_SetVertexData2f", &D3DDevice_SetVertexData2f);
    registry.AddExport("d3d8", "D3DDevice_SetVertexData4f", &D3DDevice_SetVertexData4f);
    registry.AddExport("d3d8", "D3DDevice_CopyRects", &D3DDevice_CopyRects);
}

}  // namespace rexbox::hle::d3d8
