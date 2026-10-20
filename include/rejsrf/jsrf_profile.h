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

// One XBE build the project supports, identified by its certificate.
struct XbeVariant
{
    const char* label;
    uint32_t title_id;
    uint32_t certificate_time;
    uint32_t certificate_version;
    uint32_t image_time;
};

// The variant `image` matches, or nullptr for an unsupported build.
const XbeVariant* IdentifyXbe(const rexbox::xbe::XbeImage& image);

// The variant the recompiled code and title bindings are made for.
const XbeVariant* PrimaryVariant();

// Binds what the HLE needs to know about the title's statically linked XDK
// code: the identified XDK variables (device types and the like), for the
// primary variant only. XDK functions reach the HLE through generated
// overrides instead. Returns the number of bindings made.
size_t BindTitleHle(const rexbox::xbe::XbeImage& image, rexbox::runtime::HleRegistry& registry);

// The game's language: the value of the language index each build carries as
// a constant (US: 1, Japan: 0). Every build has the assets of all five
// (docs/design/20261020-i104-language-option.md).
enum class Language : uint32_t
{
    kJapanese = 0,
    kEnglish = 1,
    kFrench = 2,
    kGerman = 3,
    kSpanish = 4,
};

constexpr uint32_t kLanguageCount = 5;

// "Japanese", "English", ...: the rejsrf.ini value and the menu label.
const char* LanguageName(Language language);

// Reads a LanguageName, ignoring case. Returns false for anything else.
bool ParseLanguage(const std::string& text, Language* language);

// Sets the language the game runs in by writing the index into the loaded
// image; call it after the XBE is loaded and before its entry runs, because
// some objects keep the value they were created with. Primary variant only;
// returns false (and writes nothing) for any other build.
bool ApplyLanguage(const rexbox::xbe::XbeImage& image, rexbox::runtime::GuestMemory& memory, Language language);

using RecompiledFunction = void (*)();

using RecompiledLookup = RecompiledFunction (*)(uint32_t guest_va);

// The generated guest-VA-to-function lookup, or nullptr when the build has no
// recompiled code (gen/ was not generated). Lifted for the primary variant only.
RecompiledLookup RecompiledLookupFunction();

// The XDK variables identified in the primary variant (generated with the
// recompiled code; the table ends with a null name), or nullptr when the build
// has no recompiled code.
const RexboxXdkVariable* RecompiledXdkVariables();

// Builds the generated code's flat dispatch table. Returns false when there is
// no recompiled code or the table could not be built (lookups then fall back
// to a binary search).
bool InitializeRecompiledDispatch();

}  // namespace rejsrf

#endif  // REJSRF_JSRF_PROFILE_H
