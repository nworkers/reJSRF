// rejsrf: application entry. Orchestration only -- load the user's XBE from
// the game directory, place it in guest memory, install the runtime environment,
// register the HLE modules, apply the title profile and, when the build
// carries recompiled code, boot the entry point as the first guest thread and
// report how the guest threads ended.
//
// Usage: rejsrf <game directory> [--xbe <file>] [--korean <folder>]   (default: default.xbe)

#include "rejsrf/jsrf_host.h"
#include "rejsrf/jsrf_profile.h"
#include "rejsrf_build_stamp.h"
#include "settings.h"
#include "settings_ui.h"
#include "rexbox/hle/d3d8/d3d8_display.h"
#include "rexbox/hle/d3d8/d3d8_hle.h"
#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/hle/xapi/xapi_hle.h"
#include "rexbox/hle/xinput/xinput_hle.h"
#include "rexbox/platform/display.h"
#include "rexbox/platform/input.h"
#include "rexbox/platform/overlay.h"
#include "rexbox/platform/paths.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"
#include "rexbox/runtime/thread_activity.h"
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
    // The XBE build it runs (CMake REJSRF_VARIANT).
    identity += std::string(", ") + rejsrf::BuiltVariant()->tag;
    return identity;
}

// Debug builds report every unresolved stub that runs: xboxrecomp's generated
// stubs print a [STUB] line the first time each one is reached when
// RECOMP_STUB_LOG is set, read when the first stub runs. A value the user set
// (0 included) is left alone; Release builds set nothing
// (docs/design/20261023-i115-xboxrecomp-v0.16.md).
void EnableStubLogInDebug()
{
#if !defined(NDEBUG)
    if (std::getenv("RECOMP_STUB_LOG") != nullptr)
    {
        return;
    }
#if defined(_WIN32)
    _putenv_s("RECOMP_STUB_LOG", "1");
#else
    setenv("RECOMP_STUB_LOG", "1", 0);
#endif
    std::fprintf(stderr, "[app] RECOMP_STUB_LOG=1 (Debug build): unresolved stubs that run are reported\n");
#endif
}

// An Xbox title starts from default.xbe; --xbe names another one in the game
// directory.
constexpr const char* kDefaultXbe = "default.xbe";

struct Options
{
    std::string game_directory;
    std::string xbe = kDefaultXbe;
    std::string korean_directory;  // --korean: the JSRF-kr-patch output folder
};

void PrintUsage(const char* program)
{
    std::fprintf(stderr,
                 "usage: %s <game directory> [--xbe <file>] [--korean <folder>]\n"
                 "  <game directory>  the folder extracted from the disc (served as D:)\n"
                 "  --xbe <file>      the XBE in that folder to load (default: %s)\n"
                 "  --korean <folder> the game folder the JSRF-kr-patch tool made, run when the\n"
                 "                    language is Korean (default: rejsrf.ini KoreanGameDirectory,\n"
                 "                    else \"<game directory>-kr\")\n",
                 program, kDefaultXbe);
}

// Returns false on a malformed command line.
bool ParseArguments(int argc, char** argv, Options* options)
{
    bool have_directory = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--xbe")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "error: --xbe needs a file name\n");
                return false;
            }
            options->xbe = argv[++i];
        }
        else if (argument.rfind("--xbe=", 0) == 0)
        {
            options->xbe = argument.substr(6);
        }
        else if (argument == "--korean")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "error: --korean needs a folder\n");
                return false;
            }
            options->korean_directory = argv[++i];
        }
        else if (argument.rfind("--korean=", 0) == 0)
        {
            options->korean_directory = argument.substr(9);
        }
        else if (argument.rfind("--", 0) == 0)
        {
            std::fprintf(stderr, "error: unknown option %s\n", argument.c_str());
            return false;
        }
        else if (!have_directory)
        {
            options->game_directory = argument;
            have_directory = true;
        }
        else
        {
            std::fprintf(stderr, "error: unexpected argument %s\n", argument.c_str());
            return false;
        }
    }
    return have_directory && !options->xbe.empty();
}

bool HasXbeExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".xbe";
}

// The .xbe files in the game directory, for the error that names a missing one.
std::string ListXbeFiles(const std::filesystem::path& directory)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (entry.is_regular_file(ec) && HasXbeExtension(entry.path()))
        {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    std::string list;
    for (const auto& name : names)
    {
        list += (list.empty() ? "" : ", ") + name;
    }
    return list.empty() ? "none" : list;
}

