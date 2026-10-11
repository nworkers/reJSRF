#include "rejsrf/jsrf_profile.h"

#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

#include <array>

namespace rejsrf
{

namespace
{

// Certificate identity of each known build (docs/analysis/jsrf-xbe-overview.md).
// Both share title id 5345000A, XDK 4134 and the entry point, but their code
// differs, so guest addresses from one do not apply to the other.
constexpr std::array<XbeVariant, 2> kVariants = {{
    // Primary target: the USA retail build, shipped as default_eng.xbe on this
    // disc (MD5 f4e366bfdceee551a09f42219c90d84e).
    {"USA (region 1)", 0x5345000A, 0x3C5881D9, 0x00000001, 0x3C558EA7},
    // Recognized only: the Japan-region build, default.xbe on this disc.
    {"Japan (region 2)", 0x5345000A, 0x3C56850C, 0x00000001, 0x3C559627},
}};

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

size_t BindTitleHle(const rexbox::xbe::XbeImage& /*image*/, rexbox::runtime::HleRegistry& /*registry*/)
{
    // XDK function addresses come from function identification, which is the
    // next task. Until then the title binds nothing.
    return 0;
}

}  // namespace rejsrf
