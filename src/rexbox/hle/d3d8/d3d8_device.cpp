// Device creation: the point where the Xbox library would initialize the
// NV2A (CMiniport_InitHardware). Here it opens the host display instead.

#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "rexbox/platform/display.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kDeviceObjectSize = 0x1000;

// REXBOX_DISPLAY=none runs without a window (headless diagnostics, CI).
bool DisplayDisabled()
{
    const char* value = std::getenv("REXBOX_DISPLAY");
    return value != nullptr && std::strcmp(value, "none") == 0;
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
    if (!device)
    {
        call.Return(kD3dErrNotAvailable, 6);
        return;
    }
    Memory().Fill(*device, 0, kDeviceObjectSize);
    state.device = *device;
    state.present = pp;
    Memory().Write32(out, state.device);
    call.Return(kD3dOk, 6);
}

}  // namespace

void RegisterDeviceExports(runtime::HleRegistry& registry)
{
    registry.AddExport("d3d8", "Direct3D_CreateDevice", &Direct3D_CreateDevice);
}

}  // namespace rexbox::hle::d3d8