// The Korean game folder (docs/design/20261023-i113-korean-language.md):
// --korean, else rejsrf.ini's KoreanGameDirectory, else "<game folder>-kr"
// beside the game folder.
std::filesystem::path KoreanDirectory(const Options& options, const rejsrf::Settings& settings,
                                      const std::filesystem::path& game_directory)
{
    if (!options.korean_directory.empty())
    {
        return std::filesystem::absolute(options.korean_directory);
    }
    if (!settings.korean_directory.empty())
    {
        return std::filesystem::absolute(settings.korean_directory);
    }
    const std::filesystem::path folder = game_directory.has_filename() ? game_directory : game_directory.parent_path();
    return folder.parent_path() / (folder.filename().string() + "-kr");
}

// Loads `directory`'s default.xbe when it is the Korean patch of the built
// variant: the disc's code with patched text (rejsrf::ClassifyXbeText).
// Otherwise returns nothing and says why in `problem`.
std::optional<rexbox::xbe::XbeImage> LoadKoreanXbe(const std::filesystem::path& directory, std::string* problem)
{
    const std::filesystem::path xbe_path = directory / kDefaultXbe;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(xbe_path, ec))
    {
        *problem = "no " + xbe_path.string() + " (make it with JSRF-kr-patch's tools/patch.py, or set "
                   "KoreanGameDirectory in rejsrf.ini or --korean)";
        return std::nullopt;
    }
    std::string error;
    auto image = rexbox::xbe::XbeImage::Load(xbe_path, &error);
    if (!image)
    {
        *problem = xbe_path.string() + ": " + error;
        return std::nullopt;
    }
    const rejsrf::XbeText text = rejsrf::ClassifyXbeText(*image);
    if (text != rejsrf::XbeText::kKorean)
    {
        *problem = xbe_path.string() + " is not a Korean patch of the " + rejsrf::BuiltVariant()->label +
                   " build (" + rejsrf::XbeTextName(text) + ")";
        return std::nullopt;
    }
    return image;
}

