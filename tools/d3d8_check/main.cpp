// rejsrf_d3d8_check: runs each D3D8 HLE export and the lifted XDK original it
// replaces (sub_X_gen) from identical guest states and compares eax, esp and
// the bytes of a scratch area (output buffers, resource headers). The
// adapter, mode, format and caps queries and the resource-header functions
// never touch the GPU, so their originals run unchanged on top of the kernel
// HLE (docs/design/20261012-i007-d3d8-hle-first-batch.md section 4,
// docs/design/20261012-i012-d3d8-surfaces-first-frame.md section 4).
//
// Usage: rejsrf_d3d8_check [--dump] [<path to default_eng.xbe>]
//   --dump   print the originals' answers (mode table, caps, format verdicts)
// Exit code 0 when every case matches.

#include "rejsrf/jsrf_profile.h"
#include "rexbox/hle/d3d8/d3d8_hle.h"
#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/recomp/recomp_abi.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/xbe_loader.h"
#include "rexbox/xbe/xbe_image.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Lifted originals of the US build's D3D8 functions (gen/jsrf_us/xdk_symbols.json).
extern "C" void sub_00199820_gen(void);  // Direct3D_GetAdapterCount
extern "C" void sub_00199830_gen(void);  // D3D_GetAdapterModeCount
extern "C" void sub_001998D0_gen(void);  // D3D_EnumAdapterModes
extern "C" void sub_00199A60_gen(void);  // D3D_CheckDeviceFormat
extern "C" void sub_00199AE0_gen(void);  // Direct3D_CheckDeviceMultiSampleType
extern "C" void sub_00199A30_gen(void);  // D3D_GetDeviceCaps
extern "C" void sub_0018CB40_gen(void);  // D3DDevice_GetDeviceCaps
extern "C" void sub_00192950_gen(void);  // D3DResource_AddRef
extern "C" void sub_00192990_gen(void);  // D3DResource_Release
extern "C" void sub_00192790_gen(void);  // D3DResource_GetType
extern "C" void sub_0018E730_gen(void);  // D3DSurface_GetDesc
extern "C" void sub_001929E0_gen(void);  // D3DResource_Register
// State setters (issue #23): compared by the library globals they leave behind.
extern "C" void sub_0018DF10_gen(void);  // D3DDevice_SetTexture
extern "C" void sub_0018E090_gen(void);  // D3DDevice_SetIndices
extern "C" void sub_0018E8A0_gen(void);  // D3DDevice_SetRenderState_MultiSampleMode
extern "C" void sub_0018E8D0_gen(void);  // D3DDevice_SetRenderState_MultiSampleRenderTargetMode
extern "C" void sub_0018E900_gen(void);  // D3DDevice_SetRenderState_PSTextureModes
extern "C" void sub_0018E960_gen(void);  // D3DDevice_SetRenderStateNotInline
extern "C" void sub_0018EB00_gen(void);  // D3DDevice_SetRenderState_EdgeAntiAlias
extern "C" void sub_0018EB40_gen(void);  // D3DDevice_SetRenderState_ShadowFunc
extern "C" void sub_0018EB80_gen(void);  // D3DDevice_SetRenderState_FogColor
extern "C" void sub_0018EBD0_gen(void);  // D3DDevice_SetRenderState_CullMode
extern "C" void sub_0018EC40_gen(void);  // D3DDevice_SetRenderState_FrontFace
extern "C" void sub_0018EC80_gen(void);  // D3DDevice_SetRenderState_NormalizeNormals
extern "C" void sub_0018ECC0_gen(void);  // D3DDevice_SetRenderState_TextureFactor
extern "C" void sub_0018ED20_gen(void);  // D3DDevice_SetRenderState_LineWidth
extern "C" void sub_0018ED90_gen(void);  // D3DDevice_SetRenderState_Dxt1NoiseEnable
extern "C" void sub_0018EE10_gen(void);  // D3DDevice_SetRenderState_ZBias
extern "C" void sub_0018EE90_gen(void);  // D3DDevice_SetRenderState_LogicOp
extern "C" void sub_0018EEF0_gen(void);  // D3DDevice_SetRenderState_FillMode
extern "C" void sub_0018EF40_gen(void);  // D3DDevice_SetRenderState_BackFillMode
extern "C" void sub_0018EFA0_gen(void);  // D3DDevice_SetRenderState_TwoSidedLighting
extern "C" void sub_0018F010_gen(void);  // D3DDevice_SetRenderState_VertexBlend
extern "C" void sub_0018F060_gen(void);  // D3DDevice_SetTextureState_TexCoordIndex
extern "C" void sub_0018F180_gen(void);  // D3DDevice_SetTextureState_BumpEnv
extern "C" void sub_0018F1F0_gen(void);  // D3DDevice_SetTextureState_BorderColor
extern "C" void sub_0018F230_gen(void);  // D3DDevice_SetTextureState_ColorKeyColor
extern "C" void sub_0018F270_gen(void);  // D3DDevice_SetTextureStageStateNotInline
extern "C" void sub_0018F6C0_gen(void);  // D3DDevice_SetRenderState_ZEnable
extern "C" void sub_0018F760_gen(void);  // D3DDevice_SetRenderState_StencilEnable
extern "C" void sub_0018F7F0_gen(void);  // D3DDevice_SetRenderState_StencilFail
extern "C" void sub_0018F860_gen(void);  // D3DDevice_SetRenderState_YuvEnable
extern "C" void sub_0018F890_gen(void);  // D3DDevice_SetRenderState_OcclusionCullEnable
extern "C" void sub_0018F900_gen(void);  // D3DDevice_SetRenderState_StencilCullEnable
extern "C" void sub_0018F970_gen(void);  // D3DDevice_SetRenderState_RopZCmpAlwaysRead
extern "C" void sub_0018F990_gen(void);  // D3DDevice_SetRenderState_RopZRead
extern "C" void sub_0018F9B0_gen(void);  // D3DDevice_SetRenderState_DoNotCullUncompressed
extern "C" void sub_0018F9D0_gen(void);  // D3DDevice_SetRenderState_MultiSampleAntiAlias
extern "C" void sub_0018FA30_gen(void);  // D3DDevice_SetRenderState_MultiSampleMask
extern "C" void sub_001900D0_gen(void);  // D3DDevice_SetStreamSource
// XGRAPHC is not an HLE library; its lifted code is the function itself.
extern "C" void sub_001BBCA5(void);  // XGSetSurfaceHeader(Width, Height, Format, pSurface, Data, Pitch)

