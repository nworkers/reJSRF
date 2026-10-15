#include "d3d8_gl_programs.h"

#include "d3d8_gl.h"
#include "d3d8_gl_program_cache.h"
#include "d3d8_profile.h"
#include "d3d8_trace.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

struct FixedFunctionKeyHash
{
    size_t operator()(const FixedFunctionKey& key) const
    {
        return static_cast<size_t>(key.Hash());
    }
};

// A vertex-shader program paired with a fragment stage.
template <typename FragmentKey>
struct PairKey
{
    uint64_t shader = 0;
    FragmentKey fragment;
    bool operator==(const PairKey&) const = default;
};

template <typename FragmentKey>
struct PairKeyHash
{
    size_t operator()(const PairKey<FragmentKey>& key) const
    {
        return static_cast<size_t>(HashMix(key.fragment.Hash(), key.shader));
    }
};

std::unordered_map<FixedFunctionKey, FixedFunctionProgram, FixedFunctionKeyHash> g_programs;

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
    const auto started = std::chrono::steady_clock::now();
    if (const GLuint cached = LoadCachedProgram(vertex_source, fragment_source); cached != 0)
    {
        profile::CountProgram(true, std::chrono::steady_clock::now() - started);
        return cached;
    }
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
    MarkProgramRetrievable(program);
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
    profile::CountProgram(false, std::chrono::steady_clock::now() - started);
    const auto stored = std::chrono::steady_clock::now();
    if (StoreProgram(program, vertex_source, fragment_source))
    {
        profile::CountProgramStore(std::chrono::steady_clock::now() - stored);
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
    InvalidateGlStateCache();
}

// The location of each element of the vertex-shader constant array c[].
std::vector<int> ConstantLocations(GLuint program)
{
    std::vector<int> locations(vsh::kConstants);
    for (uint32_t i = 0; i < vsh::kConstants; ++i)
    {
        locations[i] = Gl().GetUniformLocation(program, ("c[" + std::to_string(i) + "]").c_str());
    }
    return locations;
}

GLuint g_current_program = 0;
uint32_t g_program_epoch = 0;

std::unordered_map<PairKey<FixedFunctionKey>, VertexProgramGl, PairKeyHash<FixedFunctionKey>> g_vertex_programs;
std::unordered_map<PairKey<psh::Key>, VertexProgramGl, PairKeyHash<psh::Key>> g_vertex_pixel_programs;

}  // namespace

bool UniformCache::Changed(int location, const float* values, size_t count)
{
    if (location < 0)
    {
        return false;
    }
    for (Entry& entry : entries_)
    {
        if (entry.location == location)
        {
            // Bitwise, so a NaN that stays a NaN is unchanged.
            if (entry.values.size() == count && std::memcmp(entry.values.data(), values, count * 4) == 0)
            {
                return false;
            }
            entry.values.assign(values, values + count);
            return true;
        }
    }
    entries_.push_back({location, std::vector<float>(values, values + count)});
    return true;
}

void UniformCache::Set1f(int location, float x)
{
    if (Changed(location, &x, 1))
    {
        Gl().Uniform1f(location, x);
    }
}

void UniformCache::Set2f(int location, float x, float y)
{
    const float values[2] = {x, y};
    if (Changed(location, values, 2))
    {
        Gl().Uniform2f(location, x, y);
    }
}

void UniformCache::Set4f(int location, float x, float y, float z, float w)
{
    const float values[4] = {x, y, z, w};
    if (Changed(location, values, 4))
    {
        Gl().Uniform4f(location, x, y, z, w);
    }
}

void UniformCache::Set4fv(int location, int count, const float* values)
{
    if (Changed(location, values, static_cast<size_t>(count) * 4))
    {
        Gl().Uniform4fv(location, count, values);
    }
}

void UniformCache::SetMatrix4(int location, const float* values)
{
    if (Changed(location, values, 16))
    {
        Gl().UniformMatrix4fv(location, 1, 0, values);
    }
}

