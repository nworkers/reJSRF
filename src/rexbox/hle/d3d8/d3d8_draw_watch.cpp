#include "d3d8_draw_watch.h"

#include "d3d8_globals.h"
#include "d3d8_internal.h"
#include "d3d8_render_state.h"
#include "d3d8_shader.h"
#include "d3d8_state.h"
#include "d3d8_trace.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr uint32_t kMaxLinesPerHandle = 64;
constexpr uint32_t kMaxLinesPerHandleAll = 4;  // REXBOX_VSH_WATCH=all
constexpr uint32_t kConstantRegisters = 192;

struct Watch
{
    uint32_t first = 96;
    uint32_t last = 103;
    std::unordered_set<uint64_t> seen;
};

// REXBOX_VSH_WATCH=all: every program handle, with the default range.
bool WatchAll()
{
    static const bool all = [] {
        const char* setting = std::getenv("REXBOX_VSH_WATCH");
        return setting != nullptr && std::strcmp(setting, "all") == 0;
    }();
    return all;
}

std::unordered_map<uint32_t, Watch>& Watches()
{
    static std::unordered_map<uint32_t, Watch> watches = [] {
        std::unordered_map<uint32_t, Watch> parsed;
        const char* setting = std::getenv("REXBOX_VSH_WATCH");
        if (setting == nullptr || WatchAll())
        {
            return parsed;
        }
        const char* cursor = setting;
        while (*cursor != '\0')
        {
            char* end = nullptr;
            const uint32_t handle = static_cast<uint32_t>(std::strtoul(cursor, &end, 0));
            if (end == cursor)
            {
                break;
            }
            Watch watch;
            cursor = end;
            if (*cursor == ':')
            {
                watch.first = static_cast<uint32_t>(std::strtoul(cursor + 1, &end, 10));
                cursor = end;
                watch.last = watch.first;
                if (*cursor == '-')
                {
                    watch.last = static_cast<uint32_t>(std::strtoul(cursor + 1, &end, 10));
                    cursor = end;
                }
            }
            if (watch.last >= kConstantRegisters)
            {
                watch.last = kConstantRegisters - 1;
            }
            parsed[handle] = watch;
            while (*cursor == ',' || *cursor == ' ')
            {
                ++cursor;
            }
        }
        return parsed;
    }();
    return watches;
}

}  // namespace

bool DrawWatchEnabled()
{
    static const bool enabled = WatchAll() || !Watches().empty();
    return enabled;
}

void WatchProgramDraw(uint32_t vertex_shader, const FixedFunctionKey& fragment_key)
{
    auto it = Watches().find(vertex_shader);
    if (it == Watches().end())
    {
        if (!WatchAll())
        {
            return;
        }
        it = Watches().emplace(vertex_shader, Watch{}).first;
    }
    Watch& watch = it->second;
    if (watch.seen.size() >= (WatchAll() ? kMaxLinesPerHandleAll : kMaxLinesPerHandle))
    {
        return;
    }
    const PipelineState& pipeline = Pipeline();
    const float* constants = VertexShaderConstants();
    const uint32_t state[] = {
        RenderStateValue(rs::kCullMode),          RenderStateValue(rs::kAlphaBlendEnable),
        RenderStateValue(rs::kSrcBlend),          RenderStateValue(rs::kDestBlend),
        RenderStateValue(rs::kZWriteEnable),      RenderStateValue(rs::kTextureFactor),
        TextureStateValue(0, tss::kColorOp),      TextureStateValue(0, tss::kColorArg1),
        TextureStateValue(0, tss::kColorArg2),    TextureStateValue(0, tss::kAlphaOp),
        TextureStateValue(0, tss::kAlphaArg1),    TextureStateValue(0, tss::kAlphaArg2),
        pipeline.textures[0],                     pipeline.pixel_shader,
    };
    uint64_t hash = 1469598103934665603ull;
    for (uint32_t r = watch.first; r <= watch.last; ++r)
    {
        uint64_t bits[2];
        std::memcpy(bits, constants + r * 4, sizeof(bits));
        hash = HashMix(HashMix(hash, bits[0]), bits[1]);
    }
    for (const uint32_t value : state)
    {
        hash = HashMix(hash, value);
    }
    if (!watch.seen.insert(hash).second)
    {
        return;
    }
    std::string line = Format("[vsh watch] frame %u vs %u:", State().frame_count, vertex_shader);
    for (uint32_t r = watch.first; r <= watch.last; ++r)
    {
        const float* c = constants + r * 4;
        line += Format(" c[%u]=(%g %g %g %g)", r, c[0], c[1], c[2], c[3]);
    }
    line += Format("; cull %X, blend %u %X/%X, zwrite %u, tfactor %08X, stage0 color %u(%X,%X) alpha %u(%X,%X) "
                   "texture %08X, pixel shader %08X; key %s",
                   state[0], state[1], state[2], state[3], state[4], state[5], state[6], state[7], state[8], state[9],
                   state[10], state[11], state[12], state[13], fragment_key.Serialize().c_str());
    std::fprintf(stderr, "%s\n", line.c_str());
}

}  // namespace rexbox::hle::d3d8
