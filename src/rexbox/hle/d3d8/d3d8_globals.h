// The D3D8 library globals a title's XDK inlines read and write directly
// (render-state and texture-stage arrays, stream table, index data, dirty
// flags). They are the source of truth for that state: the HLE setters write
// the same slots the original setters write, and drawing reads them back
// (docs/design/20261012-i023-d3d8-fixed-function-draw.md section 2).
//
// Addresses come from the identified XDK variables (HleRegistry::FindVariable);
// the slots XbSymbolDatabase does not name are offsets inside the XDK 4134
// library's data, measured from its lifted setters.

#ifndef REXBOX_HLE_D3D8_D3D8_GLOBALS_H
#define REXBOX_HLE_D3D8_D3D8_GLOBALS_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>

namespace rexbox::hle::d3d8
{

using runtime::GuestAddress;

namespace globals
{
// D3D_g_Stream: per stream {Stride, (unused here), VertexBuffer}.
constexpr uint32_t kStreamCount = 16;
constexpr uint32_t kStreamEntrySize = 12;
constexpr uint32_t kStreamStride = 0x0;
constexpr uint32_t kStreamVertexBuffer = 0x8;
// Measured from D3DDevice_SetVertexShader / SetIndices / the setters' OR of
// dirty bits (offsets from D3D_g_Stream).
constexpr uint32_t kFvfShaderOffset = 0xC0;   // the vertex-shader object the library builds for an FVF
constexpr uint32_t kIndexDataOffset = 0x1EC;  // D3D__IndexData: IndexBuffer->Data of SetIndices
constexpr uint32_t kDirtyFlagsOffset = 0x1F0;

// D3D_g_DeferredTextureState: [stage][slot], 32 DWORDs per stage.
constexpr uint32_t kTextureStages = 4;
constexpr uint32_t kTextureStateSlots = 32;

// D3D_g_RenderState: DWORDs by Xbox D3DRENDERSTATETYPE; the deferred block
// starts at 82 and the complex block at 117 (D3D_g_DeferredRenderState and
// D3D_g_ComplexRenderState); D3DRS_DoNotCullUncompressed (145) is the last
// named state.
constexpr uint32_t kRenderStateCount = 146;
constexpr uint32_t kDeferredRenderStateFirst = 82;
constexpr uint32_t kComplexRenderStateFirst = 117;

// Dirty bits the setters OR into the flags (the library consumes them in
// its draw path; mirrored so the game's own reads of the word stay as on
// the original).
constexpr uint32_t kDirtyStreams = 0x40;
constexpr uint32_t kDirtyVertexFormat = 0x70;
constexpr uint32_t kDirtyTexture = 0x4800;

// Device structure: the push-buffer position stamped into a resource's Lock
// field when the device drops a binding.
constexpr uint32_t kDevicePushFence = 0x30;
}  // namespace globals

// Resolves the variable addresses (once the title has bound them). False
// when the title's D3D8 library was not identified; the accessors then read
// 0 and drop writes.
bool LibraryGlobalsAvailable();

GuestAddress DeviceObject();  // *D3D_g_pDevice
void SetDeviceObject(GuestAddress device);

uint32_t RenderStateValue(uint32_t state);
void SetRenderStateValue(uint32_t state, uint32_t value);

uint32_t TextureStateValue(uint32_t stage, uint32_t slot);
void SetTextureStateValue(uint32_t stage, uint32_t slot, uint32_t value);

GuestAddress StreamVertexBuffer(uint32_t stream);
uint32_t StreamStride(uint32_t stream);
void SetStream(uint32_t stream, GuestAddress vertex_buffer, uint32_t stride);

GuestAddress IndexData();
void SetIndexData(GuestAddress data);

void OrDirtyFlags(uint32_t bits);

// The value the original stamps into Lock when a binding is dropped.
uint32_t DevicePushFence();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GLOBALS_H
