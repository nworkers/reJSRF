// rejsrf: application entry. Orchestration only -- find the user's XBE in the
// game directory, place it in guest memory, install the runtime environment,
// register the HLE modules, apply the title profile and, when the build
// carries recompiled code, boot the entry point as the first guest thread and
// report how the guest threads ended.
//
// Usage: rejsrf <game directory>   (the folder extracted from the disc)

#include "rejsrf/jsrf_profile.h"
#include "rejsrf_build_stamp.h"
#include "rexbox/hle/d3d8/d3d8_hle.h"
#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/hle/xapi/xapi_hle.h"
#include "rexbox/hle/xinput/xinput_hle.h"
#include "rexbox/platform/display.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/runtime/xbe_loader.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{

// "Win32/x86 Release": which binary a screenshot or a report came from.
std::string BuildIdentity()
{
#if defined(_WIN32)
    std::string identity = "Win32";
#elif defined(__ANDROID__)
    std::string identity = "Android";
#elif defined(__APPLE__)
    std::string identity = "macOS";
#elif defined(__linux__)
    std::string identity = "Linux";
#else
    std::string identity = "unknown";
#endif
#if defined(_M_IX86) || defined(__i386__)
    identity += "/x86";
#elif defined(_M_X64) || defined(__x86_64__)
    identity += "/x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    identity += "/arm64";
#endif
    // A single-config generator without CMAKE_BUILD_TYPE leaves $<CONFIG> empty.
    const std::string config = REJSRF_BUILD_CONFIG;
#if defined(NDEBUG)
    identity += " " + (config.empty() ? std::string("Release") : config);
#else
    identity += " " + (config.empty() ? std::string("Debug") : config);
#endif
    return identity;
}

struct GameXbe
{
    std::filesystem::path path;
    std::optional<rexbox::xbe::XbeImage> image;
    const rejsrf::XbeVariant* variant = nullptr;
};

bool HasXbeExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".xbe";
}

// The XBE to run from the game directory, identified by content rather than
// by name: the primary variant if the directory has it, otherwise another
// supported build (which is loaded but not run). Reports every XBE it looked
// at when none is supported.
std::optional<GameXbe> FindGameXbe(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> candidates;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (entry.is_regular_file(ec) && HasXbeExtension(entry.path()))
        {
            candidates.push_back(entry.path());
        }
    }
    std::sort(candidates.begin(), candidates.end());
    std::optional<GameXbe> fallback;
    for (const auto& path : candidates)
    {
        std::string error;
        GameXbe found;
        found.path = path;
        found.image = rexbox::xbe::XbeImage::Load(path, &error);
        if (!found.image)
        {
            std::fprintf(stderr, "  %s: %s\n", path.filename().string().c_str(), error.c_str());
            continue;
        }
        found.variant = rejsrf::IdentifyXbe(*found.image);
        if (found.variant == nullptr)
        {
            std::fprintf(stderr,
                         "  %s: not a supported Jet Set Radio Future build "
                         "(title id %08X, certificate version %08X)\n",
                         path.filename().string().c_str(), found.image->certificate().title_id,
                         found.image->certificate().version);
            continue;
        }
        if (found.variant == rejsrf::PrimaryVariant())
        {
            return found;
        }
        if (!fallback)
        {
            fallback = std::move(found);
        }
    }
    if (!fallback && candidates.empty())
    {
        std::fprintf(stderr, "  no .xbe file\n");
    }
    return fallback;
}