namespace
{

using rexbox::runtime::GuestAddress;
using rexbox::runtime::GuestMemory;
using GuestFunction = void (*)();
using Setup = std::function<void()>;

constexpr const char* kDefaultXbe = "roms/Jet Set Radio Future/default_eng.xbe";
constexpr uint32_t kOutputBytes = 0x200;  // generous; the largest output is 53 DWORDs
constexpr uint8_t kFillPattern = 0xCC;

// Resource headers used by the header cases live inside the scratch area so
// their changes are compared too.
constexpr uint32_t kHeaderA = 0x100;  // offsets into the scratch area
constexpr uint32_t kHeaderB = 0x140;
constexpr uint32_t kHeaderSize = 0x18;

struct Subject
{
    const char* name;
    GuestFunction original;
    GuestFunction hle;
};

struct Outcome
{
    uint32_t eax = 0;
    uint32_t esp = 0;
    std::vector<uint8_t> output;
};

GuestMemory* g_memory = nullptr;
GuestAddress g_stack_top = 0;
GuestAddress g_output = 0;
bool g_dump = false;
std::string g_note;  // appended to DIFF lines by cases that set it
int g_failures = 0;
int g_cases = 0;

void Write32(GuestAddress address, uint32_t value)
{
    g_memory->Write32(address, value);
}

// Calls `function` as a guest `call` would, with `args` on the stack and the
// scratch area filled with a pattern (then prepared by `setup`), and collects
// what it left behind.
Outcome Run(GuestFunction function, const std::vector<uint32_t>& args, const Setup& setup = nullptr,
            bool fill_scratch = true)
{
    if (fill_scratch)
    {
        g_memory->Fill(g_output, kFillPattern, kOutputBytes);
    }
    if (setup)
    {
        setup();
    }
    uint32_t esp = g_stack_top;
    for (size_t i = args.size(); i-- > 0;)
    {
        esp -= 4;
        Write32(esp, args[i]);
    }
    esp -= 4;
    Write32(esp, 0);  // return address
    rexbox_recomp_reset_thread(esp, 0);
    RexboxRecompGprs gprs = {};
    gprs.esp = esp;
    rexbox_recomp_set_gprs(&gprs);
    function();
    Outcome outcome;
    rexbox_recomp_get_gprs(&gprs);
    outcome.eax = gprs.eax;
    outcome.esp = gprs.esp;
    outcome.output.assign(g_memory->Translate(g_output), g_memory->Translate(g_output) + kOutputBytes);
    return outcome;
}

std::string Describe(const std::vector<uint32_t>& args)
{
    std::string text;
    char buffer[16];
    for (const uint32_t arg : args)
    {
        std::snprintf(buffer, sizeof(buffer), "%s%X", text.empty() ? "" : ", ", arg);
        text += buffer;
    }
    return text;
}

// Compares the HLE with the original for one argument list; returns the
// original's outcome for dumping.
Outcome Check(const Subject& subject, const std::vector<uint32_t>& args, const Setup& setup = nullptr)
{
    const Outcome original = Run(subject.original, args, setup);
    const Outcome hle = Run(subject.hle, args, setup);
    ++g_cases;
    bool same = true;
    if (original.eax != hle.eax)
    {
        std::printf("DIFF %s(%s)%s: eax original %08X hle %08X\n", subject.name, Describe(args).c_str(),
                    g_note.c_str(), original.eax, hle.eax);
        same = false;
    }
    if (original.esp != hle.esp)
    {
        std::printf("DIFF %s(%s): esp original %08X hle %08X\n", subject.name, Describe(args).c_str(), original.esp,
                    hle.esp);
        same = false;
    }
    if (original.output != hle.output)
    {
        size_t first = 0;
        while (original.output[first] == hle.output[first])
        {
            ++first;
        }
        std::printf("DIFF %s(%s): scratch differs first at +%zX (original %02X hle %02X)\n", subject.name,
                    Describe(args).c_str(), first, original.output[first], hle.output[first]);
        same = false;
    }
    g_failures += same ? 0 : 1;
    return original;
}

// Bytes of the output buffer the function wrote (the pattern is gone).
size_t WrittenBytes(const Outcome& outcome, size_t limit = kOutputBytes)
{
    size_t end = limit;
    while (end > 0 && outcome.output[end - 1] == kFillPattern)
    {
        --end;
    }
    return end;
}

char VerdictChar(uint32_t hresult)
{
    switch (hresult)
    {
    case 0x00000000:
        return 'O';  // D3D_OK
    case 0x8876086A:
        return 'N';  // D3DERR_NOTAVAILABLE
    case 0x8876086B:
        return 'D';  // D3DERR_INVALIDDEVICE
    case 0x8876086C:
        return 'I';  // D3DERR_INVALIDCALL
    default:
        return '?';
    }
}

uint32_t Read32(GuestAddress address)
{
    return g_memory->Read32(address);
}

void CheckAdapterCount(const rexbox::runtime::HleRegistry& registry)
{
    const Subject subject{"Direct3D_GetAdapterCount", &sub_00199820_gen,
                          registry.FindExport("Direct3D_GetAdapterCount")->thunk};
    // The library pops one DWORD (ret 4); pass one so esp can be compared.
    const Outcome original = Check(subject, {0});
    if (g_dump)
    {
        std::printf("GetAdapterCount: %u\n", original.eax);
    }
}

uint32_t CheckModeCount(const rexbox::runtime::HleRegistry& registry)
{
    const Subject subject{"D3D_GetAdapterModeCount", &sub_00199830_gen,
                          registry.FindExport("D3D_GetAdapterModeCount")->thunk};
    const Outcome original = Check(subject, {0});
    Check(subject, {1});
    if (g_dump)
    {
        std::printf("GetAdapterModeCount(0): %u\n", original.eax);
    }
    return original.eax;
}

void CheckModes(const rexbox::runtime::HleRegistry& registry, uint32_t count)
{
    const Subject subject{"D3D_EnumAdapterModes", &sub_001998D0_gen, registry.FindExport("D3D_EnumAdapterModes")->thunk};
    for (uint32_t mode = 0; mode <= count + 1; ++mode)
    {
        const Outcome original = Check(subject, {0, mode, g_output});
        if (g_dump)
        {
            std::printf("EnumAdapterModes(0, %u): hr %08X, %zu byte(s) written:", mode, original.eax,
                        WrittenBytes(original, 0x40));
            for (size_t i = 0; i + 4 <= WrittenBytes(original, 0x40); i += 4)
            {
                uint32_t value;
                std::memcpy(&value, &original.output[i], 4);
                std::printf(" %08X", value);
            }
            std::printf("\n");
        }
    }
    Check(subject, {1, 0, g_output});
}

void CheckFormats(const rexbox::runtime::HleRegistry& registry)
{
    const Subject subject{"D3D_CheckDeviceFormat", &sub_00199A60_gen, registry.FindExport("D3D_CheckDeviceFormat")->thunk};
    Check(subject, {1, 1, 0x12, 0, 1, 0x12});
    Check(subject, {0, 0, 0x12, 0, 1, 0x12});
    Check(subject, {0, 2, 0x12, 0, 1, 0x12});
    const uint32_t adapter_formats[] = {0x12, 0x1E, 0x11, 0x1C, 0x06, 0x00};
    const uint32_t usages[] = {0, 1, 2, 3};
    const uint32_t types[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    if (g_dump)
    {
        std::printf("CheckDeviceFormat(0, HAL, adapter 12, usage u, rtype t, format f): columns are "
                    "usage 0,1,2,3 for rtype 1..9,0; O ok, N not available, I invalid call, D invalid device\n");
    }
    for (uint32_t format = 0; format < 0x80; ++format)
    {
        std::string row;
        for (const uint32_t type : types)
        {
            for (const uint32_t usage : usages)
            {
                const Outcome original = Check(subject, {0, 1, 0x12, usage, type, format});
                row += VerdictChar(original.eax);
            }
            row += ' ';
        }
        if (g_dump)
        {
            std::printf("  format %02X: %s\n", format, row.c_str());
        }
    }
    for (const uint32_t adapter_format : adapter_formats)
    {
        for (uint32_t format = 0; format < 0x80; ++format)
        {
            for (const uint32_t usage : usages)
            {
                Check(subject, {0, 1, adapter_format, usage, 1, format});
                Check(subject, {0, 1, adapter_format, usage, 3, format});
            }
        }
    }
}

void CheckMultiSample(const rexbox::runtime::HleRegistry& registry)
{
    const Subject subject{"Direct3D_CheckDeviceMultiSampleType", &sub_00199AE0_gen,
                          registry.FindExport("Direct3D_CheckDeviceMultiSampleType")->thunk};
    Check(subject, {1, 1, 0x12, 0, 0x11});
    Check(subject, {0, 0, 0x12, 0, 0x11});
    const uint32_t samples[] = {0x0000, 0x0001, 0x0002, 0x0004, 0x0011, 0x002A, 0x0030, 0x1021, 0x1121, 0x2021,
                                0x2012, 0x1022, 0x1222, 0x2022, 0x2222, 0x1233, 0x2233};
    if (g_dump)
    {
        std::printf("CheckDeviceMultiSampleType(0, HAL, format f, 0, type): columns are types");
        for (const uint32_t sample : samples)
        {
            std::printf(" %04X", sample);
        }
        std::printf("\n");
    }
    for (uint32_t format = 0; format < 0x80; ++format)
    {
        std::string row;
        for (const uint32_t sample : samples)
        {
            const Outcome original = Check(subject, {0, 1, format, 0, sample});
            row += VerdictChar(original.eax);
        }
        if (g_dump)
        {
            std::printf("  format %02X: %s\n", format, row.c_str());
        }
    }
}

void CheckCaps(const rexbox::runtime::HleRegistry& registry)
{
    const Subject device{"D3DDevice_GetDeviceCaps", &sub_0018CB40_gen, registry.FindExport("D3DDevice_GetDeviceCaps")->thunk};
    const Outcome original = Check(device, {g_output});
    if (g_dump)
    {
        std::printf("D3DDevice_GetDeviceCaps: eax %08X, %zu byte(s) written\n", original.eax,
                    WrittenBytes(original, 0x100));
        for (size_t i = 0; i + 4 <= WrittenBytes(original, 0x100); i += 4)
        {
            uint32_t value;
            std::memcpy(&value, &original.output[i], 4);
            std::printf("%s0x%08X,%s", i % 32 == 0 ? "    " : " ", value, i % 32 == 28 ? "\n" : "");
        }
        std::printf("\n");
    }
    const Subject d3d{"D3D_GetDeviceCaps", &sub_00199A30_gen, registry.FindExport("D3D_GetDeviceCaps")->thunk};
    Check(d3d, {0, 1, g_output});
    Check(d3d, {1, 1, g_output});
    Check(d3d, {0, 0, g_output});
    Check(d3d, {0, 2, g_output});
}

// Writes a resource header into the scratch area.
void WriteHeader(uint32_t offset, uint32_t common, uint32_t data, uint32_t format, uint32_t size, uint32_t parent)
{
    const GuestAddress header = g_output + offset;
    Write32(header + 0x00, common);
    Write32(header + 0x04, data);
    Write32(header + 0x08, 0);
    Write32(header + 0x0C, format);
    Write32(header + 0x10, size);
    Write32(header + 0x14, parent);
}

// XGRAPHC Format field for a swizzled power-of-two texture.
uint32_t SwizzledFormat(uint32_t format, uint32_t log2_width, uint32_t log2_height, uint32_t levels, bool cube)
{
    return (log2_height << 24) | (log2_width << 20) | (levels << 16) | (format << 8) | (2u << 4) | 1u | 8u |
           (cube ? 4u : 0u);
}

void CheckResources(const rexbox::runtime::HleRegistry& registry)
{
    const Subject add_ref{"D3DResource_AddRef", &sub_00192950_gen, registry.FindExport("D3DResource_AddRef")->thunk};
    const Subject release{"D3DResource_Release", &sub_00192990_gen, registry.FindExport("D3DResource_Release")->thunk};
    const Subject get_type{"D3DResource_GetType", &sub_00192790_gen, registry.FindExport("D3DResource_GetType")->thunk};
    const Subject get_desc{"D3DSurface_GetDesc", &sub_0018E730_gen, registry.FindExport("D3DSurface_GetDesc")->thunk};
    const GuestAddress header_a = g_output + kHeaderA;

    // Reference counting: plain resources, a surface with a parent texture,
    // and the paths that avoid the destroy routine.
    const uint32_t commons[] = {0x00050001, 0x00050003, 0x00040002, 0x00000001, 0x00010005, 0x01050002,
                                0x00850001, 0x01050001};
    for (const uint32_t common : commons)
    {
        const Setup plain = [common] { WriteHeader(kHeaderA, common, 0x100000, 0x00011129, 0x13013FBF, 0); };
        Check(add_ref, {header_a}, plain);
        // Destroying needs the library's device; skip the one case that would (refcount 1, no internal count).
        if ((common & 0xFFFF) != 1 || (common & 0x780000) != 0)
        {
            Check(release, {header_a}, plain);
        }
        Check(get_type, {header_a}, plain);
    }
    // A surface whose refcount is 0 or 1 propagates to its parent texture.
    for (const uint32_t common : {0x00050000u, 0x00050001u, 0x00050002u})
    {
        const Setup with_parent = [common] {
            WriteHeader(kHeaderA, common, 0x100000, 0x00011129, 0x13013FBF, g_output + kHeaderB);
            WriteHeader(kHeaderB, 0x00040003, 0x200000, SwizzledFormat(0x06, 8, 8, 1, false), 0, 0);
        };
        Check(add_ref, {header_a}, with_parent);
        if (common != 0x00050001u)
        {
            Check(release, {header_a}, with_parent);
        }
    }

    // Register adds the base to Data and masks it to a physical address for
    // everything but vertex buffers.
    const Subject reg{"D3DResource_Register", &sub_001929E0_gen, registry.FindExport("D3DResource_Register")->thunk};
    for (const uint32_t common : {0x00000001u, 0x00010001u, 0x00020001u, 0x00030001u, 0x00040001u, 0x00050001u, 0x00060001u})
    {
        for (const uint32_t base : {0x00100000u, 0x04000000u, 0x0F123456u})
        {
            char note[32];
            std::snprintf(note, sizeof(note), " common %08X", common);
            g_note = note;
            Check(reg, {header_a, base},
                  [common] { WriteHeader(kHeaderA, common, 0x00000800, 0x00011129, 0x13013FBF, 0); });
            g_note.clear();
        }
    }

    // Resource types from the header's type bits and dimension.
    struct TypeCase
    {
        uint32_t common;
        uint32_t format;
    };
    const TypeCase type_cases[] = {
        {0x00000001, 0},                                        {0x00010001, 0},
        {0x00020001, 0},                                        {0x00030001, 0},
        {0x00040001, SwizzledFormat(0x06, 8, 8, 1, false)},     {0x00040001, SwizzledFormat(0x06, 8, 8, 1, true)},
        {0x00040001, 0x00000039 | (6u << 8) | (8u << 20) | (8u << 24) | (3u << 28)},  // 3D dimension
        {0x00050001, 0x00011129},                               {0x00050001, 0x00011139},
        {0x00060001, 0},                                        {0x00070001, 0},
    };
    for (const TypeCase& type_case : type_cases)
    {
        Check(get_type, {header_a},
              [type_case] { WriteHeader(kHeaderA, type_case.common, 0x100000, type_case.format, 0, 0); });
    }

    // Surface descriptions: linear surfaces built by the lifted
    // XGSetSurfaceHeader, then swizzled textures encoded by hand.
    struct LinearCase
    {
        uint32_t width;
        uint32_t height;
        uint32_t format;
        uint32_t pitch;
    };
    const LinearCase linear_cases[] = {{640, 480, 0x11, 1280}, {640, 480, 0x12, 2560}, {640, 480, 0x2E, 2560},
                                       {512, 256, 0x1E, 2048}, {720, 480, 0x1C, 1472}, {64, 64, 0x13, 64},
                                       {320, 240, 0x11, 640},  {640, 480, 0x2C, 1280}};
    for (const LinearCase& linear : linear_cases)
    {
        const Setup setup = [linear] {
            // The lifted XGSetSurfaceHeader fills header A in place.
            uint32_t esp = g_stack_top;
            const uint32_t args[] = {linear.width, linear.height, linear.format, g_output + kHeaderA, 0x100000,
                                     linear.pitch};
            for (size_t i = 6; i-- > 0;)
            {
                esp -= 4;
                Write32(esp, args[i]);
            }
            esp -= 4;
            Write32(esp, 0);
            rexbox_recomp_reset_thread(esp, 0);
            RexboxRecompGprs gprs = {};
            gprs.esp = esp;
            rexbox_recomp_set_gprs(&gprs);
            sub_001BBCA5();
        };
        const Outcome original = Check(get_desc, {header_a, g_output}, setup);
        if (g_dump)
        {
            std::printf("XGSetSurfaceHeader(%u, %u, %02X, pitch %u): Format %08X Size %08X; GetDesc:", linear.width,
                        linear.height, linear.format, linear.pitch, Read32(header_a + 0xC), Read32(header_a + 0x10));
            for (size_t i = 0; i < 0x20; i += 4)
            {
                uint32_t value;
                std::memcpy(&value, &original.output[i], 4);
                std::printf(" %08X", value);
            }
            std::printf("\n");
        }
    }
    struct SwizzledCase
    {
        uint32_t format;
        uint32_t log2_width;
        uint32_t log2_height;
        uint32_t levels;
    };
    const SwizzledCase swizzled_cases[] = {{0x06, 8, 8, 1}, {0x06, 8, 8, 9}, {0x05, 7, 6, 1}, {0x0C, 7, 7, 8},
                                           {0x0E, 6, 6, 1}, {0x0F, 2, 2, 1}, {0x00, 4, 3, 1}, {0x19, 0, 0, 1}};
    for (const SwizzledCase& swizzled : swizzled_cases)
    {
        const uint32_t format_field =
            SwizzledFormat(swizzled.format, swizzled.log2_width, swizzled.log2_height, swizzled.levels, false);
        Check(get_desc, {header_a, g_output},
              [format_field] { WriteHeader(kHeaderA, 0x00040001, 0x100000, format_field, 0, 0); });
    }
}

// The device's own surfaces must carry the same header a title would build
// with XGSetSurfaceHeader for the same size and format.
void CheckDeviceSurfaces(const rexbox::runtime::HleRegistry& registry)
{
    rexbox::hle::d3d8::SetD3d8Headless(true);
    const GuestAddress pp = g_output;
    const GuestAddress out = g_output + 0x40;
    g_memory->Fill(g_output, 0, kOutputBytes);
    Write32(pp + 0x00, 640);
    Write32(pp + 0x04, 480);
    Write32(pp + 0x08, 0x11);
    Write32(pp + 0x0C, 2);
    Write32(pp + 0x10, 0x0011);
    Write32(pp + 0x20, 1);
    Write32(pp + 0x24, 0x2E);
    Write32(pp + 0x30, 1);
    Outcome created = Run(registry.FindExport("Direct3D_CreateDevice")->thunk, {0, 1, 0, 0x40, pp, out},
                          [pp] {
                              Write32(pp + 0x00, 640);
                              Write32(pp + 0x04, 480);
                              Write32(pp + 0x08, 0x11);
                              Write32(pp + 0x0C, 2);
                              Write32(pp + 0x10, 0x0011);
                              Write32(pp + 0x14, 0);
                              Write32(pp + 0x18, 0);
                              Write32(pp + 0x1C, 0);
                              Write32(pp + 0x20, 1);
                              Write32(pp + 0x24, 0x2E);
                              Write32(pp + 0x28, 0x100);
                              Write32(pp + 0x2C, 0);
                              Write32(pp + 0x30, 1);
                          });
    ++g_cases;
    if (created.eax != 0)
    {
        std::printf("DIFF Direct3D_CreateDevice (headless): hr %08X\n", created.eax);
        ++g_failures;
        return;
    }
    struct SurfaceCase
    {
        const char* name;
        const char* export_name;
        std::vector<uint32_t> args;
        uint32_t format;
        uint32_t pitch;
    };
    const SurfaceCase cases[] = {
        {"back buffer 0", "D3DDevice_GetBackBuffer", {0, 0, out}, 0x11, 1280},
        {"back buffer -1", "D3DDevice_GetBackBuffer", {0xFFFFFFFFu, 0, out}, 0x11, 1280},
        {"depth stencil", "D3DDevice_GetDepthStencilSurface", {out}, 0x2E, 2560},
    };
    for (const SurfaceCase& surface : cases)
    {
        const Outcome got = Run(registry.FindExport(surface.export_name)->thunk, surface.args);
        ++g_cases;
        uint32_t header = 0;
        std::memcpy(&header, &got.output[0x40], 4);
        if (got.eax != 0 || header == 0)
        {
            std::printf("DIFF %s: hr %08X header %08X\n", surface.name, got.eax, header);
            ++g_failures;
            continue;
        }
        // The reference header from the lifted XGSetSurfaceHeader.
        Run(&sub_001BBCA5, {640, 480, surface.format, g_output + kHeaderA, Read32(header + 4), surface.pitch});
        const uint32_t expected_format = Read32(g_output + kHeaderA + 0xC);
        const uint32_t expected_size = Read32(g_output + kHeaderA + 0x10);
        const uint32_t actual_format = Read32(header + 0xC);
        const uint32_t actual_size = Read32(header + 0x10);
        const uint32_t common = Read32(header);
        bool same = expected_format == actual_format && expected_size == actual_size;
        // GetBackBuffer/GetDepthStencilSurface AddRef: refcount 2 (device + caller), surface type, D3DCREATED.
        if ((common & 0xFFFF) != 2 || (common & 0x70000) != 0x50000 || (common & 0x01000000) == 0)
        {
            same = false;
        }
        if (!same)
        {
            std::printf("DIFF %s: Common %08X Format %08X (XGSetSurfaceHeader %08X) Size %08X (%08X)\n",
                        surface.name, common, actual_format, expected_format, actual_size, expected_size);
            ++g_failures;
        }
        else if (g_dump)
        {
            std::printf("%s: header %08X Common %08X Format %08X Size %08X\n", surface.name, header, common,
                        actual_format, actual_size);
        }
    }
}

}  // namespace

// State setters write the library globals the title's inlines also use
// (docs/design/20261012-i023-d3d8-fixed-function-draw.md section 2). Each
// case runs a sequence of calls on each side from the same globals and a
// fresh stand-in device (push buffer pointers only), then compares eax/esp of
// the last call, the scratch area (resource headers) and the globals. The
// dirty-flag word is left out: only the library's own draw path consumes it.
struct StateArea
{
    GuestAddress device_slot = 0;  // D3D_g_pDevice
    GuestAddress begin = 0;        // D3D_g_pDevice .. the end of D3D_g_RenderState
    GuestAddress end = 0;
    GuestAddress dirty = 0;
    GuestAddress device = 0;       // the stand-in device and its push buffer
    GuestAddress push = 0;
};

StateArea g_state_area;
constexpr uint32_t kDeviceBytes = 0x3000;
constexpr uint32_t kPushBytes = 0x10000;
constexpr uint32_t kDevicePushFence = 0x00001234;

using Call = std::vector<uint32_t>;

struct SequenceOutcome
{
    Outcome last;
    std::vector<uint8_t> globals;
};

SequenceOutcome RunSequence(GuestFunction function, const std::vector<Call>& calls, const Setup& setup,
                            const std::vector<uint8_t>& initial_globals)
{
    const StateArea& area = g_state_area;
    std::memcpy(g_memory->Translate(area.begin), initial_globals.data(), initial_globals.size());
    g_memory->Fill(area.device, 0, kDeviceBytes);
    Write32(area.device + 0x0, area.push);                    // push-buffer write pointer
    Write32(area.device + 0x4, area.push + kPushBytes - 0x400);  // limit before a kick
    Write32(area.device + 0x30, kDevicePushFence);
    Write32(area.device_slot, area.device);
    SequenceOutcome outcome;
    for (size_t i = 0; i < calls.size(); ++i)
    {
        // The scratch area (and the setup's headers) is prepared once per sequence.
        outcome.last = Run(function, calls[i], i == 0 ? setup : Setup([] {}), i == 0);
    }
    outcome.globals.assign(g_memory->Translate(area.begin), g_memory->Translate(area.end));
    const size_t dirty = area.dirty - area.begin;
    std::memset(outcome.globals.data() + dirty, 0, 4);
    return outcome;
}

// The setters return void; what they leave in eax is not observable by a
// correct caller, so only esp is compared for them.
void CheckSequence(const Subject& subject, const std::vector<Call>& calls, const Setup& setup = nullptr)
{
    const StateArea& area = g_state_area;
    const std::vector<uint8_t> initial(g_memory->Translate(area.begin), g_memory->Translate(area.end));
    const SequenceOutcome original = RunSequence(subject.original, calls, setup, initial);
    const SequenceOutcome hle = RunSequence(subject.hle, calls, setup, initial);
    std::memcpy(g_memory->Translate(area.begin), initial.data(), initial.size());
    ++g_cases;
    std::string text;
    for (const Call& call : calls)
    {
        text += (text.empty() ? "" : "; ") + Describe(call);
    }
    bool same = true;
    if (original.last.esp != hle.last.esp)
    {
        std::printf("DIFF %s(%s): esp original %08X hle %08X\n", subject.name, text.c_str(), original.last.esp,
                    hle.last.esp);
        same = false;
    }
    if (original.last.output != hle.last.output)
    {
        size_t first = 0;
        while (original.last.output[first] == hle.last.output[first])
        {
            ++first;
        }
        std::printf("DIFF %s(%s): scratch differs first at +%zX (original %02X hle %02X)\n", subject.name,
                    text.c_str(), first, original.last.output[first], hle.last.output[first]);
        same = false;
    }
    if (original.globals != hle.globals)
    {
        for (size_t offset = 0; offset < original.globals.size(); offset += 4)
        {
            uint32_t a = 0;
            uint32_t b = 0;
            std::memcpy(&a, original.globals.data() + offset, 4);
            std::memcpy(&b, hle.globals.data() + offset, 4);
            if (a != b)
            {
                std::printf("DIFF %s(%s): global %08X original %08X hle %08X\n", subject.name, text.c_str(),
                            static_cast<uint32_t>(area.begin + offset), a, b);
            }
        }
        same = false;
    }
    g_failures += same ? 0 : 1;
}

void CheckStateSetters(const rexbox::runtime::HleRegistry& registry)
{
    const auto subject = [&registry](const char* name, GuestFunction original) {
        return Subject{name, original, registry.FindExport(name)->thunk};
    };
    struct Named
    {
        const char* name;
        GuestFunction original;
    };
    // Complex render states: one slot each.
    const Named complex_setters[] = {
        {"D3DDevice_SetRenderState_PSTextureModes", &sub_0018E900_gen},
        {"D3DDevice_SetRenderState_VertexBlend", &sub_0018F010_gen},
        {"D3DDevice_SetRenderState_FogColor", &sub_0018EB80_gen},
        {"D3DDevice_SetRenderState_FillMode", &sub_0018EEF0_gen},
        {"D3DDevice_SetRenderState_BackFillMode", &sub_0018EF40_gen},
        {"D3DDevice_SetRenderState_TwoSidedLighting", &sub_0018EFA0_gen},
        {"D3DDevice_SetRenderState_NormalizeNormals", &sub_0018EC80_gen},
        {"D3DDevice_SetRenderState_ZEnable", &sub_0018F6C0_gen},
        {"D3DDevice_SetRenderState_StencilEnable", &sub_0018F760_gen},
        {"D3DDevice_SetRenderState_StencilFail", &sub_0018F7F0_gen},
        {"D3DDevice_SetRenderState_FrontFace", &sub_0018EC40_gen},
        {"D3DDevice_SetRenderState_CullMode", &sub_0018EBD0_gen},
        {"D3DDevice_SetRenderState_TextureFactor", &sub_0018ECC0_gen},
        {"D3DDevice_SetRenderState_ZBias", &sub_0018EE10_gen},
        {"D3DDevice_SetRenderState_LogicOp", &sub_0018EE90_gen},
        {"D3DDevice_SetRenderState_EdgeAntiAlias", &sub_0018EB00_gen},
        {"D3DDevice_SetRenderState_MultiSampleAntiAlias", &sub_0018F9D0_gen},
        {"D3DDevice_SetRenderState_MultiSampleMask", &sub_0018FA30_gen},
        {"D3DDevice_SetRenderState_MultiSampleMode", &sub_0018E8A0_gen},
        {"D3DDevice_SetRenderState_MultiSampleRenderTargetMode", &sub_0018E8D0_gen},
        {"D3DDevice_SetRenderState_ShadowFunc", &sub_0018EB40_gen},
        {"D3DDevice_SetRenderState_LineWidth", &sub_0018ED20_gen},
        {"D3DDevice_SetRenderState_Dxt1NoiseEnable", &sub_0018ED90_gen},
        {"D3DDevice_SetRenderState_YuvEnable", &sub_0018F860_gen},
        {"D3DDevice_SetRenderState_OcclusionCullEnable", &sub_0018F890_gen},
        {"D3DDevice_SetRenderState_StencilCullEnable", &sub_0018F900_gen},
        {"D3DDevice_SetRenderState_RopZCmpAlwaysRead", &sub_0018F970_gen},
        {"D3DDevice_SetRenderState_RopZRead", &sub_0018F990_gen},
        {"D3DDevice_SetRenderState_DoNotCullUncompressed", &sub_0018F9B0_gen},
    };
    for (const Named& setter : complex_setters)
    {
        const Subject s = subject(setter.name, setter.original);
        for (const uint32_t value : {0u, 1u, 2u, 0x404u, 0x3F800000u})
        {
            CheckSequence(s, {{value}});
        }
    }

    // NotInline: every slot of the array (deferred and complex states are
    // stored; simple ones are stored and pushed).
    const Subject not_inline = subject("D3DDevice_SetRenderStateNotInline", &sub_0018E960_gen);
    for (uint32_t state = 0; state < 146; ++state)
    {
        CheckSequence(not_inline, {{state, 0x00000201u}});
    }

    // Texture-stage states.
    const Subject stage_state = subject("D3DDevice_SetTextureStageStateNotInline", &sub_0018F270_gen);
    for (uint32_t stage = 0; stage < 4; ++stage)
    {
        for (uint32_t type = 0; type < 31; ++type)
        {
            CheckSequence(stage_state, {{stage, type, 0x00000003u + type}});
        }
    }
    const Subject tex_coord = subject("D3DDevice_SetTextureState_TexCoordIndex", &sub_0018F060_gen);
    const Subject border = subject("D3DDevice_SetTextureState_BorderColor", &sub_0018F1F0_gen);
    const Subject color_key = subject("D3DDevice_SetTextureState_ColorKeyColor", &sub_0018F230_gen);
    const Subject bump_env = subject("D3DDevice_SetTextureState_BumpEnv", &sub_0018F180_gen);
    for (uint32_t stage = 0; stage < 4; ++stage)
    {
        CheckSequence(tex_coord, {{stage, 1u}});
        CheckSequence(tex_coord, {{stage, 0x00020000u}});
        CheckSequence(border, {{stage, 0xFF00FF00u}});
        CheckSequence(color_key, {{stage, 0x00FF00FFu}});
        for (uint32_t type = 22; type <= 27; ++type)
        {
            CheckSequence(bump_env, {{stage, type, 0x3F000000u}});
        }
    }

    // Bindings hold an internal reference on the resource and stamp Lock
    // when dropped; every sequence ends unbound so both sides start clean.
    const GuestAddress header_a = g_output + kHeaderA;
    const GuestAddress header_b = g_output + kHeaderB;
    const Setup buffers = [] {
        WriteHeader(kHeaderA, 0x00000001, 0x00123000, 0, 0, 0);
        WriteHeader(kHeaderB, 0x00010002, 0x00456000, 0, 0, 0);
    };
    const Subject indices = subject("D3DDevice_SetIndices", &sub_0018E090_gen);
    CheckSequence(indices, {{header_a, 0}, {0, 0}}, buffers);
    CheckSequence(indices, {{header_b, 7}, {header_a, 0}, {header_a, 3}, {0, 0}}, buffers);
    const Subject streams = subject("D3DDevice_SetStreamSource", &sub_001900D0_gen);
    CheckSequence(streams, {{0, header_a, 32}, {0, 0, 0}}, buffers);
    CheckSequence(streams, {{0, header_a, 32}, {0, header_b, 16}, {3, header_a, 12}, {0, 0, 0}, {3, 0, 0}}, buffers);
    const Setup textures = [] {
        WriteHeader(kHeaderA, 0x00040001, 0x00200000, SwizzledFormat(0x06, 8, 8, 1, false), 0, 0);
        WriteHeader(kHeaderB, 0x00040002, 0x00300000, 0x00011129, 0x13013FBF, 0);
    };
    const Subject texture = subject("D3DDevice_SetTexture", &sub_0018DF10_gen);
    CheckSequence(texture, {{0, header_a}, {0, 0}}, textures);
    CheckSequence(texture, {{0, header_a}, {1, header_a}, {0, header_b}, {0, 0}, {1, 0}}, textures);
}

int main(int argc, char** argv)
{
    std::string xbe_path = kDefaultXbe;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--dump") == 0)
        {
            g_dump = true;
        }
        else
        {
            xbe_path = argv[i];
        }
    }

