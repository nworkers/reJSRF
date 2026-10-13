#include "d3d8_ff_shader.h"

#include <cstdio>

namespace rexbox::hle::d3d8
{

namespace
{

std::string Header(bool gles)
{
    return gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
}

std::string Number(uint32_t value)
{
    return std::to_string(value);
}

const char* SamplerType(SamplerKind kind)
{
    switch (kind)
    {
    case SamplerKind::kCube:
        return "samplerCube";
    case SamplerKind::k3D:
        return "sampler3D";
    default:
        return "sampler2D";
    }
}

// The vec4 an argument selects, with its modifiers.
std::string Argument(uint32_t arg, uint32_t stage)
{
    std::string value;
    switch (arg & ta::kSelectMask)
    {
    case ta::kDiffuse:
        value = "v_diffuse";
        break;
    case ta::kCurrent:
        value = "current";
        break;
    case ta::kTexture:
        value = "t" + Number(stage);
        break;
    case ta::kTFactor:
        value = "u_tfactor";
        break;
    case ta::kSpecular:
        value = "v_specular";
        break;
    case ta::kTemp:
        value = "temp";
        break;
    default:
        value = "vec4(0.0)";
        break;
    }
    if ((arg & ta::kAlphaReplicate) != 0)
    {
        value = value + ".aaaa";
    }
    if ((arg & ta::kComplement) != 0)
    {
        value = "(vec4(1.0) - " + value + ")";
    }
    return value;
}

// The expression of one operation over channel selection `swizzle` (".rgb"
// or ".a").
std::string Operation(uint32_t op, const uint32_t args[3], uint32_t stage, const char* swizzle)
{
    const std::string s = swizzle;
    const std::string a0 = "(" + Argument(args[0], stage) + ")" + s;
    const std::string a1 = "(" + Argument(args[1], stage) + ")" + s;
    const std::string a2 = "(" + Argument(args[2], stage) + ")" + s;
    const std::string full1 = "(" + Argument(args[1], stage) + ")";
    const std::string full2 = "(" + Argument(args[2], stage) + ")";
    const std::string tex = "t" + Number(stage);
    switch (op)
    {
    case top::kSelectArg2:
        return a2;
    case top::kModulate:
        return a1 + " * " + a2;
    case top::kModulate2x:
        return a1 + " * " + a2 + " * 2.0";
    case top::kModulate4x:
        return a1 + " * " + a2 + " * 4.0";
    case top::kAdd:
        return a1 + " + " + a2;
    case top::kAddSigned:
        return a1 + " + " + a2 + " - 0.5";
    case top::kAddSigned2x:
        return "(" + a1 + " + " + a2 + " - 0.5) * 2.0";
    case top::kSubtract:
        return a1 + " - " + a2;
    case top::kAddSmooth:
        return a1 + " + " + a2 + " - " + a1 + " * " + a2;
    case top::kBlendDiffuseAlpha:
        return "mix(" + a2 + ", " + a1 + ", v_diffuse.a)";
    case top::kBlendCurrentAlpha:
        return "mix(" + a2 + ", " + a1 + ", current.a)";
    case top::kBlendTextureAlpha:
        return "mix(" + a2 + ", " + a1 + ", " + tex + ".a)";
    case top::kBlendFactorAlpha:
        return "mix(" + a2 + ", " + a1 + ", u_tfactor.a)";
    case top::kBlendTextureAlphaPm:
        return a1 + " + " + a2 + " * (1.0 - " + tex + ".a)";
    case top::kModulateAlphaAddColor:
        return s == ".a" ? a1 : full1 + ".rgb + " + full1 + ".a * " + full2 + ".rgb";
    case top::kModulateColorAddAlpha:
        return s == ".a" ? a1 : full1 + ".rgb * " + full2 + ".rgb + " + full1 + ".a";
    case top::kModulateInvAlphaAddColor:
        return s == ".a" ? a1 : "(1.0 - " + full1 + ".a) * " + full2 + ".rgb + " + full1 + ".rgb";
    case top::kModulateInvColorAddAlpha:
        return s == ".a" ? a1 : "(vec3(1.0) - " + full1 + ".rgb) * " + full2 + ".rgb + " + full1 + ".a";
    case top::kDotProduct3:
    {
        const std::string dot = "clamp(dot(" + full1 + ".rgb * 2.0 - 1.0, " + full2 + ".rgb * 2.0 - 1.0), 0.0, 1.0)";
        return s == ".a" ? dot : "vec3(" + dot + ")";
    }
    case top::kMultiplyAdd:
        return a0 + " + " + a1 + " * " + a2;
    case top::kLerp:
        return "mix(" + a2 + ", " + a1 + ", " + a0 + ")";
    default:
        // kSelectArg1, and the operations this batch does not implement
        // (premodulate, bump mapping), which the draw path reports.
        return a1;
    }
}

}  // namespace

std::string FixedFunctionKey::Serialize() const
{
    std::string text;
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "v%d%d%d%u:%u%u%u%u;s%d;a%X", pretransformed ? 1 : 0, has_diffuse ? 1 : 0,
                  has_specular ? 1 : 0, tex_coords, tex_components[0], tex_components[1], tex_components[2],
                  tex_components[3], specular_enable ? 1 : 0, alpha_func);
    text = buffer;
    for (const Stage& stage : stages)
    {
        std::snprintf(buffer, sizeof(buffer), "|%X,%X,%X,%X/%X,%X,%X,%X/%X/%u/%u%c", stage.color_op,
                      stage.color_arg[0], stage.color_arg[1], stage.color_arg[2], stage.alpha_op, stage.alpha_arg[0],
                      stage.alpha_arg[1], stage.alpha_arg[2], stage.result_arg, stage.tex_coord_index,
                      static_cast<unsigned>(stage.sampler), stage.unnormalized ? 'u' : 'n');
        text += buffer;
        if (stage.color_op == top::kDisable)
        {
            break;  // later stages do not affect the program
        }
    }
    return text;
}

