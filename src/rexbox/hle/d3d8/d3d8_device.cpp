// The device: creation (where the Xbox library would initialize the NV2A, the
// HLE opens the host display), its back-buffer and depth surfaces, render
// targets, viewport, Clear and Swap.

#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "d3d8_resource.h"
#include "d3d8_state.h"
#include "rexbox/platform/display.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kDeviceObjectSize = 0x1000;
constexpr uint32_t kBackBufferCount = 2;

// REXBOX_DISPLAY=none runs without a window (headless diagnostics, CI).
bool DisplayDisabled()
{
    if (State().headless)
    {
        return true;
    }
    const char* value = std::getenv("REXBOX_DISPLAY");
    return value != nullptr && std::strcmp(value, "none") == 0;
}

bool HaveGl()
{
    return State().display_created;
}

PresentParameters ReadPresentParameters(GuestAddress address)
{
    runtime::GuestMemory& memory = Memory();
    PresentParameters pp;
    pp.back_buffer_width = memory.Read32(address + layout::kPresentBackBufferWidth);
    pp.back_buffer_height = memory.Read32(address + layout::kPresentBackBufferHeight);
    pp.back_buffer_format = memory.Read32(address + layout::kPresentBackBufferFormat);
    pp.back_buffer_count = memory.Read32(address + layout::kPresentBackBufferCount);
    pp.multi_sample_type = memory.Read32(address + layout::kPresentMultiSampleType);
    pp.swap_effect = memory.Read32(address + layout::kPresentSwapEffect);
    pp.enable_auto_depth_stencil = memory.Read32(address + layout::kPresentEnableAutoDepthStencil);
    pp.auto_depth_stencil_format = memory.Read32(address + layout::kPresentAutoDepthStencilFormat);
    pp.flags = memory.Read32(address + layout::kPresentFlags);
    pp.refresh_rate = memory.Read32(address + layout::kPresentRefreshRate);
    pp.presentation_interval = memory.Read32(address + layout::kPresentPresentationInterval);
    return pp;
}

bool OpenDisplay(const PresentParameters& pp, std::string* error)
{
    platform::DisplayConfig config;
    config.width = pp.back_buffer_width;
    config.height = pp.back_buffer_height;
    config.title = "reJSRF";
    config.api = platform::GlApi::kDesktopGl;
    if (!platform::CreateDisplay(config, error))
    {
        return false;
    }
    GlFunctions gl;
    std::string missing;
    if (!LoadGlFunctions(&gl, &missing))
    {
        *error = "OpenGL entry point missing: " + missing;
        platform::DestroyDisplay();
        return false;
    }
    platform::DisplayInfo info;
    platform::GetDisplayInfo(&info);
    std::fprintf(stderr, "[d3d8] display %ux%u, %s %d.%d, %s / %s\n", info.drawable_width, info.drawable_height,
                 info.gl_es ? "OpenGL ES" : "OpenGL", info.gl_major, info.gl_minor,
                 reinterpret_cast<const char*>(gl.GetString(kGlRenderer)),
                 reinterpret_cast<const char*>(gl.GetString(kGlVersion)));
    gl.Viewport(0, 0, static_cast<GLsizei>(info.drawable_width), static_cast<GLsizei>(info.drawable_height));
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(kGlColorBufferBit | kGlDepthBufferBit | kGlStencilBufferBit);
    platform::PresentDisplay();
    platform::PumpDisplayEvents(nullptr);
    return true;
}

// Applies the stored viewport to GL. D3D's origin is the top-left corner,
// GL's the bottom-left.
void ApplyViewport()
{
    if (!HaveGl())
    {
        return;
    }
    D3dState& state = State();
    const Viewport& vp = state.viewport;
    const uint32_t target_height = state.present.back_buffer_height;
    const GLint y = static_cast<GLint>(target_height) - static_cast<GLint>(vp.y + vp.height);
    Gl().Viewport(static_cast<GLint>(vp.x), y, static_cast<GLsizei>(vp.width), static_cast<GLsizei>(vp.height));
    GlDepthRange(vp.min_z, vp.max_z);
}