void PrintThreadReport(const rexbox::recomp::ThreadReport& report)
{
    using rexbox::recomp::StopReason;
    std::printf("thread %u (start %08X): ", report.thread_id, report.start);
    switch (report.reason)
    {
    case StopReason::kReturned:
        std::printf("returned, eax %08X\n", report.exit_code);
        return;
    case StopReason::kExited:
        std::printf("exited with %08X\n", report.exit_code);
        return;
    case StopReason::kNoFunction:
        std::printf("start address has no recompiled function\n");
        return;
    case StopReason::kUnimplementedKernel:
    {
        const uint32_t ordinal = rexbox::runtime::KernelThunkOrdinal(report.target);
        const char* name = rexbox::hle::kernel::KernelExportName(ordinal);
        std::printf("stopped at unimplemented kernel import %u %s\n", ordinal, name != nullptr ? name : "<unknown>");
        break;
    }
    case StopReason::kUnimplementedHle:
        std::printf("stopped at unimplemented XDK function %s %s (%08X)\n",
                    report.hle_library != nullptr ? report.hle_library : "?",
                    report.hle_name != nullptr ? report.hle_name : "?", report.target);
        break;
    case StopReason::kUnresolvedCall:
        std::printf("stopped at indirect call to %08X with no target\n", report.target);
        break;
    case StopReason::kFault:
        std::printf("fault %08X, %s at guest %08X%s (host %p)\n", report.fault_code,
                    report.access_is_write ? "write" : "read", report.guest_access_address,
                    report.access_in_guest_memory ? "" : ", outside guest RAM",
                    reinterpret_cast<void*>(report.host_access_address));
        break;
    }
    std::printf("     eax %08X ecx %08X edx %08X ebx %08X\n", report.eax, report.ecx, report.edx, report.ebx);
    std::printf("     esp %08X ebp %08X esi %08X edi %08X\n", report.esp, report.ebp, report.esi, report.edi);
    std::printf("     last indirect call %08X\n", report.last_icall);
}

}  // namespace

