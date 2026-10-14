#include "d3d8_gl_program_cache.h"

#include "d3d8_gl.h"
#include "d3d8_trace.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// File layout: magic, layout version, binary format, then four
// length-prefixed blocks: driver identity, vertex source, fragment source,
// program binary. All integers little-endian uint32.
constexpr char kMagic[4] = {'R', 'X', 'P', 'B'};
constexpr uint32_t kLayoutVersion = 1;

struct CacheState
{
    bool checked = false;
    bool enabled = false;
    std::filesystem::path directory;
    std::string driver;  // vendor, renderer and version
};

CacheState& Cache()
{
    static CacheState state;
    if (state.checked)
    {
        return state;
    }
    state.checked = true;
    const char* setting = std::getenv("REXBOX_SHADER_CACHE");
    if (setting != nullptr && (std::strcmp(setting, "0") == 0 || std::strcmp(setting, "off") == 0))
    {
        return state;
    }
    GlFunctions& gl = Gl();
    if (gl.GetProgramBinary == nullptr || gl.ProgramBinary == nullptr || gl.ProgramParameteri == nullptr)
    {
        return state;
    }
    GLint formats = 0;
    gl.GetIntegerv(kGlNumProgramBinaryFormats, &formats);
    if (formats <= 0)
    {
        return state;
    }
    for (const GLenum name : {kGlVendor, kGlRenderer, kGlVersion})
    {
        const GLubyte* text = gl.GetString(name);
        state.driver += text != nullptr ? reinterpret_cast<const char*>(text) : "?";
        state.driver += '\n';
    }
    state.directory = setting != nullptr && setting[0] != '\0' ? std::filesystem::path(setting)
                                                               : std::filesystem::path("cache") / "shaders";
    std::error_code error;
    std::filesystem::create_directories(state.directory, error);
    state.enabled = !error;
    if (!state.enabled)
    {
        ReportOnce("shader cache", Format("shader cache directory %s cannot be created; programs are not cached",
                                          state.directory.string().c_str()));
    }
    return state;
}

uint64_t Hash(const std::string& a, const std::string& b, const std::string& c)
{
    uint64_t hash = 1469598103934665603ull;
    for (const std::string* text : {&a, &b, &c})
    {
        for (const unsigned char ch : *text)
        {
            hash = (hash ^ ch) * 1099511628211ull;
        }
        hash = (hash ^ 0xFF) * 1099511628211ull;
    }
    return hash;
}

std::filesystem::path EntryPath(const std::string& vertex_source, const std::string& fragment_source)
{
    CacheState& cache = Cache();
    char name[32];
    std::snprintf(name, sizeof(name), "%016llX.bin",
                  static_cast<unsigned long long>(Hash(cache.driver, vertex_source, fragment_source)));
    return cache.directory / name;
}

void PutU32(std::vector<char>* out, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        out->push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }
}

void PutBlock(std::vector<char>* out, const char* data, size_t size)
{
    PutU32(out, static_cast<uint32_t>(size));
    out->insert(out->end(), data, data + size);
}

// Reads the file's blocks; false when it is not a complete entry.
struct Reader
{
    const std::vector<char>& data;
    size_t at = 0;

    bool U32(uint32_t* value)
    {
        if (data.size() - at < 4)
        {
            return false;
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            v |= static_cast<uint32_t>(static_cast<unsigned char>(data[at + i])) << (8 * i);
        }
        at += 4;
        *value = v;
        return true;
    }
    bool Block(const char** begin, size_t* size)
    {
        uint32_t length = 0;
        if (!U32(&length) || data.size() - at < length)
        {
            return false;
        }
        *begin = data.data() + at;
        *size = length;
        at += length;
        return true;
    }
    bool Matches(const std::string& expected)
    {
        const char* begin = nullptr;
        size_t size = 0;
        return Block(&begin, &size) && size == expected.size() && std::memcmp(begin, expected.data(), size) == 0;
    }
};

}  // namespace

bool ProgramCacheEnabled()
{
    return Cache().enabled;
}

void MarkProgramRetrievable(uint32_t program)
{
    if (ProgramCacheEnabled())
    {
        Gl().ProgramParameteri(program, kGlProgramBinaryRetrievableHint, 1);
    }
}

uint32_t LoadCachedProgram(const std::string& vertex_source, const std::string& fragment_source)
{
    if (!ProgramCacheEnabled())
    {
        return 0;
    }
    std::ifstream file(EntryPath(vertex_source, fragment_source), std::ios::binary);
    if (!file)
    {
        return 0;
    }
    const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() < 12 || std::memcmp(data.data(), kMagic, 4) != 0)
    {
        return 0;
    }
    Reader reader{data, 4};
    uint32_t layout = 0;
    uint32_t format = 0;
    const char* binary = nullptr;
    size_t binary_size = 0;
    if (!reader.U32(&layout) || layout != kLayoutVersion || !reader.U32(&format) ||
        !reader.Matches(Cache().driver) || !reader.Matches(vertex_source) || !reader.Matches(fragment_source) ||
        !reader.Block(&binary, &binary_size))
    {
        return 0;
    }
    GlFunctions& gl = Gl();
    const GLuint program = gl.CreateProgram();
    gl.ProgramBinary(program, format, binary, static_cast<GLsizei>(binary_size));
    GLint linked = 0;
    gl.GetProgramiv(program, kGlLinkStatus, &linked);
    if (linked == 0)
    {
        // The driver no longer takes this binary (an update, say): compile.
        gl.DeleteProgram(program);
        return 0;
    }
    return program;
}

bool StoreProgram(uint32_t program, const std::string& vertex_source, const std::string& fragment_source)
{
    if (!ProgramCacheEnabled())
    {
        return false;
    }
    GlFunctions& gl = Gl();
    GLint length = 0;
    gl.GetProgramiv(program, kGlProgramBinaryLength, &length);
    if (length <= 0)
    {
        return false;
    }
    std::vector<char> binary(static_cast<size_t>(length));
    GLsizei written = 0;
    GLenum format = 0;
    gl.GetProgramBinary(program, length, &written, &format, binary.data());
    if (written <= 0)
    {
        return false;
    }
    std::vector<char> out(kMagic, kMagic + 4);
    PutU32(&out, kLayoutVersion);
    PutU32(&out, format);
    PutBlock(&out, Cache().driver.data(), Cache().driver.size());
    PutBlock(&out, vertex_source.data(), vertex_source.size());
    PutBlock(&out, fragment_source.data(), fragment_source.size());
    PutBlock(&out, binary.data(), static_cast<size_t>(written));
    // Write beside the entry, then rename, so a reader never sees half a file.
    const std::filesystem::path path = EntryPath(vertex_source, fragment_source);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    // Closed and checked before the rename: a flush failing in the
    // destructor would otherwise publish a truncated entry.
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file.write(out.data(), static_cast<std::streamsize>(out.size()));
    file.close();
    std::error_code error;
    const char* failed = nullptr;
    if (!file)
    {
        failed = "cannot be written";
    }
    else
    {
        std::filesystem::rename(temporary, path, error);
        failed = error ? "cannot be renamed" : nullptr;
    }
    if (failed != nullptr)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        ReportOnce("shader cache store",
                   Format("shader cache entry %s %s%s%s; programs not stored are compiled again on the next run",
                          temporary.string().c_str(), failed, error ? ": " : "", error ? error.message().c_str() : ""));
        return false;
    }
    return true;
}

}  // namespace rexbox::hle::d3d8