// Creates the device's surfaces for the current presentation parameters.
bool CreateDeviceSurfaces(D3dState& state)
{
    const PresentParameters& pp = state.present;
    for (uint32_t i = 0; i < kBackBufferCount; ++i)
    {
        state.back_buffers[i] =
            CreateDeviceSurface(ResourceKind::kBackBuffer, pp.back_buffer_width, pp.back_buffer_height, pp.back_buffer_format);
        if (state.back_buffers[i] == 0)
        {
            return false;
        }
        FindResource(state.back_buffers[i])->back_buffer_index = static_cast<int>(i);
    }
    if (pp.enable_auto_depth_stencil != 0)
    {
        state.depth_stencil = CreateDeviceSurface(ResourceKind::kDepthStencil, pp.back_buffer_width,
                                                  pp.back_buffer_height, pp.auto_depth_stencil_format);
        if (state.depth_stencil == 0)
        {
            return false;
        }
    }
    state.render_target = state.back_buffers[0];
    state.depth_target = state.depth_stencil;
    state.viewport = Viewport{0, 0, pp.back_buffer_width, pp.back_buffer_height, 0.0f, 1.0f};
    return true;
}

// HRESULT Direct3D_CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
//     DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
//     IDirect3DDevice8** ppReturnedDeviceInterface)
void Direct3D_CreateDevice()
{
    HleCall call;
    const GuestAddress present_address = call.Arg(4);
    const GuestAddress out = call.Arg(5);
    if (call.Arg(0) != 0 || present_address == 0 || out == 0)
    {
        call.Return(kD3dErrInvalidCall, 6);
        return;
    }
    if (call.Arg(1) != kDevTypeHal)
    {
        call.Return(kD3dErrInvalidDevice, 6);
        return;
    }

    D3dState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.device != 0)
    {
        std::fprintf(stderr, "[d3d8] Direct3D_CreateDevice called again; the device already exists\n");
        Memory().Write32(out, state.device);
        call.Return(kD3dOk, 6);
        return;
    }

    PresentParameters pp = ReadPresentParameters(present_address);
    if (pp.back_buffer_width == 0 || pp.back_buffer_height == 0)
    {
        uint32_t count = 0;
        const DisplayMode* modes = AdapterModes(&count);
        pp.back_buffer_width = modes[0].width;
        pp.back_buffer_height = modes[0].height;
    }
    std::fprintf(stderr,
                 "[d3d8] Direct3D_CreateDevice: %ux%u format %02X, %u back buffer(s), multisample %04X, "
                 "depth %s format %02X, flags %08X, interval %u, behavior %08X\n",
                 pp.back_buffer_width, pp.back_buffer_height, pp.back_buffer_format, pp.back_buffer_count,
                 pp.multi_sample_type, pp.enable_auto_depth_stencil != 0 ? "on" : "off",
                 pp.auto_depth_stencil_format, pp.flags, pp.presentation_interval, call.Arg(3));
    state.present = pp;

    if (!DisplayDisabled())
    {
        std::string error;
        if (!OpenDisplay(pp, &error))
        {
            std::fprintf(stderr, "[d3d8] cannot open the display: %s\n", error.c_str());
            call.Return(kD3dErrNotAvailable, 6);
            return;
        }
        state.display_created = true;
    }

    const auto device = Heap().Allocate(kDeviceObjectSize, runtime::GuestHeap::kPageSize, false);
    if (!device || !CreateDeviceSurfaces(state))
    {
        call.Return(kD3dErrNotAvailable, 6);
        return;
    }
    Memory().Fill(*device, 0, kDeviceObjectSize);
    state.device = *device;
    Memory().Write32(out, state.device);
    call.Return(kD3dOk, 6);
}

