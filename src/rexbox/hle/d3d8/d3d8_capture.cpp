#include "d3d8_capture.h"

#include "d3d8_fvf.h"
#include "d3d8_gl_targets.h"
#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_render_state.h"
#include "d3d8_resource.h"
#include "d3d8_shader.h"
#include "d3d8_state.h"
#include "d3d8_trace.h"

#include "rexbox/platform/display.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kSmallDraw = 8;       // draws with at most this many vertices list them
constexpr uint32_t kListedVertices = 4;  // .. this many of them

uint32_t g_capture_frame = 0;  // the frame being captured, 0 when none
FILE* g_file = nullptr;
uint32_t g_index = 0;

// How many of the latest writers each pixel remembers.
constexpr uint32_t kOwnerDepth = 3;

// Per render target written in the captured frame: its pixels after the last
// call, and per pixel the calls (index + 1) that changed it last, latest
// first, with the color each left.
struct Ownership
{
    std::vector<uint8_t> pixels;
    std::vector<uint32_t> owner[kOwnerDepth];
    std::vector<uint8_t> color[kOwnerDepth];  // RGBA8
    uint32_t width = 0;
    uint32_t height = 0;
};
std::unordered_map<GuestAddress, Ownership> g_ownership;

std::string Path(uint32_t frame, const char* suffix)
{
    const char* directory = std::getenv("REXBOX_FRAME_DUMP_DIR");
    std::string path = directory != nullptr ? std::string(directory) + "/" : std::string();
    return path + "capture_" + std::to_string(frame) + suffix;
}

// "header(format, raw format and size fields)" or "-".
std::string HeaderText(GuestAddress header)
{
    if (header == 0)
    {
        return "-";
    }
    runtime::GuestMemory& memory = Memory();
    const uint32_t format_field = memory.Read32(header + header::kFormat);
    const uint32_t size_field = memory.Read32(header + header::kSize);
    return Format("%08X(fmt %02X F%08X S%08X D%08X)", header, (format_field >> header::kFormatFormatShift) & 0xFF,
                  format_field, size_field, memory.Read32(header + header::kData));
}

std::string StageText(uint32_t s)
{
    return Format("st%u %u(%X,%X,%X)/%u(%X,%X) tci %X tex %s", s, TextureStateValue(s, tss::kColorOp),
                  TextureStateValue(s, tss::kColorArg0), TextureStateValue(s, tss::kColorArg1),
                  TextureStateValue(s, tss::kColorArg2), TextureStateValue(s, tss::kAlphaOp),
                  TextureStateValue(s, tss::kAlphaArg1), TextureStateValue(s, tss::kAlphaArg2),
                  TextureStateValue(s, tss::kTexCoordIndex), HeaderText(Pipeline().textures[s]).c_str());
}

// The first vertices of a small draw, as raw DWORDs.
std::string VerticesText(uint32_t count, GuestAddress vertices, uint32_t stride, GuestAddress indices,
                         uint32_t index_base)
{
    runtime::GuestMemory& memory = Memory();
    std::string text;
    const uint32_t words = std::min<uint32_t>(stride / 4, 16);
    for (uint32_t k = 0; k < std::min(count, kListedVertices); ++k)
    {
        const uint32_t index = (indices != 0 ? memory.Read16((indices & kPhysicalMask) + k * 2) : k) + index_base;
        const GuestAddress at = (vertices + index * stride) & kPhysicalMask;
        if (!memory.IsValid(at, words * 4))
        {
            break;
        }
        text += Format(" | v%u:", index);
        for (uint32_t w = 0; w < words; ++w)
        {
            text += Format(" %08X", memory.Read32(at + w * 4));
        }
    }
    return text;
}

Ownership& Owned(GuestAddress target)
{
    auto it = g_ownership.find(target);
    if (it == g_ownership.end())
    {
        Ownership fresh;
        ReadRenderTarget(target, &fresh.pixels, &fresh.width, &fresh.height);
        for (uint32_t k = 0; k < kOwnerDepth; ++k)
        {
            fresh.owner[k].assign(static_cast<size_t>(fresh.width) * fresh.height, 0);
            fresh.color[k].assign(static_cast<size_t>(fresh.width) * fresh.height * 4, 0);
        }
        it = g_ownership.emplace(target, std::move(fresh)).first;
    }
    return it->second;
}

// capture_<frame>_owner.bmp (latest writer), _owner2.bmp and _owner3.bmp
// (the ones before), and _color2.bmp, _color3.bmp (what those left).
void WriteOwnerMaps(GuestAddress shown, uint32_t frame)
{
    const auto it = g_ownership.find(shown);
    if (it == g_ownership.end())
    {
        return;
    }
    const Ownership& owned = it->second;
    for (uint32_t k = 0; k < kOwnerDepth; ++k)
    {
        const std::vector<uint32_t>& owner = owned.owner[k];
        std::vector<uint8_t> rgba(owner.size() * 4, 0);
        for (size_t p = 0; p < owner.size(); ++p)
        {
            rgba[p * 4 + 0] = static_cast<uint8_t>(owner[p] & 0xFF);
            rgba[p * 4 + 1] = static_cast<uint8_t>((owner[p] >> 8) & 0xFF);
        }
        const std::string n = k == 0 ? std::string() : std::to_string(k + 1);
        WriteBmp(Path(frame, ("_owner" + n + ".bmp").c_str()), rgba.data(), owned.width, owned.height);
        if (k != 0)
        {
            WriteBmp(Path(frame, ("_color" + n + ".bmp").c_str()), owned.color[k].data(), owned.width,
                     owned.height);
        }
    }
}

}  // namespace

