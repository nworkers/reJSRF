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

#include <cstdio>
#include <cstdlib>
#include <string>

namespace rexbox::hle::d3d8
{

namespace
{

uint32_t g_capture_frame = 0;  // the frame being captured, 0 when none
FILE* g_file = nullptr;
uint32_t g_index = 0;

std::string Path(uint32_t frame, const char* extension)
{
    const char* directory = std::getenv("REXBOX_FRAME_DUMP_DIR");
    std::string path = directory != nullptr ? std::string(directory) + "/" : std::string();
    return path + "capture_" + std::to_string(frame) + extension;
}

// "header(format)" for a stage's texture, or "-".
std::string TextureText(GuestAddress header)
{
    if (header == 0)
    {
        return "-";
    }
    const uint32_t format = (Memory().Read32(header + header::kFormat) >> header::kFormatFormatShift) & 0xFF;
    return Format("%08X(fmt %02X)", header, format);
}

std::string StageText(uint32_t s)
{
    return Format("st%u %u(%X,%X,%X)/%u(%X,%X) tci %X tex %s", s, TextureStateValue(s, tss::kColorOp),
                  TextureStateValue(s, tss::kColorArg0), TextureStateValue(s, tss::kColorArg1),
                  TextureStateValue(s, tss::kColorArg2), TextureStateValue(s, tss::kAlphaOp),
                  TextureStateValue(s, tss::kAlphaArg1), TextureStateValue(s, tss::kAlphaArg2),
                  TextureStateValue(s, tss::kTexCoordIndex), TextureText(Pipeline().textures[s]).c_str());
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

void CaptureDraw(const char* name, uint32_t primitive, uint32_t count)
{
    const PipelineState& pipeline = Pipeline();
    const D3dState& state = State();
    const auto rsv = [](uint32_t s) { return RenderStateValue(s); };
    std::string line = Format(
        "#%u %s prim %u count %u | vs %08X ps %08X | rt %08X | cull %X front %X | z %u f%X w%u | "
        "blend %u %X/%X op %X | atest %u f%X ref %X | stencil %u f%X ref %X m%X w%X ops %X/%X/%X | "
        "cw %08X | tf %08X | fog %u | light %u",
        g_index++, name, primitive, count, pipeline.vertex_shader, pipeline.pixel_shader, state.render_target,
        rsv(rs::kCullMode), rsv(rs::kFrontFace), rsv(rs::kZEnable), rsv(rs::kZFunc), rsv(rs::kZWriteEnable),
        rsv(rs::kAlphaBlendEnable), rsv(rs::kSrcBlend), rsv(rs::kDestBlend), rsv(rs::kBlendOp),
        rsv(rs::kAlphaTestEnable), rsv(rs::kAlphaFunc), rsv(rs::kAlphaRef), rsv(rs::kStencilEnable),
        rsv(rs::kStencilFunc), rsv(rs::kStencilRef), rsv(rs::kStencilMask), rsv(rs::kStencilWriteMask),
        rsv(rs::kStencilFail), rsv(rs::kStencilZFail), rsv(rs::kStencilPass), rsv(rs::kColorWriteEnable),
        rsv(rs::kTextureFactor), rsv(rs::kFogEnable), rsv(rs::kLighting));
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
    std::fprintf(g_file, "%s\n", line.c_str());
}

void CaptureClear(uint32_t flags, uint32_t color, float z, uint32_t stencil)
{
    std::fprintf(g_file, "#%u Clear flags %X color %08X z %g stencil %X | rt %08X\n", g_index++, flags, color, z,
                 stencil, State().render_target);
}

}  // namespace rexbox::hle::d3d8
