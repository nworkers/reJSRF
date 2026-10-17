#include "d3d8_smaa.h"

#include "d3d8_internal.h"
#include "d3d8_trace.h"

// third_party/smaa/Textures: the precomputed area and search lookup tables.
#include "AreaTex.h"
#include "SearchTex.h"
// SMAA.hlsl as a string, made by cmake/embed_text.cmake.
#include "smaa_source.h"

#include <string>

namespace rexbox::hle::d3d8
{

namespace
{

constexpr GLenum kGlRed = 0x1903;
constexpr GLenum kGlRg = 0x8227;
constexpr GLenum kGlR8 = 0x8229;
constexpr GLenum kGlRg8 = 0x822B;

// The library's comments hold a few Latin-1 bytes (authors' names); GLSL
// takes ASCII only, so they become spaces.
std::string SmaaLibrary()
{
    std::string text(reinterpret_cast<const char*>(kSmaaSource), kSmaaSourceSize);
    for (char& c : text)
    {
        if (static_cast<unsigned char>(c) >= 0x80)
        {
            c = ' ';
        }
    }
    return text;
}

// Common to every stage: the target, preset and metrics, then the library
// with only the vertex or the pixel half.
std::string StageSource(bool vertex, const char* body)
{
    static const std::string library = SmaaLibrary();
    std::string text = "#version 330 core\n"
                       "#define SMAA_GLSL_3\n"
                       "#define SMAA_PRESET_HIGH\n"
                       "uniform vec4 u_metrics;\n"
                       "#define SMAA_RT_METRICS u_metrics\n";
    text += vertex ? "#define SMAA_INCLUDE_VS 1\n#define SMAA_INCLUDE_PS 0\n"
                   : "#define SMAA_INCLUDE_VS 0\n#define SMAA_INCLUDE_PS 1\n";
    text += library;
    text += "\n";
    text += body;
    return text;
}

// A full-screen triangle; texcoord 0..1 over the target.
constexpr const char* kTriangle = "    v_texcoord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
                                  "    gl_Position = vec4(v_texcoord * 2.0 - 1.0, 0.0, 1.0);\n";

const std::string kEdgeVertex = std::string("out vec2 v_texcoord;\nout vec4 v_offset[3];\nvoid main()\n{\n") +
                                kTriangle + "    SMAAEdgeDetectionVS(v_texcoord, v_offset);\n}\n";
constexpr const char* kEdgeFragment =
    "in vec2 v_texcoord;\nin vec4 v_offset[3];\nuniform sampler2D u_color;\nout vec4 o_color;\n"
    "void main()\n{\n    o_color = vec4(SMAAColorEdgeDetectionPS(v_texcoord, v_offset, u_color), 0.0, 0.0);\n}\n";

const std::string kWeightVertex =
    std::string("out vec2 v_texcoord;\nout vec2 v_pixcoord;\nout vec4 v_offset[3];\nvoid main()\n{\n") + kTriangle +
    "    SMAABlendingWeightCalculationVS(v_texcoord, v_pixcoord, v_offset);\n}\n";
constexpr const char* kWeightFragment =
    "in vec2 v_texcoord;\nin vec2 v_pixcoord;\nin vec4 v_offset[3];\n"
    "uniform sampler2D u_edges;\nuniform sampler2D u_area;\nuniform sampler2D u_search;\nout vec4 o_color;\n"
    "void main()\n{\n    o_color = SMAABlendingWeightCalculationPS(v_texcoord, v_pixcoord, v_offset, u_edges, u_area,"
    " u_search, vec4(0.0));\n}\n";

const std::string kBlendVertex = std::string("out vec2 v_texcoord;\nout vec4 v_offset;\nvoid main()\n{\n") +
                                 kTriangle + "    SMAANeighborhoodBlendingVS(v_texcoord, v_offset);\n}\n";
constexpr const char* kBlendFragment =
    "in vec2 v_texcoord;\nin vec4 v_offset;\nuniform sampler2D u_color;\nuniform sampler2D u_blend;\n"
    "out vec4 o_color;\n"
    "void main()\n{\n    o_color = SMAANeighborhoodBlendingPS(v_texcoord, v_offset, u_color, u_blend);\n}\n";

struct Pass
{
    GLuint program = 0;
    GLint metrics = -1;
};

struct Smaa
{
    bool tried = false;
    bool ready = false;
    Pass edges;
    Pass weights;
    Pass blend;
    GLuint area = 0;
    GLuint search = 0;
    GLuint vertex_array = 0;
    // Intermediate and output textures, remade when the size changes.
    uint32_t width = 0;
    uint32_t height = 0;
    GLuint edges_texture = 0;
    GLuint weights_texture = 0;
    GLuint output_texture = 0;
    GLuint edges_framebuffer = 0;
    GLuint weights_framebuffer = 0;
    GLuint output_framebuffer = 0;
};

Smaa g_smaa;

GLuint Compile(GLenum type, const std::string& source, const char* name)
{
    GlFunctions& gl = Gl();
    const GLuint shader = gl.CreateShader(type);
    const GLchar* text = source.c_str();
    gl.ShaderSource(shader, 1, &text, nullptr);
    gl.CompileShader(shader);
    GLint ok = 0;
    gl.GetShaderiv(shader, kGlCompileStatus, &ok);
    if (ok == 0)
    {
        GLchar log[2048] = {};
        gl.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ReportOnce(Format("smaa %s", name), Format("SMAA %s shader does not compile: %s", name, log));
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Link(Pass* pass, const std::string& vertex, const char* fragment, const char* name)
{
    GlFunctions& gl = Gl();
    const GLuint vs = Compile(kGlVertexShader, StageSource(true, vertex.c_str()), name);
    const GLuint fs = Compile(kGlFragmentShader, StageSource(false, fragment), name);
    if (vs == 0 || fs == 0)
    {
        return false;
    }
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vs);
    gl.AttachShader(program, fs);
    gl.LinkProgram(program);
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    GLint linked = 0;
    gl.GetProgramiv(program, kGlLinkStatus, &linked);
    if (linked == 0)
    {
        GLchar log[2048] = {};
        gl.GetProgramInfoLog(program, sizeof(log), nullptr, log);
        ReportOnce(Format("smaa link %s", name), Format("SMAA %s program does not link: %s", name, log));
        gl.DeleteProgram(program);
        return false;
    }
    pass->program = program;
    pass->metrics = gl.GetUniformLocation(program, "u_metrics");
    return true;
}

GLuint LookupTexture(GLenum internal_format, GLenum format, GLsizei width, GLsizei height, const void* data)
{
    GlFunctions& gl = Gl();
    GLuint texture = 0;
    gl.GenTextures(1, &texture);
    gl.BindTexture(kGlTexture2D, texture);
    gl.PixelStorei(kGlUnpackAlignment, 1);
    gl.TexImage2D(kGlTexture2D, 0, static_cast<GLint>(internal_format), width, height, 0, format, kGlUnsignedByte,
                  data);
    gl.TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlLinear));
    gl.TexParameteri(kGlTexture2D, kGlTextureWrapS, static_cast<GLint>(kGlClampToEdge));
    gl.TexParameteri(kGlTexture2D, kGlTextureWrapT, static_cast<GLint>(kGlClampToEdge));
    gl.TexParameteri(kGlTexture2D, kGlTextureMaxLevel, 0);
    return texture;
}

bool Prepare()
{
    Smaa& s = g_smaa;
    if (s.tried)
    {
        return s.ready;
    }
    s.tried = true;
    GlFunctions& gl = Gl();
    if (!Link(&s.edges, kEdgeVertex, kEdgeFragment, "edge detection") ||
        !Link(&s.weights, kWeightVertex, kWeightFragment, "blending weight") ||
        !Link(&s.blend, kBlendVertex, kBlendFragment, "neighborhood blending"))
    {
        return false;
    }
    s.area = LookupTexture(kGlRg8, kGlRg, AREATEX_WIDTH, AREATEX_HEIGHT, areaTexBytes);
    s.search = LookupTexture(kGlR8, kGlRed, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, searchTexBytes);
    gl.GenVertexArrays(1, &s.vertex_array);
    // Sampler units, fixed per program.
    gl.UseProgram(s.edges.program);
    gl.Uniform1i(gl.GetUniformLocation(s.edges.program, "u_color"), 0);
    gl.UseProgram(s.weights.program);
    gl.Uniform1i(gl.GetUniformLocation(s.weights.program, "u_edges"), 0);
    gl.Uniform1i(gl.GetUniformLocation(s.weights.program, "u_area"), 1);
    gl.Uniform1i(gl.GetUniformLocation(s.weights.program, "u_search"), 2);
    gl.UseProgram(s.blend.program);
    gl.Uniform1i(gl.GetUniformLocation(s.blend.program, "u_color"), 0);
    gl.Uniform1i(gl.GetUniformLocation(s.blend.program, "u_blend"), 1);
    s.ready = true;
    std::fprintf(stderr, "[d3d8] SMAA ready (preset high)\n");
    return true;
}

void MakeTarget(GLenum internal_format, GLenum format, uint32_t width, uint32_t height, GLuint* texture,
                GLuint* framebuffer)
{
    GlFunctions& gl = Gl();
    if (*texture != 0)
    {
        gl.DeleteTextures(1, texture);
        gl.DeleteFramebuffers(1, framebuffer);
    }
    *texture = LookupTexture(internal_format, format, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                             nullptr);
    gl.GenFramebuffers(1, framebuffer);
    gl.BindFramebuffer(kGlFramebuffer, *framebuffer);
    gl.FramebufferTexture2D(kGlFramebuffer, kGlColorAttachment0, kGlTexture2D, *texture, 0);
}

void Bind(uint32_t unit, GLuint texture)
{
    GlFunctions& gl = Gl();
    gl.ActiveTexture(kGlTexture0 + unit);
    gl.BindSampler(unit, 0);  // the draws' sampler objects must not apply
    gl.BindTexture(kGlTexture2D, texture);
}

void Draw(const Pass& pass, GLuint framebuffer, bool clear)
{
    const Smaa& s = g_smaa;
    GlFunctions& gl = Gl();
    gl.BindFramebuffer(kGlFramebuffer, framebuffer);
    if (clear)
    {
        // The edge pass discards pixels without edges.
        gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        gl.Clear(kGlColorBufferBit);
    }
    gl.UseProgram(pass.program);
    gl.Uniform4f(pass.metrics, 1.0f / static_cast<float>(s.width), 1.0f / static_cast<float>(s.height),
                 static_cast<float>(s.width), static_cast<float>(s.height));
    gl.DrawArrays(kGlTriangles, 0, 3);
}

}  // namespace

