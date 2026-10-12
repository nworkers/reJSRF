#include "d3d8_gl_programs.h"

#include "d3d8_gl.h"
#include "d3d8_trace.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

std::unordered_map<std::string, FixedFunctionProgram> g_programs;
GLuint g_vertex_array = 0;
GLuint g_vertex_buffer = 0;
GLuint g_index_buffer = 0;

std::string ShaderLog(GLuint shader)
{
    GLint length = 0;
    Gl().GetShaderiv(shader, kGlInfoLogLength, &length);
    std::vector<GLchar> log(static_cast<size_t>(length > 1 ? length : 1), '\0');
    Gl().GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
    return log.data();
}

GLuint Compile(GLenum type, const std::string& source, const std::string& key)
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
        ReportOnce("compile " + key, Format("%s shader for key %s does not compile:\n%s\n--- source ---\n%s",
                                            type == kGlVertexShader ? "vertex" : "fragment", key.c_str(),
                                            ShaderLog(shader).c_str(), source.c_str()));
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

const FixedFunctionProgram* GetFixedFunctionProgram(const FixedFunctionKey& key)
{
    const std::string id = key.Serialize();
    const auto it = g_programs.find(id);
    if (it != g_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    GlFunctions& gl = Gl();
    FixedFunctionProgram& entry = g_programs[id];
    const bool es = GlIsEs();
    const GLuint vertex = Compile(kGlVertexShader, GenerateFixedFunctionVertexShader(key, es), id);
    const GLuint fragment = Compile(kGlFragmentShader, GenerateFixedFunctionFragmentShader(key, es), id);
    if (vertex == 0 || fragment == 0)
    {
        return nullptr;
    }
    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, vertex);
    gl.AttachShader(program, fragment);
    gl.LinkProgram(program);
    gl.DeleteShader(vertex);
    gl.DeleteShader(fragment);
    GLint linked = 0;
    gl.GetProgramiv(program, kGlLinkStatus, &linked);
    if (linked == 0)
    {
        GLint length = 0;
        gl.GetProgramiv(program, kGlInfoLogLength, &length);
        std::vector<GLchar> log(static_cast<size_t>(length > 1 ? length : 1), '\0');
        gl.GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        ReportOnce("link " + id, Format("program for key %s does not link: %s", id.c_str(), log.data()));
        return nullptr;
    }
    entry.program = program;
    entry.transform = gl.GetUniformLocation(program, "u_transform");
    entry.target = gl.GetUniformLocation(program, "u_target");
    entry.tfactor = gl.GetUniformLocation(program, "u_tfactor");
    entry.alpha_ref = gl.GetUniformLocation(program, "u_alpha_ref");
    gl.UseProgram(program);
    for (uint32_t stage = 0; stage < kFixedFunctionStages; ++stage)
    {
        const std::string suffix = std::to_string(stage);
        const GLint sampler = gl.GetUniformLocation(program, ("u_tex" + suffix).c_str());
        if (sampler >= 0)
        {
            gl.Uniform1i(sampler, static_cast<GLint>(stage));
        }
        entry.tex_scale[stage] = gl.GetUniformLocation(program, ("u_tex_scale" + suffix).c_str());
    }
    TraceOnce("program " + id, Format("program %u built for key %s", program, id.c_str()));
    return &entry;
}

void BindStreamingBuffers()
{
    GlFunctions& gl = Gl();
    if (g_vertex_array == 0)
    {
        gl.GenVertexArrays(1, &g_vertex_array);
        gl.GenBuffers(1, &g_vertex_buffer);
        gl.GenBuffers(1, &g_index_buffer);
    }
    gl.BindVertexArray(g_vertex_array);
    gl.BindBuffer(kGlArrayBuffer, g_vertex_buffer);
    gl.BindBuffer(kGlElementArrayBuffer, g_index_buffer);
}

void UploadVertexData(const void* data, size_t bytes)
{
    Gl().BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(bytes), data, kGlStreamDraw);
}

void UploadIndexData(const void* data, size_t bytes)
{
    Gl().BufferData(kGlElementArrayBuffer, static_cast<GLsizeiptr>(bytes), data, kGlStreamDraw);
}

}  // namespace rexbox::hle::d3d8