void UniformCache::SetArray4(const std::vector<int>& locations, const float* values, uint64_t version)
{
    const size_t count = locations.size();
    const bool fresh = array_.size() != count * 4;
    if (!fresh && version == array_version_)
    {
        return;
    }
    array_version_ = version;
    if (fresh)
    {
        array_.assign(values, values + count * 4);
    }
    if (fresh)
    {
        // chain_[i]: the first element of the run of consecutive locations
        // that holds element i.
        chain_.assign(count, 0);
        for (size_t i = 1; i < count; ++i)
        {
            chain_[i] = locations[i] >= 0 && locations[i] == locations[i - 1] + 1 ? chain_[i - 1]
                                                                                   : static_cast<uint32_t>(i);
        }
    }
    // One call per run of consecutive locations, from its first changed
    // element to its last: the unchanged ones between go along, since a call
    // costs more than the bytes (issue #50 counted four calls per draw).
    GlFunctions& gl = Gl();
    size_t span_begin = count;
    size_t span_end = 0;
    const auto flush = [&] {
        if (span_begin < span_end)
        {
            std::memcpy(&array_[span_begin * 4], values + span_begin * 4, (span_end - span_begin) * 16);
            gl.Uniform4fv(locations[span_begin], static_cast<GLsizei>(span_end - span_begin), values + span_begin * 4);
        }
        span_begin = count;
    };
    for (size_t i = 0; i < count; ++i)
    {
        if (locations[i] < 0 || (!fresh && std::memcmp(&array_[i * 4], values + i * 4, 16) == 0))
        {
            continue;
        }
        if (span_begin < count && chain_[i] <= span_begin)
        {
            span_end = i + 1;
            continue;
        }
        flush();
        span_begin = i;
        span_end = i + 1;
    }
    flush();
}

const FixedFunctionProgram* GetFixedFunctionProgram(const FixedFunctionKey& key)
{
    const auto it = g_programs.find(key);
    if (it != g_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    const std::string id = key.Serialize();
    GlFunctions& gl = Gl();
    FixedFunctionProgram& entry = g_programs[key];
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
    entry.world_view = gl.GetUniformLocation(program, "u_world_view");
    entry.normal_matrix = gl.GetUniformLocation(program, "u_normal_matrix");
    for (uint32_t s = 0; s < kFixedFunctionStages; ++s)
    {
        entry.tex_matrix[s] = gl.GetUniformLocation(program, ("u_tex_matrix" + std::to_string(s)).c_str());
    }
    BindSamplers(program, entry.tex_scale);
    TraceOnce("program " + id, Format("program %u built for key %s", program, id.c_str()));
    return &entry;
}

const VertexProgramGl* GetVertexProgram(const ProgramShader& shader, const FixedFunctionKey& fragment_key)
{
    const PairKey<FixedFunctionKey> key{shader.hash, fragment_key};
    const auto it = g_vertex_programs.find(key);
    if (it != g_vertex_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    char hash[24];
    std::snprintf(hash, sizeof(hash), "vsh%016llX|", static_cast<unsigned long long>(shader.hash));
    const std::string id = hash + fragment_key.Serialize();
    GlFunctions& gl = Gl();
    VertexProgramGl& entry = g_vertex_programs[key];
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
    entry.constants = ConstantLocations(program);
    entry.tfactor = gl.GetUniformLocation(program, "u_tfactor");
    entry.alpha_ref = gl.GetUniformLocation(program, "u_alpha_ref");
    BindSamplers(program, entry.tex_scale);
    TraceOnce("program " + id, Format("program %u built for vertex program %s", program, id.c_str()));
    return &entry;
}

const VertexProgramGl* GetVertexPixelProgram(const ProgramShader& shader, const psh::Key& pixel_key)
{
    const PairKey<psh::Key> key{shader.hash, pixel_key};
    const auto it = g_vertex_pixel_programs.find(key);
    if (it != g_vertex_pixel_programs.end())
    {
        return it->second.program != 0 ? &it->second : nullptr;
    }
    char hash[24];
    std::snprintf(hash, sizeof(hash), "vsh%016llX|", static_cast<unsigned long long>(shader.hash));
    const std::string id = hash + pixel_key.Serialize();
    GlFunctions& gl = Gl();
    VertexProgramGl& entry = g_vertex_pixel_programs[key];
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
    entry.constants = ConstantLocations(program);
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

void UseProgram(uint32_t program)
{
    if (g_program_epoch == GlStateEpoch() && g_current_program == program)
    {
        return;
    }
    Gl().UseProgram(program);
    g_current_program = program;
    g_program_epoch = GlStateEpoch();
}

}  // namespace rexbox::hle::d3d8
