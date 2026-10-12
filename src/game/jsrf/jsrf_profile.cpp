#include "rejsrf/jsrf_profile.h"

#include "rexbox/recomp/recomp_hle_entry.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/xbe/xbe_image.h"

#include <array>
#include <cstdint>

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

const XbeVariant* PrimaryVariant()
{
    return &kVariants[0];
}

size_t BindTitleHle(const rexbox::xbe::XbeImage& image, rexbox::runtime::HleRegistry& registry)
{
    // The variable addresses were identified in the primary variant's XBE.
    const RexboxXdkVariable* variables = RecompiledXdkVariables();
    if (variables == nullptr || IdentifyXbe(image) != PrimaryVariant())
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

}  // namespace rejsrf
