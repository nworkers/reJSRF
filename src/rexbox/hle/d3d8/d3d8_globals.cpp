#include "d3d8_globals.h"

#include "d3d8_internal.h"

#include <atomic>

namespace rexbox::hle::d3d8
{

namespace
{

// XbSymbolDatabase names (gen/<title>/xdk_symbols.json).
constexpr const char* kDeviceVariable = "D3D_g_pDevice";
constexpr const char* kStreamVariable = "D3D_g_Stream";
constexpr const char* kRenderStateVariable = "D3D_g_RenderState";
constexpr const char* kTextureStateVariable = "D3D_g_DeferredTextureState";

struct Addresses
{
    GuestAddress device = 0;
    GuestAddress stream = 0;
    GuestAddress render_state = 0;
    GuestAddress texture_state = 0;
};

// Looked up on first use after the title bound its variables; kept once found.
const Addresses& Resolve()
{
    static Addresses addresses;
    static std::atomic<bool> resolved{false};
    if (!resolved.load(std::memory_order_acquire))
    {
        runtime::HleRegistry* registry = runtime::Env().registry;
        if (registry != nullptr)
        {
            Addresses found;
            found.device = registry->FindVariable(kDeviceVariable);
            found.stream = registry->FindVariable(kStreamVariable);
            found.render_state = registry->FindVariable(kRenderStateVariable);
            found.texture_state = registry->FindVariable(kTextureStateVariable);
            if (found.device != 0 && found.stream != 0 && found.render_state != 0 && found.texture_state != 0)
            {
                addresses = found;
                resolved.store(true, std::memory_order_release);
            }
        }
    }
    return addresses;
}

}  // namespace

bool LibraryGlobalsAvailable()
{
    return Resolve().device != 0;
}

GuestAddress DeviceObject()
{
    const GuestAddress slot = Resolve().device;
    return slot != 0 ? Memory().Read32(slot) : State().device;
}

void SetDeviceObject(GuestAddress device)
{
    if (const GuestAddress slot = Resolve().device)
    {
        Memory().Write32(slot, device);
    }
}

uint32_t RenderStateValue(uint32_t state)
{
    const GuestAddress base = Resolve().render_state;
    return base != 0 && state < globals::kRenderStateCount ? Memory().Read32(base + state * 4) : 0;
}

void SetRenderStateValue(uint32_t state, uint32_t value)
{
    const GuestAddress base = Resolve().render_state;
    if (base != 0 && state < globals::kRenderStateCount)
    {
        Memory().Write32(base + state * 4, value);
    }
}

uint32_t TextureStateValue(uint32_t stage, uint32_t slot)
{
    const GuestAddress base = Resolve().texture_state;
    if (base == 0 || stage >= globals::kTextureStages || slot >= globals::kTextureStateSlots)
    {
        return 0;
    }
    return Memory().Read32(base + (stage * globals::kTextureStateSlots + slot) * 4);
}

void SetTextureStateValue(uint32_t stage, uint32_t slot, uint32_t value)
{
    const GuestAddress base = Resolve().texture_state;
    if (base != 0 && stage < globals::kTextureStages && slot < globals::kTextureStateSlots)
    {
        Memory().Write32(base + (stage * globals::kTextureStateSlots + slot) * 4, value);
    }
}

GuestAddress StreamVertexBuffer(uint32_t stream)
{
    const GuestAddress base = Resolve().stream;
    return base != 0 && stream < globals::kStreamCount
               ? Memory().Read32(base + stream * globals::kStreamEntrySize + globals::kStreamVertexBuffer)
               : 0;
}

uint32_t StreamStride(uint32_t stream)
{
    const GuestAddress base = Resolve().stream;
    return base != 0 && stream < globals::kStreamCount
               ? Memory().Read32(base + stream * globals::kStreamEntrySize + globals::kStreamStride)
               : 0;
}

void SetStream(uint32_t stream, GuestAddress vertex_buffer, uint32_t stride)
{
    const GuestAddress base = Resolve().stream;
    if (base != 0 && stream < globals::kStreamCount)
    {
        const GuestAddress entry = base + stream * globals::kStreamEntrySize;
        Memory().Write32(entry + globals::kStreamStride, stride);
        Memory().Write32(entry + globals::kStreamVertexBuffer, vertex_buffer);
    }
}

GuestAddress IndexData()
{
    const GuestAddress base = Resolve().stream;
    return base != 0 ? Memory().Read32(base + globals::kIndexDataOffset) : 0;
}

void SetIndexData(GuestAddress data)
{
    if (const GuestAddress base = Resolve().stream)
    {
        Memory().Write32(base + globals::kIndexDataOffset, data);
    }
}

void OrDirtyFlags(uint32_t bits)
{
    if (const GuestAddress base = Resolve().stream)
    {
        const GuestAddress flags = base + globals::kDirtyFlagsOffset;
        Memory().Write32(flags, Memory().Read32(flags) | bits);
    }
}

uint32_t DevicePushFence()
{
    const GuestAddress device = DeviceObject();
    return device != 0 ? Memory().Read32(device + globals::kDevicePushFence) : 0;
}

}  // namespace rexbox::hle::d3d8