uint64_t FixedFunctionKey::Hash() const
{
    uint64_t hash = 1469598103934665603ull;
    hash = HashMix(hash, (pretransformed ? 1u : 0u) | (has_diffuse ? 2u : 0u) | (has_specular ? 4u : 0u) |
                             (specular_enable ? 8u : 0u) | (static_cast<uint64_t>(tex_coords) << 8));
    for (const uint8_t components : tex_components)
    {
        hash = HashMix(hash, components);
    }
    hash = HashMix(hash, alpha_func);
    for (const Stage& stage : stages)
    {
        hash = HashMix(hash, stage.color_op);
        hash = HashMix(hash, (static_cast<uint64_t>(stage.color_arg[0]) << 32) | stage.color_arg[1]);
        hash = HashMix(hash, (static_cast<uint64_t>(stage.color_arg[2]) << 32) | stage.alpha_op);
        hash = HashMix(hash, (static_cast<uint64_t>(stage.alpha_arg[0]) << 32) | stage.alpha_arg[1]);
        hash = HashMix(hash, (static_cast<uint64_t>(stage.alpha_arg[2]) << 32) | stage.result_arg);
        hash = HashMix(hash, stage.tex_coord_index | (static_cast<uint64_t>(stage.sampler) << 8) |
                                 (stage.unnormalized ? 1ull << 16 : 0));
    }
    return hash;
}

std::string GenerateFixedFunctionVertexShader(const FixedFunctionKey& key, bool gles)
{
    std::string text = Header(gles);
    text += "layout(location = " + Number(attr::kPosition) + ") in vec4 a_position;\n";
    if (key.has_diffuse)
    {
        text += "layout(location = " + Number(attr::kDiffuse) + ") in vec4 a_diffuse;\n";
    }
    if (key.has_specular)
    {
        text += "layout(location = " + Number(attr::kSpecular) + ") in vec4 a_specular;\n";
    }
    for (uint32_t i = 0; i < key.tex_coords; ++i)
    {
        text += "layout(location = " + Number(attr::kTexCoord0 + i) + ") in vec4 a_tex" + Number(i) + ";\n";
    }
    text += "uniform mat4 u_transform;\n"
            "uniform vec4 u_target;\n"
            "out vec4 v_diffuse;\n"
            "out vec4 v_specular;\n";
    for (uint32_t i = 0; i < key.tex_coords; ++i)
    {
        text += "out vec4 v_tex" + Number(i) + ";\n";
    }
    text += "void main()\n{\n";
    if (key.pretransformed)
    {
        // Screen pixels (top-left origin) and 1/w; keep w for perspective-correct interpolation.
        text += "    float w = a_position.w != 0.0 ? 1.0 / a_position.w : 1.0;\n"
                "    vec2 p = a_position.xy + u_target.zw;\n"
                "    vec2 ndc = vec2(p.x / u_target.x * 2.0 - 1.0, 1.0 - p.y / u_target.y * 2.0);\n"
                "    gl_Position = vec4(ndc * w, (a_position.z * 2.0 - 1.0) * w, w);\n";
    }
    else
    {
        // D3D clip space has 0 <= z <= w; GL's -w <= z <= w.
        text += "    vec4 p = u_transform * vec4(a_position.xyz, 1.0);\n"
                "    gl_Position = vec4(p.x, p.y, p.z * 2.0 - p.w, p.w);\n";
    }
    // D3DCOLOR is stored B, G, R, A.
    text += key.has_diffuse ? "    v_diffuse = a_diffuse.bgra;\n" : "    v_diffuse = vec4(1.0);\n";
    text += key.has_specular ? "    v_specular = a_specular.bgra;\n" : "    v_specular = vec4(0.0);\n";
    for (uint32_t i = 0; i < key.tex_coords; ++i)
    {
        text += "    v_tex" + Number(i) + " = a_tex" + Number(i) + ";\n";
    }
    text += "}\n";
    return text;
}

