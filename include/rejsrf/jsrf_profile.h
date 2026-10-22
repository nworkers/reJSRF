// Title profile: Jet Set Radio Future.
//
// The only place where the title's identity and title-specific bindings are
// declared. The reusable runtime (rexbox) asks nothing about the title; the
// app hands it this profile.

#ifndef REJSRF_JSRF_PROFILE_H
#define REJSRF_JSRF_PROFILE_H

#include <cstdint>
#include <string>

namespace rexbox::runtime
{
class GuestMemory;
class HleRegistry;
}

namespace rexbox::xbe
{
class XbeImage;
}

struct RexboxXdkVariable;

namespace rejsrf
{

// One XBE build the project supports, identified by its certificate (the
// file's content; its name says nothing about the build).
struct XbeVariant
{
    const char* id;     // "jsrf_us", "jsrf_jp": the CMake REJSRF_VARIANT value
    const char* tag;    // "US", "JP": short form for the build identity
    const char* label;
    uint32_t title_id;
    uint32_t certificate_time;
    uint32_t certificate_version;
    uint32_t image_time;
};

// The variant `image` matches, or nullptr for an unsupported build.
const XbeVariant* IdentifyXbe(const rexbox::xbe::XbeImage& image);

// The variant this executable is built for (CMake REJSRF_VARIANT): its
// recompiled code, title bindings and data addresses
// (docs/design/20261020-i108-japanese-variant.md).
const XbeVariant* BuiltVariant();

// Binds what the HLE needs to know about the title's statically linked XDK
// code: the identified XDK variables (device types and the like), for the
// built variant only. XDK functions reach the HLE through generated
// overrides instead. Returns the number of bindings made.
size_t BindTitleHle(const rexbox::xbe::XbeImage& image, rexbox::runtime::HleRegistry& registry);

// The game's language. Japanese and English are values of the language index
// each build carries as a constant (US: 1, Japan: 0). The index also has
// French (2), German (3) and Spanish (4), left out because the disc carries no
// localization for them beyond a few screens
// (docs/design/20261020-i104-language-option.md 3.4). Korean is not an index
// value: it runs English mode on a game folder the JSRF-kr-patch tool made
// (docs/design/20261023-i113-korean-language.md).
enum class Language : uint32_t
{
    kJapanese = 0,
    kEnglish = 1,
    kKorean = 2,
};

constexpr uint32_t kLanguageCount = 3;

// The language the built variant's disc runs in by itself (USA: English,
// Japan: Japanese); the setting's default.
Language NativeLanguage();

// Whether the built variant can run `language`: Korean is patched onto the US
// build, so the Japanese build cannot show it.
bool LanguageAvailable(Language language);

// The value the game's language index takes for `language` (Korean: English).
uint32_t GameLanguageIndex(Language language);

// Which text an XBE of the built variant's certificate carries, told from its
// section bytes (64-bit FNV-1a hashes against the built variant's).
enum class XbeText
{
    kOriginal,      // every section as on the disc
    kKorean,        // the code as on the disc, the text data patched (JSRF-kr-patch)
    kCodeChanged,   // a code or library section differs: the lifted code would not match
    kOtherBuild,    // not the built variant's certificate
};

XbeText ClassifyXbeText(const rexbox::xbe::XbeImage& image);

// "original", "Korean patch", ...: for messages.
const char* XbeTextName(XbeText text);

// "Japanese", "English", ...: the rejsrf.ini value and the menu label.
const char* LanguageName(Language language);

// Reads a LanguageName, ignoring case. Returns false for anything else.
bool ParseLanguage(const std::string& text, Language* language);

// Sets the language the game runs in by writing the index into the loaded
// image; call it after the XBE is loaded and before its entry runs, because
// some objects keep the value they were created with. Built variant only;
// returns false (and writes nothing) for any other build.
bool ApplyLanguage(const rexbox::xbe::XbeImage& image, rexbox::runtime::GuestMemory& memory, Language language);

using RecompiledFunction = void (*)();

using RecompiledLookup = RecompiledFunction (*)(uint32_t guest_va);

// The generated guest-VA-to-function lookup, or nullptr when the build has no
// recompiled code (gen/ was not generated). Lifted for the built variant only.
RecompiledLookup RecompiledLookupFunction();

// The XDK variables identified in the built variant (generated with the
// recompiled code; the table ends with a null name), or nullptr when the build
// has no recompiled code.
const RexboxXdkVariable* RecompiledXdkVariables();

// Builds the generated code's flat dispatch table. Returns false when there is
// no recompiled code or the table could not be built (lookups then fall back
// to a binary search).
bool InitializeRecompiledDispatch();

}  // namespace rejsrf

#endif  // REJSRF_JSRF_PROFILE_H