    std::string error;
    const auto image = rexbox::xbe::XbeImage::Load(xbe_path, &error);
    if (!image)
    {
        std::fprintf(stderr, "error: %s: %s\n", xbe_path.c_str(), error.c_str());
        return 2;
    }
    if (rejsrf::IdentifyXbe(*image) != rejsrf::PrimaryVariant())
    {
        std::fprintf(stderr, "error: %s is not the %s build the lifted originals come from\n", xbe_path.c_str(),
                     rejsrf::PrimaryVariant()->label);
        return 2;
    }

    GuestMemory memory;
    if (!memory.Initialize(GuestMemory::kRetailRamSize) ||
        !rexbox::runtime::LoadXbeIntoGuestMemory(*image, memory, &error))
    {
        std::fprintf(stderr, "error: cannot set up guest memory: %s\n", error.c_str());
        return 2;
    }
    rexbox::runtime::GuestHeap heap(memory.size());
    heap.Reserve(0, image->base_address());
    heap.Reserve(image->base_address(), image->size_of_image());

    rexbox::recomp::RecompExecutor executor(
        reinterpret_cast<rexbox::recomp::LookupFunction>(rejsrf::RecompiledLookupFunction()));
    rexbox::runtime::HleRegistry registry;
    rexbox::runtime::SetEnvironment({&memory, &heap, &registry, &executor});
    rexbox::hle::kernel::RegisterKernelHle(registry);
    rexbox::hle::d3d8::RegisterD3d8Hle(registry);
    rexbox::hle::kernel::KernelConfig kernel_config;
    kernel_config.game_directory = std::filesystem::absolute(xbe_path).parent_path();
    kernel_config.hdd_root = std::filesystem::current_path() / "hdd";
    rexbox::hle::kernel::InitializeKernel(kernel_config);
    rexbox::hle::kernel::BindKernelImports(*image, memory, registry);
    if (!rexbox::recomp::Attach(memory, registry, *image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 2;
    }
    rejsrf::InitializeRecompiledDispatch();
    // An original may call another overridden D3D8 function (an internal one
    // with no HLE export); run those as lifted code rather than stopping.
    rexbox::recomp::SetUnimplementedHleFallback(true);

    const auto stack = heap.Allocate(0x10000, rexbox::runtime::GuestHeap::kPageSize, false);
    const auto output = heap.Allocate(kOutputBytes, rexbox::runtime::GuestHeap::kPageSize, false);
    if (!stack || !output)
    {
        std::fprintf(stderr, "error: cannot allocate guest stack\n");
        return 2;
    }
    g_memory = &memory;
    g_stack_top = *stack + 0x10000 - 0x100;
    g_output = *output;

    CheckAdapterCount(registry);
    const uint32_t mode_count = CheckModeCount(registry);
    CheckModes(registry, mode_count);
    CheckFormats(registry);
    CheckMultiSample(registry);
    CheckCaps(registry);
    CheckResources(registry);
    CheckDeviceSurfaces(registry);

    // The setter cases need the title's identified D3D8 variables and a
    // stand-in device for the originals to push into.
    rejsrf::BindTitleHle(*image, registry);
    const auto state_device = heap.Allocate(kDeviceBytes, rexbox::runtime::GuestHeap::kPageSize, false);
    const auto state_push = heap.Allocate(kPushBytes, rexbox::runtime::GuestHeap::kPageSize, false);
    g_state_area.device_slot = registry.FindVariable("D3D_g_pDevice");
    const GuestAddress stream = registry.FindVariable("D3D_g_Stream");
    const GuestAddress render_state = registry.FindVariable("D3D_g_RenderState");
    if (!state_device || !state_push || g_state_area.device_slot == 0 || stream == 0 || render_state == 0)
    {
        std::fprintf(stderr, "error: D3D8 variables or stand-in device unavailable\n");
        return 2;
    }
    g_state_area.device = *state_device;
    g_state_area.push = *state_push;
    g_state_area.begin = g_state_area.device_slot;
    g_state_area.end = render_state + 146 * 4;
    g_state_area.dirty = stream + 0x1F0;
    CheckStateSetters(registry);

    std::printf("%d case(s), %d difference(s)\n", g_cases, g_failures);
    std::fflush(stdout);
    // The kernel's tick thread is still running; ending the process ends it.
    std::_Exit(g_failures == 0 ? 0 : 1);
}
