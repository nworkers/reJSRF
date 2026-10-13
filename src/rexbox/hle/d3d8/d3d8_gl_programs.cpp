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

// Compiles and links; 0 (reported once) on failure.
GLuint Link(const std::string& vertex_source, const std::string& fragment_source, const std::string& id)
{
    GlFunctions& gl = Gl();
    const GLuint vertex = Compile(kGlVertexShader, vertex_source, id);
    const GLuint fragment = Compile(kGlFragmentShader, fragment_source, id);
    if (vertex == 0 || fragment == 0)
    {
        if (vertex != 0)
        {
            gl.DeleteShader(vertex);
        }
        if (fragment != 0)
        {
            gl.DeleteShader(fragment);
        }
        return 0;
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
        return 0;
    }
    return program;
}

// Points the program's samplers at units 0..3 and returns the scale uniforms.
void BindSamplers(GLuint program, int tex_scale[kFixedFunctionStages])
{
    GlFunctions& gl = Gl();
    gl.UseProgram(program);
    for (uint32_t stage = 0; stage < kFixedFunctionStages; ++stage)
    {
        const std::string suffix = std::to_string(stage);
        const GLint sampler = gl.GetUniformLocation(program, ("u_tex" + suffix).c_str());
        if (sampler >= 0)
        {
            gl.Uniform1i(sampler, static_cast<GLint>(stage));
        }
        tex_scale[stage] = gl.GetUniformLocation(program, ("u_tex_scale" + suffix).c_str());
    }
}

std::unordered_map<std::string, VertexProgramGl> g_vertex_programs;

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
    const GLuint program =
        Link(GenerateFixedFunctionVertexShader(key, es), GenerateFixedFunctionFragmentShader(key, es), id);
    if (program == 0)
    {
        return nullptr;
    }
    entry.program = program;
    entry.transform = gl.GetUniformLocation(program, "u_transform");
    entry.target = gl.GetUniformLocation(program, "u_target");
    entry.tfactor = gl.GetUniformLocation(program, "u_tfactor");
    entry.alpha_ref = gl.GetUniformLocation(program, "u_alpha_ref");
    BindSamplers(program, entry.tex_scale);
    TraceOnce("program " + id, Format("program %u built for key %s", program, id.c_str()));
    return &entry;
}

const VertexProgramGl* GetVertexProgram(const ProgramShader& shader, const FixedFunctionKey& fragment_key)
{
    char hash[24];
    std::snprintf(hash, sizeof(hash), "vsh%016llX|", static_cast<unsigned long long>(shader.hash));
    const std::string id = hash + fragment_key.Serialize();
    const auto it = g_vertex_programs.find(id);
    if (it != g_vertex_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    GlFunctions& gl = Gl();
    VertexProgramGl& entry = g_vertex_programs[id];
    const bool es = GlIsEs();
    uint32_t used_inputs = 0;
    const std::string vertex = vsh::TranslateToGlsl(shader.program, shader.declaration.bgra_inputs, es, &used_inputs);
    const GLuint program = Link(vertex, GenerateFixedFunctionFragmentShader(fragment_key, es), id);
    if (program == 0)
    {
        return nullptr;
    }
    entry.program = program;
    entry.used_inputs = used_inputs;
    entry.constants = gl.GetUniformLocation(program, "c");
    entry.tfactor = gl.GetUniformLocation(program, "u_tfactor");
    entry.alpha_ref = gl.GetUniformLocation(program, "u_alpha_ref");
    BindSamplers(program, entry.tex_scale);
    TraceOnce("program " + id, Format("program %u built for vertex program %s", program, id.c_str()));
    return &entry;
}

const VertexProgramGl* GetVertexPixelProgram(const ProgramShader& shader, const psh::Key& pixel_key)
{
    char hash[24];
    std::snprintf(hash, sizeof(hash), "vsh%016llX|", static_cast<unsigned long long>(shader.hash));
    const std::string id = hash + pixel_key.Serialize();
    const auto it = g_vertex_programs.find(id);
    if (it != g_vertex_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    GlFunctions& gl = Gl();
    VertexProgramGl& entry = g_vertex_programs[id];
    const bool es = GlIsEs();
    uint32_t used_inputs = 0;
    const std::string vertex = vsh::TranslateToGlsl(shader.program, shader.declaration.bgra_inputs, es, &used_inputs);
    std::string unsupported;
    const std::string fragment = psh::GenerateFragmentShader(pixel_key, es, &unsupported);
    if (!unsupported.empty())
    {
        ReportOnce("psh " + id, Format("pixel shader drawn approximately: %s", unsupported.c_str()));
    }
    const GLuint program = Link(vertex, fragment, id);
    if (program == 0)
    {
        return nullptr;
    }
    entry.program = program;
    entry.used_inputs = used_inputs;
    entry.constants = gl.GetUniformLocation(program, "c");
    entry.alpha_ref = gl.GetUniformLocation(program, "u_alpha_ref");
    entry.stage_c0 = gl.GetUniformLocation(program, "u_c0");
    entry.stage_c1 = gl.GetUniformLocation(program, "u_c1");
    entry.final_c0 = gl.GetUniformLocation(program, "u_final_c0");
    entry.final_c1 = gl.GetUniformLocation(program, "u_final_c1");
    entry.fog_color = gl.GetUniformLocation(program, "u_fog_color");
    BindSamplers(program, entry.tex_scale);
    TraceOnce("program " + id, Format("program %u built for vertex program and pixel shader %s", program, id.c_str()));
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
