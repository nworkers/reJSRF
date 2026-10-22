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
#include <vector>

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

// Indexed by the Language value.
constexpr std::array<const char*, kLanguageCount> kLanguageNames = {
    "Japanese", "English", "Korean",
};

// The built variant's sections as on the disc: 64-bit FNV-1a of each
// section's raw bytes (hashes only; docs/design/20261023-i113-korean-language.md).
// The text patch rewrites .rdata and .data and may add sections; every other
// section must stay as it is, since the recompiled code was lifted from it.
struct SectionHash
{
    const char* name;
    uint64_t hash;
};

constexpr const char* kTextSections[] = {".rdata", ".data"};

#if defined(REJSRF_VARIANT_JP)
// The Japanese build: no Korean patch exists for it (the table stays empty,
// and LanguageAvailable refuses Korean).
constexpr std::array<SectionHash, 0> kSectionHashes = {};
#else
constexpr std::array<SectionHash, 11> kSectionHashes = {{
    {".text", 0x619511F233A0A407ull},
    {"D3D", 0xEE4754220EDB0E15ull},
    {"DSOUND", 0xA7DA262B86E57322ull},
    {"MMATRIX", 0x294679321E0C3838ull},
    {"XGRPH", 0xAD9134313C4B2009ull},
    {"XPP", 0x329E0CD7CBE7F886ull},
    {".rdata", 0xBF3C1463F290A703ull},
    {".data", 0x23051263BD7803BAull},
    {"DOLBY", 0x8D52466CB67EB05Full},
    {"$$XTIMAGE", 0xB351F8B61382FE37ull},
    {"$$XSIMAGE", 0xCC6D4EF4188CA78Aull},
}};
#endif

uint64_t Fnv1a64(const uint8_t* data, size_t size)
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 0x100000001B3ull;
    }
    return hash;
}

bool IsTextSection(const char* name)
{
    for (const char* text : kTextSections)
    {
        if (std::strcmp(name, text) == 0)
        {
            return true;
        }
    }
    return false;
}

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

bool LanguageAvailable(Language language)
{
    if (language == Language::kKorean)
    {
        return !kSectionHashes.empty();
    }
    return static_cast<uint32_t>(language) < kLanguageCount;
}

uint32_t GameLanguageIndex(Language language)
{
    return language == Language::kKorean ? static_cast<uint32_t>(Language::kEnglish)
                                         : static_cast<uint32_t>(language);
}

XbeText ClassifyXbeText(const rexbox::xbe::XbeImage& image)
{
    if (IdentifyXbe(image) != BuiltVariant())
    {
        return XbeText::kOtherBuild;
    }
    const std::vector<uint8_t>& bytes = image.bytes();
    const auto& sections = image.sections();
    bool text_changed = false;
    for (const SectionHash& expected : kSectionHashes)
    {
        const auto section = std::find_if(sections.begin(), sections.end(),
                                          [&](const auto& candidate) { return candidate.name == expected.name; });
        if (section == sections.end() || uint64_t{section->raw_address} + section->raw_size > bytes.size())
        {
            return XbeText::kCodeChanged;
        }
        const bool same = Fnv1a64(bytes.data() + section->raw_address, section->raw_size) == expected.hash;
        if (IsTextSection(expected.name))
        {
            text_changed |= !same;
        }
        else if (!same)
        {
            return XbeText::kCodeChanged;
        }
    }
    return text_changed ? XbeText::kKorean : XbeText::kOriginal;
}

const char* XbeTextName(XbeText text)
{
    switch (text)
    {
    case XbeText::kOriginal:
        return "original";
    case XbeText::kKorean:
        return "Korean patch";
    case XbeText::kCodeChanged:
        return "changed code";
    case XbeText::kOtherBuild:
        return "another build";
    }
    return "unknown";
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
    if (IdentifyXbe(image) != BuiltVariant() || !LanguageAvailable(language) ||
        !memory.IsValid(kLanguageIndexAddress, 4))
    {
        return false;
    }
    memory.Write32(kLanguageIndexAddress, GameLanguageIndex(language));
    std::printf("[jsrf] language: %s\n", LanguageName(language));
    return true;
}

}  // namespace rejsrf
