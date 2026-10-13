// The device: creation (where the Xbox library would initialize the NV2A, the
// HLE opens the host display), its back-buffer and depth surfaces, render
// targets, viewport, Clear and Swap.

#include "d3d8_gl.h"
#include "d3d8_gl_targets.h"
#include "d3d8_globals.h"
#include "d3d8_render_state.h"
#include "d3d8_internal.h"
#include "d3d8_shader.h"
#include "d3d8_resource.h"
#include "d3d8_state.h"
#include "d3d8_trace.h"
#include "d3d8_vblank.h"
#include "rexbox/platform/display.h"
#include "rexbox/runtime/environment.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// Large enough for the device fields the library's own routines touch (the
// identified m_VerticalBlankEvent is at +0x2430).
constexpr uint32_t kDeviceObjectSize = 0x3000;
constexpr const char* kInitializeDeviceStateName = "D3D_InitializeDeviceState";
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

// Processes window events. Closing the window (its X button, Alt+F4) asks the
// app to end the process; events are only seen here, when the device is
// created and at each present.
void PumpEvents()
{
    bool close = false;
    platform::PumpDisplayEvents(&close);
    D3dState& state = State();
    if (close && !state.close_requested)
    {
        state.close_requested = true;
        std::fprintf(stderr, "[d3d8] window close requested\n");
        runtime::RequestExit(0, "the window was closed");
    }
}

bool OpenDisplay(const PresentParameters& pp, std::string* error)
{
    platform::DisplayConfig config;
    config.width = pp.back_buffer_width;
    config.height = pp.back_buffer_height;
    config.title = "reJSRF";
    config.api = platform::GlApi::kDesktopGl;
    // REXBOX_DISPLAY=hidden: a GL context without a visible window (frame dumps in unattended runs).
    const char* display = std::getenv("REXBOX_DISPLAY");
    config.hidden = display != nullptr && std::strcmp(display, "hidden") == 0;
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
    SetGlIsEs(info.gl_es);
    std::fprintf(stderr, "[d3d8] display %ux%u, %s %d.%d, %s / %s\n", info.drawable_width, info.drawable_height,
                 info.gl_es ? "OpenGL ES" : "OpenGL", info.gl_major, info.gl_minor,
                 reinterpret_cast<const char*>(gl.GetString(kGlRenderer)),
                 reinterpret_cast<const char*>(gl.GetString(kGlVersion)));
    gl.Viewport(0, 0, static_cast<GLsizei>(info.drawable_width), static_cast<GLsizei>(info.drawable_height));
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(kGlColorBufferBit | kGlDepthBufferBit | kGlStencilBufferBit);
    platform::PresentDisplay();
    PumpEvents();
    return true;
}