std::string GenerateFixedFunctionFragmentShader(const FixedFunctionKey& key, bool gles)
{
    std::string text = Header(gles);
    text += "in vec4 v_diffuse;\n"
            "in vec4 v_specular;\n";
    for (uint32_t i = 0; i < key.tex_coords; ++i)
    {
        text += "in vec4 v_tex" + Number(i) + ";\n";
    }
    uint32_t active = 0;
    while (active < kFixedFunctionStages && key.stages[active].color_op != top::kDisable)
    {
        ++active;
    }
    for (uint32_t s = 0; s < active; ++s)
    {
        if (key.stages[s].sampler != SamplerKind::kNone)
        {
            text += std::string("uniform ") + SamplerType(key.stages[s].sampler) + " u_tex" + Number(s) + ";\n";
            if (key.stages[s].unnormalized)
            {
                text += "uniform vec2 u_tex_scale" + Number(s) + ";\n";
            }
        }
    }
    text += "uniform vec4 u_tfactor;\n"
            "uniform float u_alpha_ref;\n"
            "out vec4 o_color;\n"
            "void main()\n{\n"
            "    vec4 current = v_diffuse;\n"
            "    vec4 temp = vec4(0.0);\n";
    for (uint32_t s = 0; s < active; ++s)
    {
        const FixedFunctionKey::Stage& stage = key.stages[s];
        const std::string t = "t" + Number(s);
        if (stage.sampler == SamplerKind::kNone)
        {
            text += "    vec4 " + t + " = vec4(1.0);\n";
        }
        else
        {
            const std::string coords = stage.tex_coord_index < key.tex_coords
                                           ? "v_tex" + Number(stage.tex_coord_index)
                                           : std::string("vec4(0.0, 0.0, 0.0, 1.0)");
            std::string lookup = coords + (stage.sampler == SamplerKind::k2D ? ".xy" : ".xyz");
            if (stage.unnormalized && stage.sampler == SamplerKind::k2D)
            {
                lookup = "(" + lookup + " * u_tex_scale" + Number(s) + ")";
            }
            text += "    vec4 " + t + " = texture(u_tex" + Number(s) + ", " + lookup + ");\n";
        }
        const std::string color = Operation(stage.color_op, stage.color_arg, s, ".rgb");
        const std::string alpha = stage.alpha_op == top::kDisable
                                      ? std::string("current.a")
                                      : Operation(stage.alpha_op, stage.alpha_arg, s, ".a");
        const std::string result = (stage.result_arg & ta::kSelectMask) == ta::kTemp ? "temp" : "current";
        text += "    " + result + " = clamp(vec4(" + color + ", " + alpha + "), 0.0, 1.0);\n";
    }
    if (key.specular_enable)
    {
        text += "    current.rgb = clamp(current.rgb + v_specular.rgb, 0.0, 1.0);\n";
    }
    if (key.alpha_func != 0)
    {
        // D3DCMP / NV2A comparison values 0x200 NEVER .. 0x207 ALWAYS: pass when
        // the 8-bit alpha <op> the 8-bit reference.
        static const char* const kTests[8] = {"false",
                                              "alpha < u_alpha_ref",
                                              "alpha == u_alpha_ref",
                                              "alpha <= u_alpha_ref",
                                              "alpha > u_alpha_ref",
                                              "alpha != u_alpha_ref",
                                              "alpha >= u_alpha_ref",
                                              "true"};
        text += "    float alpha = floor(current.a * 255.0 + 0.5);\n";
        text += std::string("    if (!(") + kTests[key.alpha_func & 7] + ")) discard;\n";
    }
    text += "    o_color = current;\n}\n";
    return text;
}

}  // namespace rexbox::hle::d3d8
