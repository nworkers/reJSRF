#include "d3d8_gamma.h"

namespace rexbox::hle::d3d8
{

namespace
{

std::string Header(bool gles)
{
    return gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
}

}  // namespace

GammaRamp::GammaRamp()
{
    for (size_t channel = 0; channel < 3; ++channel)
    {
        for (size_t i = 0; i < kEntries; ++i)
        {
            bytes[channel * kEntries + i] = static_cast<uint8_t>(i);
        }
    }
}

bool GammaRamp::IsIdentity() const
{
    for (size_t i = 0; i < kBytes; ++i)
    {
        if (bytes[i] != static_cast<uint8_t>(i % kEntries))
        {
            return false;
        }
    }
    return true;
}

void ApplyGammaRamp(const GammaRamp& ramp, uint8_t* rgba, size_t pixels)
{
    for (size_t p = 0; p < pixels; ++p)
    {
        uint8_t* texel = rgba + p * 4;
        for (size_t channel = 0; channel < 3; ++channel)
        {
            texel[channel] = ramp.bytes[channel * GammaRamp::kEntries + texel[channel]];
        }
    }
}

std::array<uint8_t, GammaRamp::kEntries * 4> GammaRampTexels(const GammaRamp& ramp)
{
    std::array<uint8_t, GammaRamp::kEntries * 4> texels = {};
    for (size_t i = 0; i < GammaRamp::kEntries; ++i)
    {
        texels[i * 4 + 0] = ramp.bytes[i];
        texels[i * 4 + 1] = ramp.bytes[GammaRamp::kEntries + i];
        texels[i * 4 + 2] = ramp.bytes[2 * GammaRamp::kEntries + i];
        texels[i * 4 + 3] = 0xFF;
    }
    return texels;
}

std::string GammaPresentVertexShader(bool gles)
{
    // Vertices 0, 1, 2 cover the viewport with one triangle.
    return Header(gles) +
           "out vec2 v_uv;\n"
           "void main()\n{\n"
           "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
           "    v_uv = p;\n"
           "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
           "}\n";
}

std::string GammaPresentFragmentShader(bool gles)
{
    return Header(gles) +
           "in vec2 v_uv;\n"
           "uniform sampler2D u_image;\n"
           "uniform sampler2D u_ramp;\n"
           "out vec4 o_color;\n"
           "void main()\n{\n"
           "    vec4 c = texture(u_image, v_uv);\n"
           "    ivec3 i = ivec3(clamp(floor(c.rgb * 255.0 + 0.5), 0.0, 255.0));\n"
           "    o_color = vec4(texelFetch(u_ramp, ivec2(i.r, 0), 0).r, texelFetch(u_ramp, ivec2(i.g, 0), 0).g,\n"
           "                   texelFetch(u_ramp, ivec2(i.b, 0), 0).b, c.a);\n"
           "}\n";
}

}  // namespace rexbox::hle::d3d8
