// rejsrf_d3d8_check: runs each D3D8 HLE export and the lifted XDK original it
// replaces (sub_X_gen) from identical guest states and compares eax, esp and
// the bytes written to the output buffer. The adapter, mode, format and caps
// queries never touch the GPU, so their originals run unchanged on top of
// the kernel HLE (docs/design/20261012-i007-d3d8-hle-first-batch.md section 4).
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

namespace
{

using rexbox::runtime::GuestAddress;
using rexbox::runtime::GuestMemory;
using GuestFunction = void (*)();

constexpr const char* kDefaultXbe = "roms/Jet Set Radio Future/default_eng.xbe";
constexpr uint32_t kOutputBytes = 0x200;  // generous; the largest output is 53 DWORDs
constexpr uint8_t kFillPattern = 0xCC;

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
int g_failures = 0;
int g_cases = 0;

// Calls `function` as a guest `call` would, with `args` on the stack and the
// output buffer filled with a pattern, and collects what it left behind.
Outcome Run(GuestFunction function, const std::vector<uint32_t>& args)
{
    g_memory->Fill(g_output, kFillPattern, kOutputBytes);
    uint32_t esp = g_stack_top;
    for (size_t i = args.size(); i-- > 0;)
    {
        esp -= 4;
        g_memory->Write32(esp, args[i]);
    }
    esp -= 4;
    g_memory->Write32(esp, 0);  // return address
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
Outcome Check(const Subject& subject, const std::vector<uint32_t>& args)
{
    const Outcome original = Run(subject.original, args);
    const Outcome hle = Run(subject.hle, args);
    ++g_cases;
    bool same = true;
    if (original.eax != hle.eax)
    {
        std::printf("DIFF %s(%s): eax original %08X hle %08X\n", subject.name, Describe(args).c_str(), original.eax,
                    hle.eax);
        same = false;
    }
    if (original.esp != hle.esp)
    {
        std::printf("DIFF %s(%s): esp original %08X hle %08X (stack arguments popped: %u vs %u)\n", subject.name,
                    Describe(args).c_str(), original.esp, hle.esp, (original.esp - g_stack_top) / 4 + args.size(),
                    (hle.esp - g_stack_top) / 4 + args.size());
        same = false;
    }
    if (original.output != hle.output)
    {
        size_t first = 0;
        while (original.output[first] == hle.output[first])
        {
            ++first;
        }
        std::printf("DIFF %s(%s): output differs first at +%zX (original %02X hle %02X)\n", subject.name,
                    Describe(args).c_str(), first, original.output[first], hle.output[first]);
        same = false;
    }
    g_failures += same ? 0 : 1;
    return original;
}

// Bytes of the output buffer the function wrote (the pattern is gone).
size_t WrittenBytes(const Outcome& outcome)
{
    size_t end = outcome.output.size();
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
    const Outcome original = Check(subject, {0, 0});
    Check(subject, {1, 0});
    Check(subject, {0, 0x12});
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
                        WrittenBytes(original));
            for (size_t i = 0; i + 4 <= WrittenBytes(original); i += 4)
            {
                uint32_t value;
                std::memcpy(&value, &original.output[i], 4);
                std::printf(" %08X", value);
            }
            std::printf("\n");
        }
    }
    Check(subject, {1, 0, g_output});
    Check(subject, {0, 0, 0});
}

void CheckFormats(const rexbox::runtime::HleRegistry& registry)
{
    const Subject subject{"D3D_CheckDeviceFormat", &sub_00199A60_gen, registry.FindExport("D3D_CheckDeviceFormat")->thunk};
    Check(subject, {1, 1, 0x12, 0, 1, 0x12});
    Check(subject, {0, 0, 0x12, 0, 1, 0x12});
    Check(subject, {0, 2, 0x12, 0, 1, 0x12});
    const uint32_t adapter_formats[] = {0x12, 0x1E, 0x11, 0x1C, 0x06};
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
    // Does the adapter format matter? Compare a few other adapter formats on
    // the formats a title asks about most.
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
    const uint32_t samples[] = {0x0000, 0x0001, 0x0002, 0x0004, 0x0011, 0x1021, 0x1121, 0x2021, 0x2012,
                                0x1022, 0x1222, 0x2022, 0x2222, 0x1233, 0x2233};
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
        std::printf("D3DDevice_GetDeviceCaps: eax %08X, %zu byte(s) written\n", original.eax, WrittenBytes(original));
        for (size_t i = 0; i + 4 <= WrittenBytes(original); i += 4)
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

}  // namespace

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

    std::printf("%d case(s), %d difference(s)\n", g_cases, g_failures);
    std::fflush(stdout);
    // The kernel's tick thread is still running; ending the process ends it.
    std::_Exit(g_failures == 0 ? 0 : 1);
}