void CaptureAtPresent(uint32_t frame, GuestAddress shown)
{
    if (g_capture_frame != 0 && frame == g_capture_frame)
    {
        std::fclose(g_file);
        g_file = nullptr;
        const std::string picture = Path(frame, ".bmp");
        const bool written = DumpRenderTarget(shown, picture, State().gamma_ramp);
        WriteOwnerMaps(shown, frame);
        g_ownership.clear();
        std::fprintf(stderr, "[d3d8] capture: frame %u, %u call(s) in %s, picture %s%s\n", frame, g_index,
                     Path(frame, ".txt").c_str(), picture.c_str(), written ? "" : " (not written)");
        g_capture_frame = 0;
    }
    // REXBOX_CAPTURE_FRAME=<n> captures frame n without the key.
    static const uint32_t requested_frame = [] {
        const char* value = std::getenv("REXBOX_CAPTURE_FRAME");
        return value != nullptr ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 0u;
    }();
    const bool key = platform::ConsumeCaptureRequest();
    if (g_capture_frame == 0 && (key || (requested_frame != 0 && frame + 1 == requested_frame)))
    {
        const uint32_t next = frame + 1;
        g_file = std::fopen(Path(next, ".txt").c_str(), "w");
        if (g_file == nullptr)
        {
            std::fprintf(stderr, "[d3d8] capture: cannot write %s\n", Path(next, ".txt").c_str());
            return;
        }
        g_capture_frame = next;
        g_index = 0;
        std::fprintf(g_file, "frame %u: every Clear and draw call in order (state as the title set it)\n", next);
    }
}

bool CaptureActive()
{
    return g_capture_frame != 0;
}

void CaptureDraw(const char* name, uint32_t primitive, uint32_t count, GuestAddress vertices, uint32_t stride,
                 GuestAddress indices, uint32_t index_base)
{
    const PipelineState& pipeline = Pipeline();
    const D3dState& state = State();
    const auto rsv = [](uint32_t s) { return RenderStateValue(s); };
    const Viewport& vp = state.viewport;
    std::string line = Format(
        "#%u %s prim %u count %u | vs %08X ps %08X | rt %s vp %u,%u %ux%u | cull %X front %X | z %u f%X w%u | "
        "blend %u %X/%X op %X | atest %u f%X ref %X | stencil %u f%X ref %X m%X w%X ops %X/%X/%X | "
        "cw %08X | tf %08X | fog %u | light %u",
        g_index++, name, primitive, count, pipeline.vertex_shader, pipeline.pixel_shader,
        HeaderText(state.render_target).c_str(), vp.x, vp.y, vp.width, vp.height, rsv(rs::kCullMode),
        rsv(rs::kFrontFace), rsv(rs::kZEnable), rsv(rs::kZFunc), rsv(rs::kZWriteEnable), rsv(rs::kAlphaBlendEnable),
        rsv(rs::kSrcBlend), rsv(rs::kDestBlend), rsv(rs::kBlendOp), rsv(rs::kAlphaTestEnable), rsv(rs::kAlphaFunc),
        rsv(rs::kAlphaRef), rsv(rs::kStencilEnable), rsv(rs::kStencilFunc), rsv(rs::kStencilRef),
        rsv(rs::kStencilMask), rsv(rs::kStencilWriteMask), rsv(rs::kStencilFail), rsv(rs::kStencilZFail),
        rsv(rs::kStencilPass), rsv(rs::kColorWriteEnable), rsv(rs::kTextureFactor), rsv(rs::kFogEnable),
        rsv(rs::kLighting));
    for (uint32_t s = 0; s < 4; ++s)
    {
        if (s == 0 || pipeline.textures[s] != 0)
        {
            line += " | " + StageText(s);
        }
    }
    if ((pipeline.vertex_shader & fvf::kProgramHandle) != 0)
    {
        const float* c = VertexShaderConstants();
        for (const uint32_t r : {97u, 98u})
        {
            line += Format(" | c[%u]=(%g %g %g %g)", r, c[r * 4], c[r * 4 + 1], c[r * 4 + 2], c[r * 4 + 3]);
        }
    }
    if (count <= kSmallDraw && vertices != 0 && stride != 0)
    {
        line += Format(" | stride %u", stride) + VerticesText(count, vertices, stride, indices, index_base);
    }
    std::fprintf(g_file, "%s\n", line.c_str());
    if (state.render_target != 0)
    {
        Owned(state.render_target);
    }
}

void CaptureClear(uint32_t flags, uint32_t color, float z, uint32_t stencil)
{
    std::fprintf(g_file, "#%u Clear flags %X color %08X z %g stencil %X | rt %08X\n", g_index++, flags, color, z,
                 stencil, State().render_target);
    if (State().render_target != 0)
    {
        Owned(State().render_target);
    }
}

void CaptureAfterCall()
{
    const GuestAddress target = State().render_target;
    if (target == 0)
    {
        return;
    }
    Ownership& owned = Owned(target);
    std::vector<uint8_t> now;
    uint32_t width = 0;
    uint32_t height = 0;
    ReadRenderTarget(target, &now, &width, &height);
    if (width != owned.width || height != owned.height)
    {
        return;
    }
    for (size_t p = 0; p < owned.owner[0].size(); ++p)
    {
        if (std::memcmp(&now[p * 4], &owned.pixels[p * 4], 4) != 0)
        {
            for (uint32_t k = kOwnerDepth - 1; k > 0; --k)
            {
                owned.owner[k][p] = owned.owner[k - 1][p];
                std::memcpy(&owned.color[k][p * 4], &owned.color[k - 1][p * 4], 4);
            }
            owned.owner[0][p] = g_index;  // the call just listed, plus one
            std::memcpy(&owned.color[0][p * 4], &now[p * 4], 4);
        }
    }
    owned.pixels.swap(now);
}

}  // namespace rexbox::hle::d3d8
