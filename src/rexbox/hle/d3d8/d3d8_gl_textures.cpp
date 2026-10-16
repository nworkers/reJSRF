#include "d3d8_gl_textures.h"

#include "d3d8_gl.h"
#include "d3d8_gl_targets.h"
#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_render_state.h"
#include "d3d8_resource.h"
#include "d3d8_texture_format.h"
#include "d3d8_trace.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kHashSamples = 256;

// D3DTEXTUREADDRESS and D3DTEXTUREFILTERTYPE (Xbox; inferred, PC numbering
// plus CLAMPTOEDGE 5).
constexpr uint32_t kAddressWrap = 1;
constexpr uint32_t kAddressMirror = 2;
constexpr uint32_t kAddressBorder = 4;
constexpr uint32_t kFilterNone = 0;
constexpr uint32_t kFilterPoint = 1;

struct CachedTexture
{
    BoundTexture bound;
    GLenum target = kGlTexture2D;
    uint32_t format_field = 0;
    uint32_t size_field = 0;
    uint32_t data = 0;
    uint32_t hash = 0;
    uint32_t checked_frame = 0xFFFFFFFF;
    bool written = false;
};

std::unordered_map<GuestAddress, CachedTexture> g_textures;

// One sampler object per combination of address modes and filters, so a
// draw binds one instead of changing a shared sampler's parameters.
std::unordered_map<uint64_t, GLuint> g_samplers;

// What each texture unit holds, valid while `epoch` is the GL state epoch.
struct UnitBinding
{
    uint32_t epoch = 0;
    GLenum target = 0;
    GLuint texture = 0;
    GLuint sampler = 0;
};
UnitBinding g_units[globals::kTextureStages];

// After an upload binds on unit 0, or a delete unbinds a texture everywhere.
void ForgetUnitBindings()
{
    for (UnitBinding& unit : g_units)
    {
        unit = {};
    }
}

// A cheap fingerprint of level 0: evenly spaced DWORDs.
uint32_t SampleHash(uint32_t data, uint32_t bytes)
{
    runtime::GuestMemory& memory = Memory();
    uint32_t hash = 2166136261u ^ bytes;
    const uint32_t words = bytes / 4;
    if (words == 0 || !memory.IsValid(data, bytes))
    {
        return hash;
    }
    const uint32_t step = std::max<uint32_t>(1, words / kHashSamples);
    for (uint32_t i = 0; i < words; i += step)
    {
        hash = (hash ^ memory.Read32(data + i * 4)) * 16777619u;
    }
    return hash;
}

