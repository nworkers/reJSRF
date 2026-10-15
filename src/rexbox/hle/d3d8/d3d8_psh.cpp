#include "d3d8_psh.h"

#include <cstdio>

namespace rexbox::hle::d3d8::psh
{

namespace
{

// Register numbers (an input byte's low nibble, an output's destination).
namespace reg
{
constexpr uint32_t kZero = 0, kC0 = 1, kC1 = 2, kFog = 3, kV0 = 4, kV1 = 5, kT0 = 8, kR0 = 12, kR1 = 13,
                   kV1R0Sum = 14, kEfProduct = 15;
}

// Input mappings (an input byte's top three bits).
constexpr uint32_t kMapUnsignedIdentity = 0x00, kMapUnsignedInvert = 0x20, kMapExpandNormal = 0x40,
                   kMapExpandNegate = 0x60, kMapHalfBiasNormal = 0x80, kMapHalfBiasNegate = 0xA0,
                   kMapSignedIdentity = 0xC0, kMapSignedNegate = 0xE0;
constexpr uint32_t kChannelAlpha = 0x10;

// Output flags (an output word's bits 12 and up).
constexpr uint32_t kOutCdDot = 0x01, kOutAbDot = 0x02, kOutMux = 0x04, kOutMappingMask = 0x38,
                   kOutCdBlueToAlpha = 0x40, kOutAbBlueToAlpha = 0x80;
constexpr uint32_t kOutBias = 0x08, kOutShiftLeft1 = 0x10, kOutShiftLeft1Bias = 0x18, kOutShiftLeft2 = 0x20,
                   kOutShiftRight1 = 0x30;

// Combiner count flags.
constexpr uint32_t kCountMuxMsb = 0x100, kCountUniqueC0 = 0x1000, kCountUniqueC1 = 0x10000;

// Final combiner flags (the EFG word's low byte).
constexpr uint32_t kFinalClampSum = 0x80, kFinalComplementV1 = 0x40, kFinalComplementR0 = 0x20;

std::string Number(uint32_t value)
{
    return std::to_string(value);
}

std::string Hex(uint32_t value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", value);
    return buffer;
}

// The vec4 a register reads in general stage `stage`.
std::string RegisterVector(uint32_t r, uint32_t stage)
{
    switch (r)
    {
    case reg::kC0:
        return "u_c0[" + Number(stage) + "]";
    case reg::kC1:
        return "u_c1[" + Number(stage) + "]";
    case reg::kFog:
        return "fog";
    case reg::kV0:
        return "v0";
    case reg::kV1:
        return "v1";
    case reg::kT0:
    case reg::kT0 + 1:
    case reg::kT0 + 2:
    case reg::kT0 + 3:
        return "t" + Number(r - reg::kT0);
    case reg::kR0:
        return "r0";
    case reg::kR1:
        return "r1";
    default:
        return "vec4(0.0)";
    }
}

// An input mapping applied to `x` (a vec3 or float expression).
std::string Map(uint32_t mapping, const std::string& x, bool vector)
{
    const std::string zero = vector ? "vec3(0.0)" : "0.0";
    const std::string one = vector ? "vec3(1.0)" : "1.0";
    switch (mapping)
    {
    case kMapUnsignedIdentity:
        return "max(" + x + ", " + zero + ")";
    case kMapUnsignedInvert:
        return "(" + one + " - clamp(" + x + ", 0.0, 1.0))";
    case kMapExpandNormal:
        return "(2.0 * max(" + x + ", " + zero + ") - 1.0)";
    case kMapExpandNegate:
        return "(1.0 - 2.0 * max(" + x + ", " + zero + "))";
    case kMapHalfBiasNormal:
        return "(max(" + x + ", " + zero + ") - 0.5)";
    case kMapHalfBiasNegate:
        return "(0.5 - max(" + x + ", " + zero + "))";
    case kMapSignedIdentity:
        return "(" + x + ")";
    default:  // kMapSignedNegate
        return "(-" + x + ")";
    }
}

// One input byte of a general stage: the RGB portion reads .rgb (or .aaa with
// the alpha bit), the alpha portion .a (or .b without it).
std::string StageInput(uint32_t byte, uint32_t stage, bool rgb)
{
    const uint32_t r = byte & 0x0F;
    const bool alpha = (byte & kChannelAlpha) != 0;
    const std::string v = RegisterVector(r, stage);
    std::string x;
    if (rgb)
    {
        x = alpha ? "vec3(" + v + ".a)" : v + ".rgb";
    }
    else
    {
        x = alpha ? v + ".a" : v + ".b";
    }
    return Map(byte & 0xE0, x, rgb);
}

std::string OutputMapping(uint32_t flags, const std::string& x)
{
    switch (flags & kOutMappingMask)
    {
    case kOutBias:
        return "(" + x + " - 0.5)";
    case kOutShiftLeft1:
        return "(" + x + " * 2.0)";
    case kOutShiftLeft1Bias:
        return "((" + x + " - 0.5) * 2.0)";
    case kOutShiftLeft2:
        return "(" + x + " * 4.0)";
    case kOutShiftRight1:
        return "(" + x + " * 0.5)";
    default:
        return x;
    }
}

std::string Destination(uint32_t r)
{
    switch (r)
    {
    case reg::kV0:
        return "v0";
    case reg::kV1:
        return "v1";
    case reg::kT0:
    case reg::kT0 + 1:
    case reg::kT0 + 2:
    case reg::kT0 + 3:
        return "t" + Number(r - reg::kT0);
    case reg::kR0:
        return "r0";
    case reg::kR1:
        return "r1";
    default:
        return std::string();  // 0 discards; others are not writable
    }
}

// One portion of a general stage: computes ab, cd and sum (mapped and
// clamped) into <prefix>ab/cd/sum. Returns the code.
std::string Portion(uint32_t inputs, uint32_t outputs, uint32_t stage, bool rgb, bool mux_msb, const std::string& prefix)
{
    const uint32_t flags = outputs >> 12;
    const std::string type = rgb ? "vec3" : "float";
    const std::string a = StageInput(inputs >> 24, stage, rgb);
    const std::string b = StageInput((inputs >> 16) & 0xFF, stage, rgb);
    const std::string c = StageInput((inputs >> 8) & 0xFF, stage, rgb);
    const std::string d = StageInput(inputs & 0xFF, stage, rgb);
    std::string text;
    const std::string ab = rgb && (flags & kOutAbDot) != 0 ? "vec3(dot(" + a + ", " + b + "))" : a + " * " + b;
    const std::string cd = rgb && (flags & kOutCdDot) != 0 ? "vec3(dot(" + c + ", " + d + "))" : c + " * " + d;
    text += "        " + type + " " + prefix + "ab = " + ab + ";\n";
    text += "        " + type + " " + prefix + "cd = " + cd + ";\n";
    std::string sum;
    if ((flags & kOutMux) != 0)
    {
        // The mux selects CD when r0.a's top bit (or lowest bit) is set.
        const std::string test = mux_msb ? "r0.a >= 0.5" : "mod(floor(clamp(r0.a, 0.0, 1.0) * 255.0 + 0.5), 2.0) >= 1.0";
        sum = "(" + test + " ? " + prefix + "cd : " + prefix + "ab)";
    }
    else
    {
        sum = prefix + "ab + " + prefix + "cd";
    }
    text += "        " + type + " " + prefix + "sum = " + sum + ";\n";
    for (const char* part : {"ab", "cd", "sum"})
    {
        const std::string name = prefix + part;
        text += "        " + name + " = clamp(" + OutputMapping(flags, name) + ", -1.0, 1.0);\n";
    }
    return text;
}

std::string Writes(uint32_t outputs, bool rgb, const std::string& prefix)
{
    std::string text;
    const uint32_t flags = outputs >> 12;
    const char* component = rgb ? ".rgb" : ".a";
    const struct
    {
        uint32_t dest;
        const char* part;
    } writes[3] = {{(outputs >> 4) & 0xF, "ab"}, {outputs & 0xF, "cd"}, {(outputs >> 8) & 0xF, "sum"}};
    for (const auto& write : writes)
    {
        const std::string dest = Destination(write.dest);
        if (!dest.empty())
        {
            text += "        " + dest + component + " = " + prefix + write.part + ";\n";
        }
    }
    if (rgb)
    {
        // Blue to alpha: the AB (CD) destination's alpha gets the result's blue.
        const std::string ab_dest = Destination((outputs >> 4) & 0xF);
        const std::string cd_dest = Destination(outputs & 0xF);
        if ((flags & kOutAbBlueToAlpha) != 0 && !ab_dest.empty())
        {
            text += "        " + ab_dest + ".a = " + prefix + "ab.b;\n";
        }
        if ((flags & kOutCdBlueToAlpha) != 0 && !cd_dest.empty())
        {
            text += "        " + cd_dest + ".a = " + prefix + "cd.b;\n";
        }
    }
    return text;
}

// One final-combiner input: identity (clamped) or invert, RGB or alpha.
std::string FinalInput(uint32_t byte, bool rgb)
{
    const uint32_t r = byte & 0x0F;
    const bool alpha = (byte & kChannelAlpha) != 0;
    std::string v;
    switch (r)
    {
    case reg::kV1R0Sum:
        v = "vec4(sum_v1r0, 0.0)";
        break;
    case reg::kEfProduct:
        v = "vec4(ef, 0.0)";
        break;
    case reg::kC0:
        v = "u_final_c0";
        break;
    case reg::kC1:
        v = "u_final_c1";
        break;
    default:
        v = RegisterVector(r, 0);
        break;
    }
    std::string x;
    if (rgb)
    {
        x = alpha ? "vec3(" + v + ".a)" : v + ".rgb";
    }
    else
    {
        x = alpha ? v + ".a" : v + ".b";
    }
    const bool invert = (byte & 0xE0) == kMapUnsignedInvert;
    const std::string clamped = "clamp(" + x + ", 0.0, 1.0)";
    return invert ? "(" + std::string(rgb ? "vec3(1.0)" : "1.0") + " - " + clamped + ")" : clamped;
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

}  // namespace

std::string Key::Serialize() const
{
    std::string text = "psh";
    for (const uint32_t w : definition)
    {
        text += Hex(w);
    }
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "|%u%u%u%u|%d%d%d%d|%d%d%d%d|%X", static_cast<unsigned>(samplers[0]),
                  static_cast<unsigned>(samplers[1]), static_cast<unsigned>(samplers[2]),
                  static_cast<unsigned>(samplers[3]), unnormalized[0] ? 1 : 0, unnormalized[1] ? 1 : 0,
                  unnormalized[2] ? 1 : 0, unnormalized[3] ? 1 : 0, bump_source_argb[0] ? 1 : 0,
                  bump_source_argb[1] ? 1 : 0, bump_source_argb[2] ? 1 : 0, bump_source_argb[3] ? 1 : 0,
                  alpha_func);
    return text + buffer;
}

uint64_t Key::Hash() const
{
    uint64_t hash = 1469598103934665603ull;
    for (const uint32_t w : definition)
    {
        hash = HashMix(hash, w);
    }
    for (uint32_t s = 0; s < 4; ++s)
    {
        hash = HashMix(hash, static_cast<uint64_t>(samplers[s]) | (unnormalized[s] ? 0x100u : 0u) |
                                 (bump_source_argb[s] ? 0x200u : 0u));
    }
    return HashMix(hash, alpha_func);
}

uint32_t TextureMode(const Definition& definition, uint32_t stage)
{
    return (definition[word::kTextureModes] >> (5 * stage)) & 0x1F;
}

std::string GenerateFragmentShader(const Key& key, bool gles, std::string* unsupported)
{
    unsupported->clear();
    const Definition& def = key.definition;
    std::string text = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
    text += "in vec4 v_diffuse;\n"
            "in vec4 v_specular;\n"
            "in vec4 v_tex0;\n"
            "in vec4 v_tex1;\n"
            "in vec4 v_tex2;\n"
            "in vec4 v_tex3;\n";
    for (uint32_t s = 0; s < 4; ++s)
    {
        if (TextureMode(def, s) != texmode::kNone && key.samplers[s] != SamplerKind::kNone)
        {
            text += std::string("uniform ") + SamplerType(key.samplers[s]) + " u_tex" + Number(s) + ";\n";
            if (key.unnormalized[s])
            {
                text += "uniform vec2 u_tex_scale" + Number(s) + ";\n";
            }
        }
    }
    text += "uniform vec4 u_c0[8];\n"
            "uniform vec4 u_c1[8];\n"
            "uniform vec4 u_final_c0;\n"
            "uniform vec4 u_final_c1;\n"
            "uniform vec4 u_fog_color;\n"
            "uniform float u_alpha_ref;\n"
            // BUMPENVMAP, per stage: the matrix (00, 01, 10, 11) and the
            // luminance scale and offset (x, y).
            "uniform vec4 u_bump_mat[4];\n"
            "uniform vec4 u_bump_lum[4];\n"
            "out vec4 o_color;\n"
            // A signed byte read through an unsigned format, as -1..1.
            "vec2 psh_signed(vec2 v)\n{\n"
            "    vec2 b = floor(v * 255.0 + 0.5);\n"
            "    b -= 256.0 * step(128.0, b);\n"
            "    return max(b / 127.0, vec2(-1.0));\n"
            "}\n"
            "void main()\n{\n"
            "    vec4 v0 = v_diffuse;\n"
            "    vec4 v1 = v_specular;\n"
            // Fog is not computed yet: a factor of 1 keeps the object's colour.
            "    vec4 fog = vec4(u_fog_color.rgb, 1.0);\n";

    // Texture stages.
    for (uint32_t s = 0; s < 4; ++s)
    {
        const std::string t = "t" + Number(s);
        const std::string coord = "v_tex" + Number(s);
        const uint32_t mode = TextureMode(def, s);
        const SamplerKind sampler = key.samplers[s];
        std::string value;
        if (mode == texmode::kNone)
        {
            value = "vec4(0.0)";
        }
        else if (mode == texmode::kPassThru)
        {
            value = "clamp(" + coord + ", 0.0, 1.0)";
        }
        else if (sampler == SamplerKind::kNone)
        {
            value = "vec4(1.0)";  // unbound: opaque white, as the fixed-function path
        }
        else if ((mode == texmode::kBumpEnvMap || mode == texmode::kBumpEnvMapLum) && s > 0 &&
                 sampler == SamplerKind::k2D)
        {
            // The previous stage's texel is a signed (du, dv) offset, turned
            // by the stage's bump matrix and added to this stage's projected
            // coordinates (without the division the boost effect showed the
            // screen copy at twice its size, issue #69); the luminance form
            // also scales the result by L * scale + offset.
            const std::string n = Number(s);
            const std::string source = "t" + Number(s - 1) + (key.bump_source_argb[s - 1] ? ".bgr" : ".rgb");
            const std::string projected = "(" + coord + ".w != 0.0 ? " + coord + " / " + coord + ".w : " + coord + ")";
            text += "    vec3 uvl" + n + " = " + source + ";\n"
                    "    vec2 dsdt" + n + " = psh_signed(uvl" + n + ".xy);\n"
                    "    vec2 bumped" + n + " = " + projected + ".xy + vec2(dot(u_bump_mat[" + n + "].xz, dsdt" + n +
                    "), dot(u_bump_mat[" + n + "].yw, dsdt" + n + "));\n";
            std::string uv = "bumped" + n;
            if (key.unnormalized[s])
            {
                uv = "(" + uv + " * u_tex_scale" + n + ")";
            }
            value = "texture(u_tex" + n + ", " + uv + ")";
            if (mode == texmode::kBumpEnvMapLum)
            {
                value = "(" + value + " * vec4(vec3(clamp(uvl" + n + ".z * u_bump_lum[" + n + "].x + u_bump_lum[" +
                        n + "].y, 0.0, 1.0)), 1.0))";
            }
        }
        else
        {
            if (mode != texmode::kProject2D && mode != texmode::kProject3D && mode != texmode::kCubeMap)
            {
                *unsupported += "texture mode " + Number(mode) + " (stage " + Number(s) + ") sampled as 2D; ";
            }
            const std::string sampler_name = "u_tex" + Number(s);
            const std::string projected = "(" + coord + ".w != 0.0 ? " + coord + " / " + coord + ".w : " + coord + ")";
            if (sampler == SamplerKind::kCube)
            {
                value = "texture(" + sampler_name + ", " + coord + ".xyz)";
            }
            else if (sampler == SamplerKind::k3D)
            {
                value = "texture(" + sampler_name + ", " + projected + ".xyz)";
            }
            else
            {
                std::string uv = projected + ".xy";
                if (key.unnormalized[s])
                {
                    uv = "(" + uv + " * u_tex_scale" + Number(s) + ")";
                }
                value = "texture(" + sampler_name + ", " + uv + ")";
            }
        }
        text += "    vec4 " + t + " = " + value + ";\n";
    }
    if (def[word::kCompareMode] != 0)
    {
        *unsupported += "compare mode " + Hex(def[word::kCompareMode]) + " ignored; ";
    }

    // General combiners. R0's alpha starts as texture 0's.
    text += "    vec4 r0 = vec4(0.0, 0.0, 0.0, t0.a);\n"
            "    vec4 r1 = vec4(0.0);\n";
    const uint32_t count_word = def[word::kCombinerCount];
    const uint32_t stages = count_word & 0xFF;
    const bool mux_msb = (count_word & kCountMuxMsb) != 0;
    for (uint32_t s = 0; s < stages && s < kStages; ++s)
    {
        const uint32_t rgb_in = def[word::kRgbInputs + s];
        const uint32_t rgb_out = def[word::kRgbOutputs + s];
        const uint32_t alpha_in = def[word::kAlphaInputs + s];
        const uint32_t alpha_out = def[word::kAlphaOutputs + s];
        text += "    {\n";
        // Both portions read their inputs before either writes.
        text += Portion(rgb_in, rgb_out, s, true, mux_msb, "c");
        text += Portion(alpha_in, alpha_out, s, false, mux_msb, "a");
        // The RGB portion's blue-to-alpha overrides the alpha portion's write.
        text += Writes(alpha_out, false, "a");
        text += Writes(rgb_out, true, "c");
        text += "    }\n";
    }

    // Final combiner, when the definition has one; else r0.
    const uint32_t abcd = def[word::kFinalInputsAbcd];
    const uint32_t efg = def[word::kFinalInputsEfg];
    if (abcd != 0 || efg != 0)
    {
        const uint32_t flags = efg & 0xFF;
        text += "    vec3 sum_v1r0 = " +
                std::string((flags & kFinalComplementV1) != 0 ? "(vec3(1.0) - clamp(v1.rgb, 0.0, 1.0))"
                                                                : "clamp(v1.rgb, 0.0, 1.0)") +
                " + " +
                std::string((flags & kFinalComplementR0) != 0 ? "(vec3(1.0) - clamp(r0.rgb, 0.0, 1.0))"
                                                                : "clamp(r0.rgb, 0.0, 1.0)") +
                ";\n";
        if ((flags & kFinalClampSum) != 0)
        {
            text += "    sum_v1r0 = clamp(sum_v1r0, 0.0, 1.0);\n";
        }
        text += "    vec3 ef = vec3(0.0);\n";
        text += "    ef = " + FinalInput(efg >> 24, true) + " * " + FinalInput((efg >> 16) & 0xFF, true) + ";\n";
        const std::string a = FinalInput(abcd >> 24, true);
        const std::string b = FinalInput((abcd >> 16) & 0xFF, true);
        const std::string c = FinalInput((abcd >> 8) & 0xFF, true);
        const std::string d = FinalInput(abcd & 0xFF, true);
        text += "    vec3 fa = " + a + ";\n";
        text += "    vec4 current = vec4(clamp(fa * " + b + " + (vec3(1.0) - fa) * " + c + " + " + d +
                ", 0.0, 1.0), " + FinalInput((efg >> 8) & 0xFF, false) + ");\n";
    }
    else
    {
        text += "    vec4 current = clamp(r0, 0.0, 1.0);\n";
    }

    if (key.alpha_func != 0)
    {
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

void StageConstants(const Definition& definition, const float pixel_constants[kPixelConstants][4], float c0[kStages][4],
                    float c1[kStages][4], float final_c0[4], float final_c1[4])
{
    const auto unpack = [](uint32_t argb, float out[4]) {
        out[0] = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
        out[1] = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
        out[2] = static_cast<float>(argb & 0xFF) / 255.0f;
        out[3] = static_cast<float>(argb >> 24) / 255.0f;
    };
    const auto pick = [&](uint32_t nibble, uint32_t fallback, float out[4]) {
        if (nibble < kPixelConstants && nibble != 0xF)
        {
            for (int i = 0; i < 4; ++i)
            {
                out[i] = pixel_constants[nibble][i];
            }
        }
        else
        {
            unpack(fallback, out);
        }
    };
    const uint32_t count_word = definition[word::kCombinerCount];
    for (uint32_t s = 0; s < kStages; ++s)
    {
        const uint32_t own0 = (count_word & kCountUniqueC0) != 0 ? s : 0;
        const uint32_t own1 = (count_word & kCountUniqueC1) != 0 ? s : 0;
        pick((definition[word::kC0Mapping] >> (4 * s)) & 0xF, definition[word::kConstant0 + own0], c0[s]);
        pick((definition[word::kC1Mapping] >> (4 * s)) & 0xF, definition[word::kConstant1 + own1], c1[s]);
    }
    pick(definition[word::kFinalConstants] & 0xF, definition[word::kFinalConstant0], final_c0);
    pick((definition[word::kFinalConstants] >> 4) & 0xF, definition[word::kFinalConstant1], final_c1);
}

}  // namespace rexbox::hle::d3d8::psh