// HRESULT D3DDevice_GetBackBuffer(INT BackBuffer, D3DBACKBUFFER_TYPE Type, D3DSurface** ppBackBuffer)
// The library indexes its surface array with 0 -> 0, -1 -> 1, anything else -> 2.
void D3DDevice_GetBackBuffer()
{
    HleCall call;
    const int32_t which = static_cast<int32_t>(call.Arg(0));
    const GuestAddress out = call.Arg(2);
    D3dState& state = State();
    const uint32_t index = which == 0 ? 0 : which == -1 ? 1 : 2;
    if (out == 0 || index >= kBackBufferCount || state.back_buffers[index] == 0)
    {
        call.Return(kD3dErrInvalidCall, 3);
        return;
    }
    Memory().Write32(out, state.back_buffers[index]);
    AddResourceRef(state.back_buffers[index]);
    call.Return(kD3dOk, 3);
}

// HRESULT D3DDevice_GetDepthStencilSurface(D3DSurface** ppZStencilSurface)
void D3DDevice_GetDepthStencilSurface()
{
    HleCall call;
    const GuestAddress out = call.Arg(0);
    D3dState& state = State();
    if (out != 0)
    {
        Memory().Write32(out, state.depth_stencil);
    }
    if (state.depth_stencil == 0)
    {
        call.Return(kD3dErrNotFound, 1);
        return;
    }
    AddResourceRef(state.depth_stencil);
    call.Return(kD3dOk, 1);
}

// HRESULT D3DDevice_SetRenderTarget(D3DSurface* pRenderTarget, D3DSurface* pNewZStencil)
// A null render target keeps the current one (as on the Xbox); only the
// device's own surfaces can be drawn to so far.
void D3DDevice_SetRenderTarget()
{
    HleCall call;
    D3dState& state = State();
    const GuestAddress target = call.Arg(0);
    const GuestAddress depth = call.Arg(1);
    if (target != 0)
    {
        const HostResource* host = FindResource(target);
        if (host == nullptr || host->kind != ResourceKind::kBackBuffer)
        {
            static bool warned = false;
            if (!warned)
            {
                warned = true;
                std::fprintf(stderr, "[d3d8] SetRenderTarget to %08X: only the back buffers can be targets yet\n",
                             target);
            }
        }
        state.render_target = target;
        state.viewport = Viewport{0, 0, state.present.back_buffer_width, state.present.back_buffer_height, 0.0f, 1.0f};
        ApplyViewport();
    }
    state.depth_target = depth;
    call.Return(kD3dOk, 2);
}

// HRESULT D3DDevice_Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
// The game's parameters are the ones the device was created with; a size or
// format change would need new surfaces and is reported.
void D3DDevice_Reset()
{
    HleCall call;
    D3dState& state = State();
    const PresentParameters pp = ReadPresentParameters(call.Arg(0));
    if (pp.back_buffer_width != state.present.back_buffer_width ||
        pp.back_buffer_height != state.present.back_buffer_height ||
        pp.back_buffer_format != state.present.back_buffer_format)
    {
        std::fprintf(stderr, "[d3d8] D3DDevice_Reset to %ux%u format %02X: changing the mode is not supported yet\n",
                     pp.back_buffer_width, pp.back_buffer_height, pp.back_buffer_format);
    }
    state.present = pp;
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetViewport(const D3DVIEWPORT8* pViewport)
void D3DDevice_SetViewport()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const GuestAddress address = call.Arg(0);
    if (address != 0)
    {
        Viewport vp;
        vp.x = memory.Read32(address + layout::kViewportX);
        vp.y = memory.Read32(address + layout::kViewportY);
        vp.width = memory.Read32(address + layout::kViewportWidth);
        vp.height = memory.Read32(address + layout::kViewportHeight);
        uint32_t bits = memory.Read32(address + layout::kViewportMinZ);
        std::memcpy(&vp.min_z, &bits, 4);
        bits = memory.Read32(address + layout::kViewportMaxZ);
        std::memcpy(&vp.max_z, &bits, 4);
        State().viewport = vp;
        ApplyViewport();
    }
    call.Return(kD3dOk, 1);
}