int main(int argc, char** argv)
{
    // Diagnostics interleave with the guest's output; keep stderr unbuffered even when redirected.
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::printf("reJSRF %s\n", REJSRF_VERSION);
    // The window adds the renderer and the FPS (rePIU's form).
    rexbox::platform::SetDisplayTitle(std::string("reJSRF v" REJSRF_VERSION " (") + BuildIdentity() +
                                      ") - Build " REJSRF_BUILD_TIME);
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <game directory>\n"
                             "  the folder extracted from the disc, holding default_eng.xbe and Media\\\n",
                     argv[0]);
        return 2;
    }

    // The game directory is the disc (D:); the XBE to run is found in it.
    const std::filesystem::path game_directory = std::filesystem::absolute(argv[1]);
    std::error_code ec;
    if (!std::filesystem::is_directory(game_directory, ec))
    {
        std::fprintf(stderr, "error: %s is not a directory; pass the game directory (the folder holding the XBE)\n",
                     argv[1]);
        return 1;
    }
    std::optional<GameXbe> game = FindGameXbe(game_directory);
    if (!game)
    {
        std::fprintf(stderr, "error: no supported Jet Set Radio Future XBE in %s\n", argv[1]);
        return 1;
    }
    const rexbox::xbe::XbeImage* image = &*game->image;
    const rejsrf::XbeVariant* variant = game->variant;
    std::string error;
    std::printf("xbe: %s (%s)\n", variant->label, game->path.filename().string().c_str());

    rexbox::runtime::GuestMemory memory;
    if (!memory.Initialize(rexbox::runtime::GuestMemory::kAddressSpaceSize))
    {
        std::fprintf(stderr, "error: cannot reserve guest memory\n");
        return 1;
    }
    if (!rexbox::runtime::LoadXbeIntoGuestMemory(*image, memory, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    rexbox::runtime::GuestHeap heap(memory.size(), rexbox::runtime::GuestMemory::kRetailRamSize);
    heap.Reserve(0, image->base_address());  // low memory below the image
    heap.Reserve(image->base_address(), image->size_of_image());
    std::printf("guest memory: %zu MiB, %zu sections placed, %u KiB free\n", memory.size() >> 20,
                image->sections().size(), heap.free_bytes() >> 10);

    const rejsrf::RecompiledLookup lookup = rejsrf::RecompiledLookupFunction();
    rexbox::recomp::RecompExecutor executor(reinterpret_cast<rexbox::recomp::LookupFunction>(lookup));
    rexbox::runtime::HleRegistry registry;
    rexbox::runtime::SetEnvironment({&memory, &heap, &registry, &executor});

    rexbox::hle::kernel::RegisterKernelHle(registry);
    rexbox::hle::xapi::RegisterXapiHle(registry);
    rexbox::hle::d3d8::RegisterD3d8Hle(registry);
    rexbox::hle::dsound::RegisterDsoundHle(registry);
    rexbox::hle::xinput::RegisterXinputHle(registry);
    // The disc is the game directory; the hard disk lives under ./hdd unless
    // REJSRF_HDD_ROOT names another place.
    rexbox::hle::kernel::KernelConfig kernel_config;
    kernel_config.game_directory = game_directory;
    const char* hdd_root = std::getenv("REJSRF_HDD_ROOT");
    kernel_config.hdd_root = hdd_root != nullptr ? std::filesystem::path(hdd_root)
                                                 : std::filesystem::current_path() / "hdd";
    rexbox::hle::kernel::InitializeKernel(kernel_config);
    const auto missing_kernel = rexbox::hle::kernel::BindKernelImports(*image, memory, registry);
    const size_t title_bindings = rejsrf::BindTitleHle(*image, registry);
    std::printf("hle: %zu exports, kernel imports %zu/%zu bound, %zu title bindings\n", registry.export_count(),
                image->kernel_imports().size() - missing_kernel.size(), image->kernel_imports().size(),
                title_bindings);

    if (lookup == nullptr)
    {
        std::printf("this build has no recompiled code (run scripts/recomp/regenerate.py); stopping after load\n");
        return 0;
    }
    if (variant != rejsrf::PrimaryVariant())
    {
        std::printf("recompiled code was lifted from the %s build; not running %s\n",
                    rejsrf::PrimaryVariant()->label, variant->label);
        return 0;
    }

    if (!rexbox::recomp::Attach(memory, registry, *image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    const bool flat_dispatch = rejsrf::InitializeRecompiledDispatch();
    std::printf("boot: entry %08X, %s dispatch\n", image->entry_point(), flat_dispatch ? "flat" : "searched");
    std::fflush(stdout);
    if (!rexbox::hle::kernel::StartInitialThread(*image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    // REXBOX_WATCHDOG=<seconds>: a run that is still going after that long
    // dumps the guest threads' direct-call profile and ends (diagnostic for a
    // guest thread that stopped reaching the HLE).
    if (const char* watchdog = std::getenv("REXBOX_WATCHDOG"))
    {
        const int seconds = std::atoi(watchdog);
        if (seconds > 0)
        {
            std::thread([seconds, &memory] {
                std::this_thread::sleep_for(std::chrono::seconds(seconds));
                std::fprintf(stderr, "[watchdog] still running after %d s\n", seconds);
                rexbox::recomp::DumpCallProfile(stderr);
                // REXBOX_WATCHDOG_DUMP=<file>: the whole guest RAM, for offline analysis.
                if (const char* dump_path = std::getenv("REXBOX_WATCHDOG_DUMP"))
                {
                    if (FILE* dump = std::fopen(dump_path, "wb"))
                    {
                        std::fwrite(memory.base(), 1, memory.size(), dump);
                        std::fclose(dump);
                        std::fprintf(stderr, "[watchdog] guest memory written to %s\n", dump_path);
                    }
                }
                std::fflush(stderr);
                std::_Exit(4);
            }).detach();
        }
    }

    const std::vector<rexbox::recomp::ThreadReport> reports = executor.WaitForThreads();
    // REXBOX_STOP_DUMP=<file>: after an abnormal stop, the whole guest RAM for
    // offline analysis (diagnostic).
    if (const char* dump_path = std::getenv("REXBOX_STOP_DUMP"); dump_path != nullptr && executor.halted())
    {
        if (FILE* dump = std::fopen(dump_path, "wb"))
        {
            std::fwrite(memory.base(), 1, memory.size(), dump);
            std::fclose(dump);
            std::fprintf(stderr, "[stop] guest memory written to %s\n", dump_path);
        }
    }
    for (const rexbox::recomp::ThreadReport& report : reports)
    {
        PrintThreadReport(report);
    }
    std::fflush(stdout);
    // Guest threads may still be blocked; ending the process ends them.
    std::_Exit(executor.halted() ? 3 : 0);
}
