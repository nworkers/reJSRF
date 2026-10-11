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
class HleRegistry;
}

namespace rexbox::xbe
{
class XbeImage;
}

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

// Binds the title's statically linked XDK functions to HLE exports. Returns
// the number of bindings made.
size_t BindTitleHle(const rexbox::xbe::XbeImage& image, rexbox::runtime::HleRegistry& registry);

}  // namespace rejsrf

#endif  // REJSRF_JSRF_PROFILE_H