// void D3DDevice_Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
void D3DDevice_Clear()
{
    HleCall call;
    const uint32_t count = call.Arg(0);
    const GuestAddress rects = call.Arg(1);
    const uint32_t flags = call.Arg(2);
    const uint32_t color = call.Arg(3);
    uint32_t z_bits = call.Arg(4);
    float z = 1.0f;
    std::memcpy(&z, &z_bits, 4);
    const uint32_t stencil = call.Arg(5);
    if (!HaveGl())
    {
        call.Return(kD3dOk, 6);
        return;
    }
    GlFunctions& gl = Gl();
    GLbitfield mask = 0;
    if ((flags & kClearTarget) != 0)
    {
        gl.ColorMask(1, 1, 1, 1);
        gl.ClearColor(((color >> 16) & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, (color & 0xFF) / 255.0f,
                      ((color >> 24) & 0xFF) / 255.0f);
        mask |= kGlColorBufferBit;
    }
    if ((flags & kClearZBuffer) != 0)
    {
        gl.DepthMask(1);
        GlClearDepth(z);
        mask |= kGlDepthBufferBit;
    }
    if ((flags & kClearStencil) != 0)
    {
        gl.StencilMask(0xFFFFFFFFu);
        gl.ClearStencil(static_cast<GLint>(stencil));
        mask |= kGlStencilBufferBit;
    }
    if (mask == 0)
    {
        call.Return(kD3dOk, 6);
        return;
    }
    const uint32_t target_height = State().present.back_buffer_height;
    if (count == 0 || rects == 0)
    {
        gl.Disable(kGlScissorTest);
        gl.Clear(mask);
    }
    else
    {
        runtime::GuestMemory& memory = Memory();
        gl.Enable(kGlScissorTest);
        for (uint32_t i = 0; i < count; ++i)
        {
            const GuestAddress rect = rects + i * 16;
            const int32_t x1 = static_cast<int32_t>(memory.Read32(rect + layout::kRectX1));
            const int32_t y1 = static_cast<int32_t>(memory.Read32(rect + layout::kRectY1));
            const int32_t x2 = static_cast<int32_t>(memory.Read32(rect + layout::kRectX2));
            const int32_t y2 = static_cast<int32_t>(memory.Read32(rect + layout::kRectY2));
            gl.Scissor(x1, static_cast<GLint>(target_height) - y2, x2 - x1, y2 - y1);
            gl.Clear(mask);
        }
        gl.Disable(kGlScissorTest);
    }
    call.Return(kD3dOk, 6);
}

// void D3DDevice_Swap(DWORD Flags)
void D3DDevice_Swap()
{
    HleCall call;
    D3dState& state = State();
    ++state.frame_count;
    if (HaveGl())
    {
        platform::PresentDisplay();
        bool close = false;
        platform::PumpDisplayEvents(&close);
        if (close && !state.close_requested)
        {
            state.close_requested = true;
            std::fprintf(stderr, "[d3d8] window close requested\n");
        }
    }
    if (state.frame_count == 1)
    {
        std::fprintf(stderr, "[d3d8] first frame presented (D3DDevice_Swap flags %08X)\n", call.Arg(0));
    }
    // REXBOX_FRAME_LOG=<n>: one line every n presents, to tell a running
    // frame loop from a stall without a full trace (diagnostic).
    static const uint32_t frame_log = [] {
        const char* value = std::getenv("REXBOX_FRAME_LOG");
        return value != nullptr ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 0u;
    }();
    if (frame_log != 0 && state.frame_count % frame_log == 0)
    {
        static const auto start = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        std::fprintf(stderr, "[d3d8] frame %u (+%lld ms since the first logged)\n", state.frame_count,
                     static_cast<long long>(elapsed.count()));
    }
    call.Return(kD3dOk, 1);
}

// ULONG D3DDevice_AddRef(void) / D3DDevice_Release(void): the device's own
// reference count (device + 0x43C in the library); nothing is destroyed here.
void D3DDevice_AddRef()
{
    HleCall call;
    call.Return(++State().device_ref_count, 0);
}

void D3DDevice_Release()
{
    HleCall call;
    D3dState& state = State();
    if (state.device_ref_count > 0)
    {
        --state.device_ref_count;
    }
    call.Return(state.device_ref_count, 0);
}

// void D3DDevice_GetRasterStatus(D3DRASTER_STATUS* pRasterStatus)
// {BOOL InVBlank; UINT ScanLine}. The host has no scan-out position; the
// answer alternates so a title polling for the vertical blank proceeds.
void D3DDevice_GetRasterStatus()
{
    HleCall call;
    const GuestAddress out = call.Arg(0);
    const uint32_t calls = ++State().raster_status_calls;
    if (out != 0)
    {
        const bool in_vblank = (calls & 1) != 0;
        Memory().Write32(out + 0, in_vblank ? 1 : 0);
        Memory().Write32(out + 4, in_vblank ? 0 : (calls * 37) % 480);
    }
    call.Return(kD3dOk, 1);
}

// BOOL D3DDevice_IsBusy(void): the GPU never lags the HLE.
void D3DDevice_IsBusy()
{
    HleCall call;
    call.Return(0, 0);
}

// void D3DDevice_GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix)
void D3DDevice_GetTransform()
{
    HleCall call;
    const uint32_t which = call.Arg(0);
    const GuestAddress out = call.Arg(1);
    if (out != 0 && which < kMaxTransforms)
    {
        std::memcpy(Memory().Translate(out), Pipeline().transforms[which].data(), 64);
    }
    call.Return(kD3dOk, 2);
}

// void D3DDevice_GetGammaRamp(D3DGAMMARAMP* pRamp): identity, 3 x 256 bytes.
void D3DDevice_GetGammaRamp()
{
    HleCall call;
    const GuestAddress out = call.Arg(0);
    if (out != 0)
    {
        for (uint32_t channel = 0; channel < 3; ++channel)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                Memory().Write8(out + channel * 256 + i, static_cast<uint8_t>(i));
            }
        }
    }
    call.Return(kD3dOk, 1);
}