bool Upload(GuestAddress header, CachedTexture* cached)
{
    GlFunctions& gl = Gl();
    runtime::GuestMemory& memory = Memory();
    const uint32_t format_field = cached->format_field;
    const uint32_t format = (format_field >> header::kFormatFormatShift) & 0xFF;
    const bool linear = cached->size_field != 0;
    const bool cube = (format_field & header::kFormatCubeMap) != 0;
    const uint32_t dimension = (format_field >> header::kFormatDimensionShift) & 0xF;
    TextureFormatInfo info;
    if (!DescribeTextureFormat(format, &info) || info.palette)
    {
        ReportOnce(Format("texfmt %02X", format), Format("texture format %02X is not decoded yet (header %08X); "
                                                          "sampled as white",
                                                          format, header));
        return false;
    }
    if (cube)
    {
        ReportOnce("cube", Format("cube texture %08X: cube maps are not uploaded yet; sampled as white", header));
        return false;
    }
    const uint32_t levels = linear ? 1 : std::max<uint32_t>(1, (format_field >> header::kFormatMipLevelsShift) & 0xF);
    const GLenum target = dimension == 3 ? kGlTexture3D : kGlTexture2D;
    if (cached->bound.texture == 0 || cached->target != target)
    {
        if (cached->bound.texture != 0)
        {
            gl.DeleteTextures(1, &cached->bound.texture);
        }
        gl.GenTextures(1, &cached->bound.texture);
    }
    cached->target = target;
    ForgetUnitBindings();
    gl.ActiveTexture(kGlTexture0);
    gl.BindTexture(target, cached->bound.texture);
    gl.PixelStorei(kGlUnpackAlignment, 1);
    std::vector<uint8_t> rgba;
    uint32_t uploaded = 0;
    for (uint32_t level = 0; level < levels; ++level)
    {
        const LevelLayout layout = HeaderLevelLayout(header, level);
        // A level's texel size; the layout of a compressed level is padded to 4x4.
        const uint32_t log2_width = (format_field >> header::kFormatLog2WidthShift) & 0xF;
        const uint32_t log2_height = (format_field >> header::kFormatLog2HeightShift) & 0xF;
        const uint32_t width = linear ? layout.width : std::max<uint32_t>(1, (1u << log2_width) >> level);
        const uint32_t height = linear ? layout.height : std::max<uint32_t>(1, (1u << log2_height) >> level);
        const uint32_t depth = layout.depth;
        const uint32_t address = (cached->data + TextureLevelOffset(header, level)) & kPhysicalMask;
        const uint32_t bytes = LevelBytes(format, std::max(width, info.layout == PixelLayout::kCompressed ? 4u : 1u),
                                          std::max(height, info.layout == PixelLayout::kCompressed ? 4u : 1u), depth,
                                          linear ? layout.pitch : 0);
        if (!memory.IsValid(address, bytes))
        {
            ReportOnce(Format("texmem %08X", header),
                       Format("texture %08X level %u data %08X+%X is outside guest memory", header, level, address,
                              bytes));
            break;
        }
        // Compressed levels smaller than a block decode from the padded block.
        const uint32_t decode_width = info.layout == PixelLayout::kCompressed ? std::max(width, 4u) : width;
        const uint32_t decode_height = info.layout == PixelLayout::kCompressed ? std::max(height, 4u) : height;
        if (!DecodeTextureLevel(format, memory.Translate(address), decode_width, decode_height, depth,
                                linear ? layout.pitch : 0, &rgba))
        {
            break;
        }
        if (decode_width != width || decode_height != height)
        {
            // Keep the top-left texels of the padded block.
            std::vector<uint8_t> cropped(static_cast<size_t>(width) * height * 4);
            for (uint32_t y = 0; y < height; ++y)
            {
                std::copy_n(rgba.data() + static_cast<size_t>(y) * decode_width * 4, width * 4,
                            cropped.data() + static_cast<size_t>(y) * width * 4);
            }
            rgba.swap(cropped);
        }
        if (target == kGlTexture3D)
        {
            gl.TexImage3D(target, static_cast<GLint>(level), static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height), static_cast<GLsizei>(depth), 0, kGlRgba, kGlUnsignedByte,
                          rgba.data());
        }
        else
        {
            gl.TexImage2D(target, static_cast<GLint>(level), static_cast<GLint>(kGlRgba8), static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height), 0, kGlRgba, kGlUnsignedByte, rgba.data());
        }
        if (level == 0)
        {
            cached->bound.width = width;
            cached->bound.height = height;
        }
        ++uploaded;
    }
    if (uploaded == 0)
    {
        return false;
    }
    gl.TexParameteri(target, kGlTextureBaseLevel, 0);
    gl.TexParameteri(target, kGlTextureMaxLevel, static_cast<GLint>(uploaded - 1));
    cached->bound.levels = uploaded;
    cached->bound.format = format;
    cached->bound.kind = target == kGlTexture3D ? SamplerKind::k3D : SamplerKind::k2D;
    cached->bound.unnormalized = linear;
    TraceOnce(Format("upload %02X %u %u", format, cached->bound.width, cached->bound.height),
              Format("texture upload: format %02X %ux%u, %u level(s)%s", format, cached->bound.width,
                     cached->bound.height, uploaded, linear ? ", linear (texel coordinates)" : ""));
    return true;
}

GLint AddressMode(uint32_t value)
{
    switch (value)
    {
    case kAddressWrap:
        return static_cast<GLint>(kGlRepeat);
    case kAddressMirror:
        return static_cast<GLint>(kGlMirroredRepeat);
    case kAddressBorder:
        ReportOnce("border", "texture address mode BORDER drawn as CLAMP (GLES has no border color)");
        return static_cast<GLint>(kGlClampToEdge);
    default:
        return static_cast<GLint>(kGlClampToEdge);
    }
}

}  // namespace

BoundTexture ResolveTexture(GuestAddress header)
{
    if (header == 0)
    {
        return {};
    }
    runtime::GuestMemory& memory = Memory();
    const uint32_t format_field = memory.Read32(header + header::kFormat);
    const uint32_t size_field = memory.Read32(header + header::kSize);
    const uint32_t data = memory.Read32(header + header::kData);
    // Pixels a render target holds live on the GPU, not in guest memory.
    GuestAddress surface = 0;
    BoundTexture target;
    target.texture = SampleRenderTarget(data, &surface, &target.width, &target.height);
    if (target.texture != 0)
    {
        if (FirstReport("rt sample", header))
        {
            ReportOnce(Format("rt sample %08X", header),
                       Format("texture %08X samples render target %08X at %08X (%ux%u)", header, surface,
                              data & kPhysicalMask, target.width, target.height));
        }
        target.kind = SamplerKind::k2D;
        target.unnormalized = size_field != 0;
        return target;
    }
    CachedTexture& cached = g_textures[header];
    bool upload = cached.bound.texture == 0 || cached.written || format_field != cached.format_field ||
                  size_field != cached.size_field || data != cached.data;
    // Writes without a lock: compare a sampled hash once per frame.
    const uint32_t frame = State().frame_count;
    const LevelLayout layout0 = HeaderLevelLayout(header, 0);
    if (!upload && cached.checked_frame != frame)
    {
        cached.checked_frame = frame;
        upload = SampleHash(data & kPhysicalMask, layout0.size) != cached.hash;
    }
    if (upload)
    {
        cached.format_field = format_field;
        cached.size_field = size_field;
        cached.data = data;
        cached.written = false;
        cached.checked_frame = frame;
        cached.hash = SampleHash(data & kPhysicalMask, layout0.size);
        if (!Upload(header, &cached))
        {
            cached.bound.kind = SamplerKind::kNone;
        }
    }
    return cached.bound.kind == SamplerKind::kNone ? BoundTexture{} : cached.bound;
}

