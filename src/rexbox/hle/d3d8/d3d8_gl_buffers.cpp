#include "d3d8_gl_buffers.h"

#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "d3d8_profile.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace rexbox::hle::d3d8
{

namespace
{

// Cached ranges unused for this many presents are dropped.
constexpr uint32_t kIdleFrames = 600;

GLuint g_vertex_array = 0;
uint32_t g_vertex_array_epoch = 0;

// A buffer binding the draws change, valid while `epoch` is the GL state
// epoch. The element binding belongs to the vertex array object, which only
// the draws use.
struct Binding
{
    GLenum target;
    GLuint buffer = 0;
    uint32_t epoch = 0;

    void Bind(GLuint name)
    {
        if (epoch == GlStateEpoch() && buffer == name)
        {
            return;
        }
        Gl().BindBuffer(target, name);
        buffer = name;
        epoch = GlStateEpoch();
    }
};

Binding g_array_binding{kGlArrayBuffer};
Binding g_element_binding{kGlElementArrayBuffer};

// Data appended behind the previous data with glBufferSubData; a full buffer
// is orphaned (the driver keeps the old storage for draws in flight) and
// filling starts again at 0.
class StreamBuffer
{
public:
    StreamBuffer(Binding* binding) : binding_(binding) {}

    // Binds the buffer and appends; returns the data's offset.
    size_t Append(const void* data, size_t bytes)
    {
        GlFunctions& gl = Gl();
        if (buffer_ == 0)
        {
            gl.GenBuffers(1, &buffer_);
        }
        binding_->Bind(buffer_);
        size_t offset = (offset_ + kAlignment - 1) & ~(kAlignment - 1);
        if (offset + bytes > capacity_)
        {
            capacity_ = std::max(capacity_, kInitialBytes);
            while (capacity_ < bytes)
            {
                capacity_ *= 2;
            }
            gl.BufferData(binding_->target, static_cast<GLsizeiptr>(capacity_), nullptr, kGlStreamDraw);
            offset = 0;
        }
        gl.BufferSubData(binding_->target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes), data);
        offset_ = offset + bytes;
        return offset;
    }

private:
    static constexpr size_t kInitialBytes = 4u << 20;
    static constexpr size_t kAlignment = 16;  // any attribute type and index size
    Binding* binding_;
    GLuint buffer_ = 0;
    size_t capacity_ = 0;
    size_t offset_ = 0;
};

StreamBuffer g_vertex_stream(&g_array_binding);
StreamBuffer g_index_stream(&g_element_binding);

// Guest data and the GL buffer holding its last upload.
struct CachedRange
{
    GLuint buffer = 0;
    std::vector<uint8_t> copy;  // the guest bytes at the last upload
    uint32_t last_used = 0;     // frame
    DrawIndices indices;          // index ranges: the prepared result
};

struct IndexKey
{
    uint32_t address = 0;
    uint32_t count = 0;
    uint32_t base = 0;
    bool quads = false;
    bool operator==(const IndexKey&) const = default;
};

struct IndexKeyHash
{
    size_t operator()(const IndexKey& key) const
    {
        uint64_t hash = (static_cast<uint64_t>(key.address) << 32) ^ key.count;
        hash = (hash ^ key.base) * 1099511628211ull;
        return static_cast<size_t>(hash ^ (key.quads ? 1 : 0));
    }
};

std::unordered_map<uint64_t, CachedRange> g_vertex_ranges;
std::unordered_map<IndexKey, CachedRange, IndexKeyHash> g_index_ranges;
uint32_t g_last_sweep = 0;

// True when `range` holds `bytes` of `data` (and marks it used); otherwise
// remembers `data` and the caller uploads it.
bool Unchanged(CachedRange* range, const uint8_t* data, size_t bytes)
{
    range->last_used = State().frame_count;
    if (range->buffer == 0)
    {
        Gl().GenBuffers(1, &range->buffer);
    }
    else if (range->copy.size() == bytes && std::memcmp(range->copy.data(), data, bytes) == 0)
    {
        return true;
    }
    range->copy.assign(data, data + bytes);
    return false;
}

// Rebases `indices` to their lowest vertex as 16- or 32-bit values.
bool Convert(const std::vector<uint32_t>& indices, DrawIndices* out, std::vector<uint8_t>* bytes)
{
    if (indices.empty())
    {
        return false;
    }
    const auto [low, high] = std::minmax_element(indices.begin(), indices.end());
    out->first = *low;
    out->vertex_count = *high - *low + 1;
    out->count = static_cast<uint32_t>(indices.size());
    out->use_short = out->vertex_count <= 0x10000;
    bytes->resize(indices.size() * (out->use_short ? 2 : 4));
    if (out->use_short)
    {
        uint16_t* p = reinterpret_cast<uint16_t*>(bytes->data());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            p[i] = static_cast<uint16_t>(indices[i] - out->first);
        }
    }
    else
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(bytes->data());
        for (size_t i = 0; i < indices.size(); ++i)
        {
            p[i] = indices[i] - out->first;
        }
    }
    return true;
}