// void D3DDevice_GetTexture2(DWORD Stage, D3DBaseTexture** ppTexture): the
// bound texture, AddRef'd (the lifted original pops two arguments).
void D3DDevice_GetTexture2()
{
    HleCall call;
    const uint32_t stage = call.Arg(0);
    const GuestAddress out = call.Arg(1);
    GuestAddress texture = 0;
    if (stage < kMaxTextureStages)
    {
        texture = Pipeline().stages[stage].texture;
    }
    if (texture != 0)
    {
        AddResourceRef(texture);
    }
    if (out != 0)
    {
        Memory().Write32(out, texture);
    }
    call.Return(texture, 2);
}

// void D3DDevice_SetTile(DWORD Index, const D3DTILE* pTile) / GetTile: the
// NV2A's tiled-memory registers, kept as plain values.
void D3DDevice_SetTile()
{
    HleCall call;
    const uint32_t index = call.Arg(0);
    const GuestAddress tile = call.Arg(1);
    if (index < 8 && tile != 0)
    {
        for (uint32_t i = 0; i < 6; ++i)
        {
            State().tiles[index][i] = Memory().Read32(tile + i * 4);
        }
    }
    call.Return(kD3dOk, 2);
}

void D3DDevice_GetTile()
{
    HleCall call;
    const uint32_t index = call.Arg(0);
    const GuestAddress tile = call.Arg(1);
    if (index < 8 && tile != 0)
    {
        for (uint32_t i = 0; i < 6; ++i)
        {
            Memory().Write32(tile + i * 4, State().tiles[index][i]);
        }
    }
    call.Return(kD3dOk, 2);
}

// DWORD D3D_SetFence(DWORD Flags): a fence the title may wait on; fences
// complete immediately here.
void D3D_SetFence()
{
    HleCall call;
    call.Return(++State().fence_counter, 1);
}