// The sampler object for these parameters, created on first use.
GLuint Sampler(GLint wrap_s, GLint wrap_t, GLint wrap_r, GLenum mag_filter, GLenum min_filter)
{
    // GL enum values fit in 16 bits.
    const uint64_t key = (static_cast<uint64_t>(wrap_s & 0xFFFF) << 48) |
                         (static_cast<uint64_t>(wrap_t & 0xFFFF) << 32) |
                         (static_cast<uint64_t>(wrap_r & 0xFFFF) << 16) |
                         (static_cast<uint64_t>(mag_filter == kGlNearest ? 1 : 0) << 15) | (min_filter & 0x7FFF);
    const auto it = g_samplers.find(key);
    if (it != g_samplers.end())
    {
        return it->second;
    }
    GlFunctions& gl = Gl();
    GLuint sampler = 0;
    gl.GenSamplers(1, &sampler);
    gl.SamplerParameteri(sampler, kGlTextureWrapS, wrap_s);
    gl.SamplerParameteri(sampler, kGlTextureWrapT, wrap_t);
    gl.SamplerParameteri(sampler, kGlTextureWrapR, wrap_r);
    gl.SamplerParameteri(sampler, kGlTextureMagFilter, static_cast<GLint>(mag_filter));
    gl.SamplerParameteri(sampler, kGlTextureMinFilter, static_cast<GLint>(min_filter));
    g_samplers.emplace(key, sampler);
    return sampler;
}

void BindStageTexture(uint32_t stage, const BoundTexture& texture)
{
    GlFunctions& gl = Gl();
    const bool mag_point = TextureStateValue(stage, tss::kMagFilter) == kFilterPoint;
    const bool min_point = TextureStateValue(stage, tss::kMinFilter) == kFilterPoint;
    const uint32_t mip = texture.levels > 1 ? TextureStateValue(stage, tss::kMipFilter) : kFilterNone;
    GLenum min_filter = min_point ? kGlNearest : kGlLinear;
    if (mip == kFilterPoint)
    {
        min_filter = min_point ? kGlNearestMipmapNearest : kGlLinearMipmapNearest;
    }
    else if (mip != kFilterNone)
    {
        min_filter = min_point ? kGlNearestMipmapLinear : kGlLinearMipmapLinear;
    }
    const GLuint sampler = Sampler(AddressMode(TextureStateValue(stage, tss::kAddressU)),
                                   AddressMode(TextureStateValue(stage, tss::kAddressV)),
                                   AddressMode(TextureStateValue(stage, tss::kAddressW)),
                                   mag_point ? kGlNearest : kGlLinear, min_filter);
    const GLenum target = texture.kind == SamplerKind::k3D ? kGlTexture3D : kGlTexture2D;

    UnitBinding& unit = g_units[stage];
    const uint32_t epoch = GlStateEpoch();
    if (unit.epoch != epoch)
    {
        unit = {};
        unit.epoch = epoch;
    }
    if (unit.target != target || unit.texture != texture.texture)
    {
        gl.ActiveTexture(kGlTexture0 + stage);
        gl.BindTexture(target, texture.texture);
        unit.target = target;
        unit.texture = texture.texture;
    }
    if (unit.sampler != sampler)
    {
        gl.BindSampler(stage, sampler);
        unit.sampler = sampler;
    }
}

void MarkTextureWritten(GuestAddress header)
{
    const auto it = g_textures.find(header);
    if (it != g_textures.end())
    {
        it->second.written = true;
    }
}

void ForgetTexture(GuestAddress header)
{
    const auto it = g_textures.find(header);
    if (it == g_textures.end())
    {
        return;
    }
    if (it->second.bound.texture != 0)
    {
        Gl().DeleteTextures(1, &it->second.bound.texture);
        ForgetUnitBindings();
    }
    g_textures.erase(it);
}

}  // namespace rexbox::hle::d3d8