// The game copies Media files into its cache drive (Z:, one of Partition3-5)
// and reads the copies afterwards. The Korean patch changes some of those
// files under the same names, so the cache partitions are emptied whenever
// the text set they were filled from changes; the game fills them again.
// Saves (Partition1) are left alone (docs/design/20261023-i113-korean-language.md).
void PrepareCache(const std::filesystem::path& hdd_root, const std::string& text_set)
{
    const std::filesystem::path marker = hdd_root / "rejsrf_cache_text.txt";
    std::string previous;
    if (FILE* file = std::fopen(marker.string().c_str(), "rb"))
    {
        char buffer[64] = {};
        const size_t read = std::fread(buffer, 1, sizeof(buffer) - 1, file);
        std::fclose(file);
        previous.assign(buffer, read);
        while (!previous.empty() && std::isspace(static_cast<unsigned char>(previous.back())))
        {
            previous.pop_back();
        }
    }
    if (previous == text_set)
    {
        return;
    }
    std::error_code ec;
    size_t removed = 0;
    for (int partition = 3; partition <= 5; ++partition)
    {
        const std::filesystem::path directory = hdd_root / ("Partition" + std::to_string(partition));
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
        {
            removed += static_cast<size_t>(std::filesystem::remove_all(entry.path(), ec));
        }
    }
    if (removed != 0)
    {
        std::printf("[app] cache: filled from %s text, this run reads %s; emptied Partition3-5 (%zu entries)\n",
                    previous.empty() ? "unrecorded" : previous.c_str(), text_set.c_str(), removed);
    }
    std::filesystem::create_directories(hdd_root, ec);
    if (FILE* file = std::fopen(marker.string().c_str(), "wb"))
    {
        std::fprintf(file, "%s\n", text_set.c_str());
        std::fclose(file);
    }
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
    // Closing the window ends the process the way the watchdog does: the
    // guest threads never return on their own, and ending the process ends them.
    rexbox::runtime::SetExitHandler([](int exit_code, const char* reason) {
        std::fprintf(stderr, "[app] %s; exiting\n", reason);
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(exit_code);
    });

    std::printf("reJSRF %s\n", REJSRF_VERSION);
    // The per-user settings folder (rejsrf.ini when the executable's folder is
    // not writable): the same name the in-tree platform layer used.
    rexbox::platform::SetApplicationIdentity("reJSRF", "reJSRF");
    // The window adds the renderer and the FPS (rePIU's form).
    rexbox::platform::SetDisplayTitle(std::string("reJSRF v" REJSRF_VERSION " (") + BuildIdentity() +
                                      ") - Build " REJSRF_BUILD_TIME);
    Options options;
    if (!ParseArguments(argc, argv, &options))
    {
        PrintUsage(argv[0]);
        return 2;
    }

    // The game directory is the disc (D:); the XBE comes from it.
    const std::filesystem::path game_directory = std::filesystem::absolute(options.game_directory);
    std::error_code ec;
    if (!std::filesystem::is_directory(game_directory, ec))
    {
        std::fprintf(stderr, "error: %s is not a directory; pass the game directory (the folder holding the XBE)\n",
                     options.game_directory.c_str());
        return 1;
    }
    if (options.xbe == kDefaultXbe)
    {
        const std::filesystem::path bundle_xbe = game_directory / "SegaJSRF.xbe";
        if (std::string(rejsrf::BuiltVariant()->tag) == "KOR_BUNDLE" && std::filesystem::is_regular_file(bundle_xbe, ec))
        {
            options.xbe = "SegaJSRF.xbe";
        }
        else if (!std::filesystem::is_regular_file(game_directory / kDefaultXbe, ec) &&
                 std::filesystem::is_regular_file(bundle_xbe, ec))
        {
            options.xbe = "SegaJSRF.xbe";
        }
    }
    const std::filesystem::path xbe_path = game_directory / options.xbe;
    if (!std::filesystem::is_regular_file(xbe_path, ec))
    {
        std::fprintf(stderr, "error: %s has no %s (XBE files there: %s); choose one with --xbe <file>\n",
                     options.game_directory.c_str(), options.xbe.c_str(), ListXbeFiles(game_directory).c_str());
        return 1;
    }
    std::string error;
    // The Korean language may swap in the Korean folder's XBE below.
    std::optional<rexbox::xbe::XbeImage> image = rexbox::xbe::XbeImage::Load(xbe_path, &error);
    if (!image)
    {
        std::fprintf(stderr, "error: %s: %s\n", options.xbe.c_str(), error.c_str());
        return 1;
    }
    const rejsrf::XbeVariant* variant = rejsrf::IdentifyXbe(*image);
    if (variant == nullptr)
    {
        std::fprintf(stderr, "error: %s is not a supported Jet Set Radio Future build "
                             "(title id %08X, certificate version %08X)\n",
                     options.xbe.c_str(), image->certificate().title_id, image->certificate().version);
        return 1;
    }
    std::printf("xbe: %s (%s)\n", variant->label, options.xbe.c_str());
    // The recompiled code was lifted from the disc's code sections; an XBE of
    // this build whose code differs cannot run on it
    // (docs/design/20261023-i113-korean-language.md).
    const rejsrf::XbeText game_text = rejsrf::ClassifyXbeText(*image);
    if (game_text == rejsrf::XbeText::kCodeChanged)
    {
        std::fprintf(stderr, "error: %s has the %s build's certificate but different code; the recompiled code "
                             "comes from the disc's, so it cannot run\n",
                     options.xbe.c_str(), variant->label);
        return 1;
    }

    // The settings menu (Tab) and rejsrf.ini: the display starts from the
    // saved settings (docs/design/20261017-i084-settings-overlay.md).
    static rejsrf::SettingsFile settings_file;
    const rejsrf::Settings settings = settings_file.Load();
    // The language this run starts in: REJSRF_LANGUAGE for this run only,
    // else the saved one (docs/design/20261020-i104-language-option.md).
    rejsrf::Language language = settings.language;
    if (const char* language_override = std::getenv("REJSRF_LANGUAGE"))
    {
        if (!rejsrf::ParseLanguage(language_override, &language))
        {
            std::fprintf(stderr, "warning: REJSRF_LANGUAGE=%s is not English, Japanese or Korean; using %s\n",
                         language_override, rejsrf::LanguageName(language));
        }
    }
    // Korean runs English mode on the game folder the JSRF-kr-patch tool made:
    // its XBE carries the disc's code with patched text, and its Media the
    // patched text and font. English and Japanese run the original folder
    // (docs/design/20261023-i113-korean-language.md).
    rejsrf::LanguageStatus language_status;
    std::filesystem::path disc_directory = game_directory;
    std::string xbe_name = options.xbe;
    if (game_text == rejsrf::XbeText::kKorean)
    {
        // The game folder itself is the Korean patch: its font has no kana or
        // kanji, so it runs in Korean only.
        language_status.patched_game = true;
        language_status.korean_directory = game_directory.string();
        if (language != rejsrf::Language::kKorean)
        {
            std::printf("[app] %s is the Korean patch; running in Korean\n", game_directory.string().c_str());
        }
        language = rejsrf::Language::kKorean;
    }
    else if (rejsrf::LanguageAvailable(rejsrf::Language::kKorean) && variant == rejsrf::BuiltVariant())
    {
        const std::filesystem::path korean_directory = KoreanDirectory(options, settings, game_directory);
        language_status.korean_directory = korean_directory.string();
        auto korean_image = LoadKoreanXbe(korean_directory, &language_status.korean_problem);
        if (language == rejsrf::Language::kKorean)
        {
            if (korean_image)
            {
                image = std::move(korean_image);
                disc_directory = korean_directory;
                xbe_name = (korean_directory / kDefaultXbe).string();
                std::printf("[app] Korean: the disc is %s\n", korean_directory.string().c_str());
            }
            else
            {
                std::fprintf(stderr, "warning: Korean: %s; running in English\n",
                             language_status.korean_problem.c_str());
                language = rejsrf::Language::kEnglish;
            }
        }
    }
    else if (language == rejsrf::Language::kKorean)
    {
        language_status.korean_problem = std::string("the Korean patch is made for the US build; this executable "
                                                     "is built for the ") + rejsrf::BuiltVariant()->label + " build";
        std::fprintf(stderr, "warning: Korean: %s; running in %s\n", language_status.korean_problem.c_str(),
                     rejsrf::LanguageName(rejsrf::NativeLanguage()));
        language = rejsrf::NativeLanguage();
    }
    rexbox::hle::d3d8::DisplayDefaults display_defaults;
    display_defaults.render_scale = settings.render_scale;
    display_defaults.aspect = static_cast<rexbox::hle::d3d8::AspectMode>(std::min<uint32_t>(settings.aspect, 2));
    display_defaults.vsync = settings.vsync ? rexbox::platform::VsyncMode::kAdaptive : rexbox::platform::VsyncMode::kOff;
    display_defaults.fullscreen = settings.fullscreen;
    display_defaults.window_scale = settings.window_scale;
    display_defaults.vblank_wait = settings.vblank_wait;
    display_defaults.msaa_samples = settings.msaa;
    display_defaults.smaa = static_cast<rexbox::hle::d3d8::SmaaMode>(settings.smaa);
    display_defaults.anisotropy = settings.anisotropy;
    rexbox::hle::d3d8::SetDisplayDefaults(display_defaults);
    // The title's own culling follows the widened picture (#97).
    rejsrf_host_horizontal_view_scale = &rexbox_d3d8_horizontal_view_scale;
    rexbox::platform::SetRumbleStrength(settings.vibration ? settings.vibration_strength : 0);
    rejsrf::BuildInfo build_info;
    build_info.version = REJSRF_VERSION;
    build_info.build_time = REJSRF_BUILD_TIME;
    build_info.identity = BuildIdentity();
    build_info.commit = REJSRF_GIT_COMMIT;
    char title_id[64];
    std::snprintf(title_id, sizeof(title_id), " (title ID %08X)", image->certificate().title_id);
    build_info.xbe = xbe_name + title_id;
    build_info.rexbox_version = REJSRF_REXBOX_VERSION;
    build_info.rexbox_commit = REJSRF_REXBOX_COMMIT;
    static rejsrf::SettingsUi settings_ui(build_info, settings, language, language_status, &settings_file);
    rexbox::platform::SetOverlayBuilder([] { settings_ui.Build(); });

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
    // Before the entry: some of the game's objects keep the language they
    // were created with.
    rejsrf::ApplyLanguage(*image, memory, language);
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
    // The disc is the game directory (the Korean folder for Korean); the hard
    // disk lives under ./hdd unless REJSRF_HDD_ROOT names another place.
    rexbox::hle::kernel::KernelConfig kernel_config;
    kernel_config.game_directory = disc_directory;
    const char* hdd_root = std::getenv("REJSRF_HDD_ROOT");
    kernel_config.hdd_root = hdd_root != nullptr ? std::filesystem::path(hdd_root)
                                                 : std::filesystem::current_path() / "hdd";
    PrepareCache(kernel_config.hdd_root, language == rejsrf::Language::kKorean ? "korean" : "original");
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
    if (variant != rejsrf::BuiltVariant())
    {
        // One executable runs one build: the other disc needs its own build
        // (docs/design/20261020-i108-japanese-variant.md).
        std::fprintf(stderr,
                     "error: %s is the %s build, but this executable is built for the %s build; "
                     "build reJSRF with -DREJSRF_VARIANT=%s to run it\n",
                     options.xbe.c_str(), variant->label, rejsrf::BuiltVariant()->label, variant->id);
        return 1;
    }

    if (!rexbox::recomp::Attach(memory, registry, *image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    const bool flat_dispatch = rejsrf::InitializeRecompiledDispatch();
    std::printf("boot: entry %08X, %s dispatch\n", image->entry_point(), flat_dispatch ? "flat" : "searched");
    std::fflush(stdout);
    EnableStubLogInDebug();
    if (!rexbox::hle::kernel::StartInitialThread(*image, &error))
    {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    // REXBOX_STALL_REPORT=<seconds> (default 10, 0 off): when presents stop
    // for that long, print what each guest thread is doing.
    rexbox::runtime::StartStallMonitor();

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