// Functions with no arguments whose work (push buffer, GPU idle) does not exist here.
void AcceptNone()
{
    HleCall call;
    call.Return(kD3dOk, 0);
}

// Setters whose effect is a display property the host does not reproduce;
// the value is accepted and dropped.
void AcceptOneArgument()
{
    HleCall call;
    call.Return(kD3dOk, 1);
}

void AcceptTwoArguments()
{
    HleCall call;
    call.Return(kD3dOk, 2);
}

// void D3DDevice_BlockUntilVerticalBlank(void)
// Presentation is synchronized to the host's vertical blank in Swap.
void D3DDevice_BlockUntilVerticalBlank()
{
    HleCall call;
    call.Return(kD3dOk, 0);
}

}  // namespace

void RegisterDeviceExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "Direct3D_CreateDevice", &Direct3D_CreateDevice);
    registry.AddExport("d3d8", "D3DDevice_GetBackBuffer", &D3DDevice_GetBackBuffer);
    registry.AddExport("d3d8", "D3DDevice_GetDepthStencilSurface", &D3DDevice_GetDepthStencilSurface);
    registry.AddExport("d3d8", "D3DDevice_SetRenderTarget", &D3DDevice_SetRenderTarget);
    registry.AddExport("d3d8", "D3DDevice_Reset", &D3DDevice_Reset);
    registry.AddExport("d3d8", "D3DDevice_SetViewport", &D3DDevice_SetViewport);
    registry.AddExport("d3d8", "D3DDevice_Clear", &D3DDevice_Clear);
    registry.AddExport("d3d8", "D3DDevice_Swap", &D3DDevice_Swap);
    registry.AddExport("d3d8", "D3DDevice_BlockUntilVerticalBlank", &D3DDevice_BlockUntilVerticalBlank);
    registry.AddExport("d3d8", "D3DDevice_SetVerticalBlankCallback", &AcceptOneArgument);
    registry.AddExport("d3d8", "D3DDevice_SetFlickerFilter", &AcceptOneArgument);
    registry.AddExport("d3d8", "D3DDevice_SetSoftDisplayFilter", &AcceptOneArgument);
    registry.AddExport("d3d8", "D3DDevice_SetGammaRamp", &AcceptTwoArguments);
    registry.AddExport("d3d8", "D3DDevice_GetGammaRamp", &D3DDevice_GetGammaRamp);
    registry.AddExport("d3d8", "D3DDevice_AddRef", &D3DDevice_AddRef);
    registry.AddExport("d3d8", "D3DDevice_Release", &D3DDevice_Release);
    registry.AddExport("d3d8", "D3DDevice_GetRasterStatus", &D3DDevice_GetRasterStatus);
    registry.AddExport("d3d8", "D3DDevice_IsBusy", &D3DDevice_IsBusy);
    registry.AddExport("d3d8", "D3DDevice_GetTransform", &D3DDevice_GetTransform);
    registry.AddExport("d3d8", "D3DDevice_GetTexture2", &D3DDevice_GetTexture2);
    registry.AddExport("d3d8", "D3DDevice_SetTile", &D3DDevice_SetTile);
    registry.AddExport("d3d8", "D3DDevice_GetTile", &D3DDevice_GetTile);
    registry.AddExport("d3d8", "D3DDevice_LightEnable", &AcceptTwoArguments);
    registry.AddExport("d3d8", "D3D_SetPushBufferSize", &AcceptTwoArguments);
    registry.AddExport("d3d8", "D3D_SetFence", &D3D_SetFence);
    registry.AddExport("d3d8", "D3D_BlockOnResource", &AcceptOneArgument);
    registry.AddExport("d3d8", "D3D_CommonSetDebugRegisters", &AcceptNone);
    registry.AddExport("d3d8", "D3D_KickOffAndWaitForIdle", &AcceptNone);
    registry.AddExport("d3d8", "D3DDevice_MakeSpace", &AcceptNone);
}

}  // namespace rexbox::hle::d3d8