template <typename Map>
void Sweep(Map* map, uint32_t frame, Binding* binding)
{
    GlFunctions& gl = Gl();
    for (auto it = map->begin(); it != map->end();)
    {
        if (frame - it->second.last_used >= kIdleFrames)
        {
            if (binding->buffer == it->second.buffer)
            {
                binding->epoch = 0;
            }
            gl.DeleteBuffers(1, &it->second.buffer);
            it = map->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace

void BindDrawVertexArray()
{
    GlFunctions& gl = Gl();
    if (g_vertex_array == 0)
    {
        gl.GenVertexArrays(1, &g_vertex_array);
    }
    else if (g_vertex_array_epoch == GlStateEpoch())
    {
        return;
    }
    gl.BindVertexArray(g_vertex_array);
    g_vertex_array_epoch = GlStateEpoch();
    g_element_binding.epoch = 0;  // re-sent with the next index data
}

size_t PrepareVertexData(uint32_t address, size_t bytes, bool cacheable)
{
    const uint8_t* data = Memory().Translate(address);
    if (!cacheable)
    {
        profile::CountUpload(bytes, 0);
        return g_vertex_stream.Append(data, bytes);
    }
    CachedRange& range = g_vertex_ranges[(static_cast<uint64_t>(address) << 32) | static_cast<uint32_t>(bytes)];
    const bool unchanged = Unchanged(&range, data, bytes);
    g_array_binding.Bind(range.buffer);
    if (!unchanged)
    {
        // Respecify the storage so draws in flight keep the old contents.
        Gl().BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(bytes), data, kGlStaticDraw);
        profile::CountUpload(bytes, 0);
    }
    return 0;
}

bool PrepareGuestIndices(uint32_t address, uint32_t count, uint32_t base, bool quads, bool cacheable,
                         DrawIndices* out)
{
    runtime::GuestMemory& memory = Memory();
    address &= kPhysicalMask;
    if (count == 0 || !memory.IsValid(address, count * 2))
    {
        return false;
    }
    const uint8_t* data = memory.Translate(address);
    CachedRange* range = nullptr;
    if (cacheable)
    {
        range = &g_index_ranges[IndexKey{address, count, base, quads}];
        if (Unchanged(range, data, count * 2))
        {
            if (range->indices.count == 0)
            {
                return false;
            }
            g_element_binding.Bind(range->buffer);
            *out = range->indices;
            return true;
        }
    }
    std::vector<uint32_t> indices(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        uint16_t index = 0;
        std::memcpy(&index, data + i * 2, 2);
        indices[i] = index + base;
    }
    if (quads)
    {
        std::vector<uint32_t> triangles;
        triangles.reserve(indices.size() / 4 * 6);
        for (size_t q = 0; q + 3 < indices.size(); q += 4)
        {
            triangles.insert(triangles.end(),
                             {indices[q], indices[q + 1], indices[q + 2], indices[q], indices[q + 2], indices[q + 3]});
        }
        indices.swap(triangles);
    }
    std::vector<uint8_t> bytes;
    DrawIndices result;
    const bool ok = Convert(indices, &result, &bytes);
    if (range == nullptr)
    {
        if (!ok)
        {
            return false;
        }
        result.offset = g_index_stream.Append(bytes.data(), bytes.size());
        profile::CountUpload(0, bytes.size());
        *out = result;
        return true;
    }
    range->indices = ok ? result : DrawIndices{};
    if (!ok)
    {
        return false;
    }
    g_element_binding.Bind(range->buffer);
    Gl().BufferData(kGlElementArrayBuffer, static_cast<GLsizeiptr>(bytes.size()), bytes.data(), kGlStaticDraw);
    profile::CountUpload(0, bytes.size());
    *out = result;
    return true;
}

bool PrepareHostIndices(const std::vector<uint32_t>& indices, DrawIndices* out)
{
    std::vector<uint8_t> bytes;
    if (!Convert(indices, out, &bytes))
    {
        return false;
    }
    out->offset = g_index_stream.Append(bytes.data(), bytes.size());
    profile::CountUpload(0, bytes.size());
    return true;
}

void EndVertexDataFrame()
{
    const uint32_t frame = State().frame_count;
    if (frame - g_last_sweep < kIdleFrames)
    {
        return;
    }
    g_last_sweep = frame;
    Sweep(&g_vertex_ranges, frame, &g_array_binding);
    Sweep(&g_index_ranges, frame, &g_element_binding);
}

}  // namespace rexbox::hle::d3d8
