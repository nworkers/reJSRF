#include "rejsrf/jsrf_profile.h"

#include "rejsrf/jsrf_addresses.h"
#include "rexbox/recomp/recomp_hle_entry.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace rejsrf
{

namespace
{

// Certificate identity of each known build (docs/analysis/jsrf-xbe-overview.md).
// Both share title id 5345000A, XDK 4134 and the entry point. Their code sits
// at the same addresses but their data moved, so one executable runs one of
// them (docs/design/20261020-i108-japanese-variant.md).
constexpr std::array<XbeVariant, 2> kVariants = {{
    // The USA retail build (MD5 f4e366bfdceee551a09f42219c90d84e).
    {"jsrf_us", "US", "USA (region 1)", 0x5345000A, 0x3C5881D9, 0x00000001, 0x3C558EA7},
    // The Japan-region build (MD5 d8490cdb75bd47b36c62effdf1461ae0).
    {"jsrf_jp", "JP", "Japan (region 2)", 0x5345000A, 0x3C56850C, 0x00000001, 0x3C559627},
}};

#if defined(REJSRF_VARIANT_JP)
constexpr size_t kBuiltVariant = 1;
constexpr Language kNativeLanguage = Language::kJapanese;
#else
constexpr size_t kBuiltVariant = 0;
constexpr Language kNativeLanguage = Language::kEnglish;
#endif

struct XdkInternal
{
    const char* name;
    uint32_t address;
};

// D3D8: the device's default-state routine the hardware initialization
// (0x00192090, under Direct3D_CreateDevice) ends with. It sets the identity
// transforms, the render states from its table, the texture-stage defaults
// and the screen-space offset through the public setters, so the HLE device
// runs it to start from the original defaults.
constexpr std::array<XdkInternal, 1> kXdkInternals = {{
    {"D3D_InitializeDeviceState", 0x001919C0},
}};

// The language index, read through the getter sub_00043700 at 191 sites
// (docs/analysis/language.md sections 3, 7 and 10): the US .data or the
// Japanese bss.
constexpr uint32_t kLanguageIndexAddress = JSRF_LANGUAGE_INDEX;

// Indexed by the Language value (the ending-texture switch at 0x27B87).
constexpr std::array<const char*, kLanguageCount> kLanguageNames = {
    "Japanese", "English",
};

}  // namespace

const XbeVariant* IdentifyXbe(const rexbox::xbe::XbeImage& image)
{
    const auto& cert = image.certificate();
    for (const XbeVariant& variant : kVariants)
    {
        if (cert.title_id == variant.title_id && cert.time_date == variant.certificate_time &&
            cert.version == variant.certificate_version && image.time_date() == variant.image_time)
        {
            return &variant;
        }
    }
    return nullptr;
}

const XbeVariant* BuiltVariant()
{
    return &kVariants[kBuiltVariant];
}

Language NativeLanguage()
{
    return kNativeLanguage;
}

size_t BindTitleHle(const rexbox::xbe::XbeImage& image, rexbox::runtime::HleRegistry& registry)
{
    // The variable addresses were identified in the built variant's XBE.
    const RexboxXdkVariable* variables = RecompiledXdkVariables();
    if (variables == nullptr || IdentifyXbe(image) != BuiltVariant())
    {
        return 0;
    }
    size_t count = 0;
    for (; variables[count].name != nullptr; ++count)
    {
        registry.BindVariable(variables[count].name, variables[count].address);
    }
    // XDK internals XbSymbolDatabase does not name, found by reading the
    // lifted library (docs/analysis/d3d8-adapter.md section 7).
    for (const auto& internal : kXdkInternals)
    {
        registry.BindVariable(internal.name, internal.address);
        ++count;
    }
    return count;
}

const char* LanguageName(Language language)
{
    const auto index = static_cast<uint32_t>(language);
    return index < kLanguageCount ? kLanguageNames[index] : "Unknown";
}

bool ParseLanguage(const std::string& text, Language* language)
{
    for (uint32_t index = 0; index < kLanguageCount; ++index)
    {
        const char* name = kLanguageNames[index];
        if (text.size() == std::strlen(name) &&
            std::equal(text.begin(), text.end(), name, [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            }))
        {
            *language = static_cast<Language>(index);
            return true;
        }
    }
    return false;
}

bool ApplyLanguage(const rexbox::xbe::XbeImage& image, rexbox::runtime::GuestMemory& memory, Language language)
{
    if (IdentifyXbe(image) != BuiltVariant() || static_cast<uint32_t>(language) >= kLanguageCount ||
        !memory.IsValid(kLanguageIndexAddress, 4))
    {
        return false;
    }
    memory.Write32(kLanguageIndexAddress, static_cast<uint32_t>(language));
    std::printf("[jsrf] language: %s\n", LanguageName(language));
    return true;
}

}  // namespace rejsrf