// Applies the stored viewport to GL. D3D's origin is the top-left corner,
// GL's the bottom-left.
void ApplyViewport()
{
    D3dState& state = State();
    const Viewport& vp = state.viewport;
    WriteViewportConstants(vp.x, vp.y, vp.width, vp.height);
    if (!HaveGl())
    {
        return;
    }
    const uint32_t target_height = RenderTargetHeight();
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

// The original's hardware initialization ends by setting the default render,
// texture-stage and transform state through the public setters. The title
// profile binds that routine (it has no XbSymbolDatabase name); running it
// leaves the library globals as the original device would. ZEnable follows
// the depth buffer, as the routine's own D3DRS_ZENABLE call reads it from a
// device field the HLE does not keep.
void InitializeDeviceState(const D3dState& state)
{
    const GuestAddress routine = runtime::Env().registry->FindVariable(kInitializeDeviceStateName);
    if (routine == 0)
    {
        std::fprintf(stderr, "[d3d8] no %s binding; render and texture-stage state start empty\n",
                     kInitializeDeviceStateName);
        return;
    }
    bool called = false;
    runtime::Env().executor->CallGuest(routine, nullptr, 0, &called);
    if (!called)
    {
        std::fprintf(stderr, "[d3d8] %s (%08X) has no code; render state starts empty\n",
                     kInitializeDeviceStateName, routine);
    }
    SetRenderStateValue(rs::kZEnable, state.depth_stencil != 0 ? 1u : 0u);
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
    std::unique_lock<std::mutex> lock(state.mutex);
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
    SetVerticalBlankRate(pp.refresh_rate);

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
    // The library global the original sets; its own functions (and the
    // binding fences, d3d8_globals.h) read the device through it.
    SetDeviceObject(state.device);
    if (state.display_created)
    {
        BindRenderTarget(state.render_target, state.depth_target);
        ApplyViewport();
    }
    lock.unlock();
    InitializeDeviceState(state);
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
// A null render target keeps the current one (as on the Xbox). Surfaces
// other than the back buffers get their own GL texture (d3d8_gl_targets.cpp
// reports them; sampling them as textures comes later).
void D3DDevice_SetRenderTarget()
{
    HleCall call;
    D3dState& state = State();
    const GuestAddress target = call.Arg(0);
    const GuestAddress depth = call.Arg(1);
    if (target != 0)
    {
        state.render_target = target;
    }
    state.depth_target = depth;
    if (HaveGl())
    {
        BindRenderTarget(state.render_target, state.depth_target);
    }
    if (target != 0)
    {
        // The viewport resets to the whole new target.
        const LevelLayout layout = HeaderLevelLayout(target, 0);
        state.viewport = Viewport{0, 0, layout.width, layout.height, 0.0f, 1.0f};
        ApplyViewport();
    }
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
        gl.ColorMask((flags & kClearTargetR) != 0 ? 1 : 0, (flags & kClearTargetG) != 0 ? 1 : 0,
                     (flags & kClearTargetB) != 0 ? 1 : 0, (flags & kClearTargetA) != 0 ? 1 : 0);
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
    const uint32_t target_height = RenderTargetHeight();
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

// REXBOX_FRAME_DUMP=<n>[,<n>...]: writes the back buffer shown by the n-th
// present to frame_<n>.bmp in REXBOX_FRAME_DUMP_DIR (default: the current
// folder), to check output without watching the window (diagnostic).
void DumpFrameIfRequested(GuestAddress shown, uint32_t frame)
{
    static const std::vector<uint32_t> frames = [] {
        std::vector<uint32_t> list;
        if (const char* value = std::getenv("REXBOX_FRAME_DUMP"))
        {
            const char* cursor = value;
            while (*cursor != '\0')
            {
                char* end = nullptr;
                const unsigned long number = std::strtoul(cursor, &end, 10);
                if (end == cursor)
                {
                    break;
                }
                list.push_back(static_cast<uint32_t>(number));
                cursor = *end == ',' ? end + 1 : end;
            }
        }
        return list;
    }();
    if (std::find(frames.begin(), frames.end(), frame) == frames.end())
    {
        return;
    }
    const char* directory = std::getenv("REXBOX_FRAME_DUMP_DIR");
    std::string path = directory != nullptr ? std::string(directory) + "/" : std::string();
    path += "frame_" + std::to_string(frame) + ".bmp";
    if (DumpRenderTarget(shown, path, State().gamma_ramp))
    {
        std::fprintf(stderr, "[d3d8] frame %u written to %s\n", frame, path.c_str());
    }
    else
    {
        std::fprintf(stderr, "[d3d8] cannot write frame %u to %s\n", frame, path.c_str());
    }
}

// void D3DDevice_Swap(DWORD Flags)
void D3DDevice_Swap()
{
    HleCall call;
    D3dState& state = State();
    ++state.frame_count;
    bool presented = false;
    if (HaveGl())
    {
        // The back buffer drawn to is the one shown (the library rotates the
        // surfaces behind the same header); anything else shows back buffer 0.
        const HostResource* host = FindResource(state.render_target);
        const GuestAddress shown = host != nullptr && host->kind == ResourceKind::kBackBuffer
                                       ? state.render_target
                                       : state.back_buffers[0];
        DumpFrameIfRequested(shown, state.frame_count);
        WaitForPresentInterval(state.present.presentation_interval);
        PresentRenderTarget(shown, state.gamma_ramp);
        presented = true;
        PumpEvents();
    }
    if (!presented)
    {
        // Without a display the frame loop keeps the same pace.
        WaitForPresentInterval(state.present.presentation_interval);
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
// {BOOL InVBlank; UINT ScanLine}, from the virtual vertical-blank clock.
void D3DDevice_GetRasterStatus()
{
    HleCall call;
    const GuestAddress out = call.Arg(0);
    if (out != 0)
    {
        bool in_vblank = false;
        uint32_t scan_line = 0;
        RasterPosition(&in_vblank, &scan_line, State().present.back_buffer_height);
        Memory().Write32(out + 0, in_vblank ? 1 : 0);
        Memory().Write32(out + 4, scan_line);
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
    if (out != 0 && Memory().IsValid(out, GammaRamp::kBytes))
    {
        const GammaRamp& ramp = State().gamma_ramp;
        std::memcpy(Memory().Translate(out), ramp.bytes.data(), GammaRamp::kBytes);
    }
    call.Return(kD3dOk, 1);
}

// void D3DDevice_SetGammaRamp(DWORD Flags, const D3DGAMMARAMP* pRamp)
// The original copies the 768 bytes into the device and, with
// D3DSGR_IMMEDIATE (bit 0), writes the RAMDAC palette at once, otherwise at
// the next vertical blank. The screen changes only at Swap here, so both
// take effect at the next present.
void D3DDevice_SetGammaRamp()
{
    HleCall call;
    const GuestAddress ramp = call.Arg(1);
    if (ramp != 0 && Memory().IsValid(ramp, GammaRamp::kBytes))
    {
        GammaRamp& state_ramp = State().gamma_ramp;
        std::memcpy(state_ramp.bytes.data(), Memory().Translate(ramp), GammaRamp::kBytes);
        if (!state_ramp.IsIdentity())
        {
            TraceOnce(Format("gamma %02X%02X%02X", state_ramp.bytes[255], state_ramp.bytes[511], state_ramp.bytes[767]),
                      Format("gamma ramp at frame %u: red/green/blue[255] = %u/%u/%u, [128] = %u/%u/%u",
                             State().frame_count, state_ramp.bytes[255], state_ramp.bytes[511], state_ramp.bytes[767],
                             state_ramp.bytes[128], state_ramp.bytes[384], state_ramp.bytes[640]));
        }
    }
    call.Return(kD3dOk, 2);
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
        texture = Pipeline().textures[stage];
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
void D3DDevice_BlockUntilVerticalBlank()
{
    HleCall call;
    WaitForVerticalBlank();
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
    registry.AddExport("d3d8", "D3DDevice_SetGammaRamp", &D3DDevice_SetGammaRamp);
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