SmaaOutput ApplySmaa(GLuint input, uint32_t width, uint32_t height)
{
    if (input == 0 || width == 0 || height == 0 || !Prepare())
    {
        return {};
    }
    Smaa& s = g_smaa;
    GlFunctions& gl = Gl();
    if (s.width != width || s.height != height)
    {
        MakeTarget(kGlRg8, kGlRg, width, height, &s.edges_texture, &s.edges_framebuffer);
        MakeTarget(kGlRgba8, kGlRgba, width, height, &s.weights_texture, &s.weights_framebuffer);
        MakeTarget(kGlRgba8, kGlRgba, width, height, &s.output_texture, &s.output_framebuffer);
        s.width = width;
        s.height = height;
    }
    gl.Disable(kGlDepthTest);
    gl.Disable(kGlStencilTest);
    gl.Disable(kGlBlend);
    gl.Disable(kGlCullFace);
    gl.Disable(kGlScissorTest);
    gl.Disable(kGlPolygonOffsetFill);
    if (gl.PolygonMode != nullptr)
    {
        gl.PolygonMode(kGlFrontAndBack, kGlFill);
    }
    gl.ColorMask(1, 1, 1, 1);
    gl.Viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    gl.BindVertexArray(s.vertex_array);

    Bind(0, input);
    Draw(s.edges, s.edges_framebuffer, true);

    Bind(0, s.edges_texture);
    Bind(1, s.area);
    Bind(2, s.search);
    Draw(s.weights, s.weights_framebuffer, true);

    Bind(0, input);
    Bind(1, s.weights_texture);
    Draw(s.blend, s.output_framebuffer, false);

    gl.ActiveTexture(kGlTexture0);
    return {s.output_texture, s.output_framebuffer};
}

}  // namespace rexbox::hle::d3d8
