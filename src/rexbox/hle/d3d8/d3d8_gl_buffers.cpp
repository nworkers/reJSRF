#include "d3d8_gl_buffers.h"

#include "d3d8_gl.h"
#include "d3d8_internal.h"
#include "d3d8_profile.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rexbox::hle::d3d8
{

bool VertexInput::operator==(const VertexInput& other) const
{
    return stride == other.stride && count == other.count &&
           std::equal(attributes, attributes + count, other.attributes);
}

namespace
{

// Cached ranges unused for this many presents are dropped.
constexpr uint32_t kIdleFrames = 600;

// Ranges this large ask the guest memory's write version before comparing:
// one query costs about as much as comparing 6 KiB
// (docs/design/20261014-i046-hle-performance.md section 3.2).
constexpr size_t kTrackedBytes = 8 * 1024;

// A buffer binding point, remembered while `epoch` is the GL state epoch.
// Neither binding below is vertex array state.
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
    void Forget(GLuint name)
    {
        if (buffer == name)
        {
            epoch = 0;
        }
    }
};

// Vertex data is uploaded through GL_ARRAY_BUFFER, index data through
// GL_COPY_WRITE_BUFFER: GL_ELEMENT_ARRAY_BUFFER is part of whichever vertex
// array is bound, and an upload through it would change that array.
Binding g_array_binding{kGlArrayBuffer};
Binding g_copy_binding{kGlCopyWriteBuffer};

// Data appended behind the previous data with glBufferSubData; a full buffer
// is orphaned (the driver keeps the old storage for draws in flight) and
// filling starts again at 0.
class StreamBuffer
{
public:
    StreamBuffer(Binding* binding) : binding_(binding) {}

    GLuint buffer() const { return buffer_; }

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
StreamBuffer g_index_stream(&g_copy_binding);

// Guest data and the GL buffer holding its last upload.
struct CachedRange
{
    GLuint buffer = 0;
    std::vector<uint8_t> copy;   // the guest bytes at the last upload
    uint32_t last_used = 0;      // frame
    uint64_t write_version = 0;  // guest write version when `copy` last matched
    DrawIndices indices;         // index ranges: the prepared result
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

// A vertex array and the element buffer it holds (vertex array state, so it
// stays valid when other code binds its own arrays; unknown once a buffer it
// may hold has been deleted).
struct VertexArray
{
    GLuint name = 0;
    GLuint element = 0;
    bool element_known = false;
};

// The array bound to GL_VERTEX_ARRAY_BINDING, valid in g_bound_epoch.
GLuint g_bound_array = 0;
uint32_t g_bound_epoch = 0;

void BindArray(const VertexArray& array)
{
    if (g_bound_epoch == GlStateEpoch() && g_bound_array == array.name)
    {
        return;
    }
    Gl().BindVertexArray(array.name);
    g_bound_array = array.name;
    g_bound_epoch = GlStateEpoch();
}

void BindElements(VertexArray* array, const DrawIndices* indices)
{
    if (indices == nullptr || (array->element_known && array->element == indices->buffer))
    {
        return;
    }
    Gl().BindBuffer(kGlElementArrayBuffer, indices->buffer);
    array->element = indices->buffer;
    array->element_known = true;
}

// One array per (cached vertex buffer, input layout), pointed when made.
struct ArrayKey
{
    GLuint buffer = 0;
    VertexInput input;
    bool operator==(const ArrayKey&) const = default;
};

struct ArrayKeyHash
{
    size_t operator()(const ArrayKey& key) const
    {
        uint64_t hash = (1469598103934665603ull ^ key.buffer) * 1099511628211ull;
        hash = (hash ^ key.input.stride ^ (static_cast<uint64_t>(key.input.count) << 32)) * 1099511628211ull;
        for (uint32_t i = 0; i < key.input.count; ++i)
        {
            const VertexInput::Attribute& a = key.input.attributes[i];
            hash = (hash ^ (a.index | (static_cast<uint64_t>(a.size) << 8) | (static_cast<uint64_t>(a.type) << 16) |
                            (a.normalized ? 1ull << 48 : 0))) *
                   1099511628211ull;
            hash = (hash ^ a.offset) * 1099511628211ull;
        }
        return static_cast<size_t>(hash);
    }
};

std::unordered_map<ArrayKey, VertexArray, ArrayKeyHash> g_arrays;

// The shared array for streamed vertices, whose attribute state is sent only
// where it differs from what the previous draw left.
struct AttributeState
{
    bool pointed = false;
    GLuint buffer = 0;
    VertexInput::Attribute format;
    uint32_t stride = 0;
    size_t offset = 0;
};
VertexArray g_shared;
AttributeState g_attributes[VertexInput::kMaxAttributes];
uint32_t g_enabled_attributes = 0;

void ForgetSharedPointers()
{
    for (AttributeState& attribute : g_attributes)
    {
        attribute.pointed = false;
    }
}

void PointAttribute(const VertexInput::Attribute& a, uint32_t stride, size_t offset)
{
    Gl().VertexAttribPointer(a.index, a.size, a.type, a.normalized ? 1 : 0, static_cast<GLsizei>(stride),
                             reinterpret_cast<const void*>(offset));
}

void BindShared(const VertexInput& input, const VertexData& data)
{
    GlFunctions& gl = Gl();
    if (g_shared.name == 0)
    {
        gl.GenVertexArrays(1, &g_shared.name);
        ForgetSharedPointers();
        g_enabled_attributes = 0;
    }
    BindArray(g_shared);
    g_array_binding.Bind(data.buffer);
    uint32_t named = 0;
    for (uint32_t i = 0; i < input.count; ++i)
    {
        const VertexInput::Attribute& a = input.attributes[i];
        const uint32_t bit = 1u << a.index;
        named |= bit;
        if ((g_enabled_attributes & bit) == 0)
        {
            gl.EnableVertexAttribArray(a.index);
            g_enabled_attributes |= bit;
        }
        AttributeState& state = g_attributes[a.index];
        const size_t offset = data.offset + a.offset;
        if (state.pointed && state.buffer == data.buffer && state.format == a && state.stride == input.stride &&
            state.offset == offset)
        {
            continue;
        }
        PointAttribute(a, input.stride, offset);
        state = {true, data.buffer, a, input.stride, offset};
    }
    uint32_t unused = g_enabled_attributes & ~named;
    for (uint32_t index = 0; unused != 0; ++index, unused >>= 1)
    {
        if ((unused & 1) != 0)
        {
            gl.DisableVertexAttribArray(index);
        }
    }
    g_enabled_attributes &= named;
}

// True when `range` holds the `bytes` at guest `address` (and marks it
// used); otherwise remembers them and the caller uploads them. A large range
// whose pages have not been written since it last matched is not compared.
bool Unchanged(CachedRange* range, GuestAddress address, size_t bytes)
{
    runtime::GuestMemory& memory = Memory();
    const uint8_t* data = memory.Translate(address);
    range->last_used = State().frame_count;
    const bool tracked = bytes >= kTrackedBytes && memory.TracksWrites();
    const uint64_t version = tracked ? memory.WriteVersion(address, bytes) : 0;
    if (range->buffer == 0)
    {
        Gl().GenBuffers(1, &range->buffer);
    }
    else if (range->copy.size() == bytes)
    {
        if (tracked && version == range->write_version)
        {
            return true;
        }
        profile::CountCompare(bytes);
        if (std::memcmp(range->copy.data(), data, bytes) == 0)
        {
            range->write_version = version;
            return true;
        }
    }
    range->copy.assign(data, data + bytes);
    range->write_version = version;
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

// Deletes the ranges unused for kIdleFrames; returns the deleted buffers.
template <typename Map>
std::unordered_set<GLuint> Sweep(Map* map, uint32_t frame)
{
    GlFunctions& gl = Gl();
    std::unordered_set<GLuint> deleted;
    for (auto it = map->begin(); it != map->end();)
    {
        if (frame - it->second.last_used >= kIdleFrames)
        {
            const GLuint buffer = it->second.buffer;
            g_array_binding.Forget(buffer);
            g_copy_binding.Forget(buffer);
            gl.DeleteBuffers(1, &buffer);
            deleted.insert(buffer);
            it = map->erase(it);
        }
        else
        {
            ++it;
        }
    }
    return deleted;
}

}  // namespace

VertexData PrepareVertexData(uint32_t address, size_t bytes, bool cacheable)
{
    const uint8_t* data = Memory().Translate(address);
    VertexData result;
    if (!cacheable)
    {
        profile::CountUpload(bytes, 0);
        result.offset = g_vertex_stream.Append(data, bytes);
        result.buffer = g_vertex_stream.buffer();
        return result;
    }
    CachedRange& range = g_vertex_ranges[(static_cast<uint64_t>(address) << 32) | static_cast<uint32_t>(bytes)];
    if (!Unchanged(&range, address, bytes))
    {
        // Respecify the storage so draws in flight keep the old contents.
        g_array_binding.Bind(range.buffer);
        Gl().BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(bytes), data, kGlStaticDraw);
        profile::CountUpload(bytes, 0);
    }
    result.buffer = range.buffer;
    result.cached = true;
    return result;
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
        if (Unchanged(range, address, count * 2))
        {
            if (range->indices.count == 0)
            {
                return false;
            }
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
        result.buffer = g_index_stream.buffer();
        profile::CountUpload(0, bytes.size());
        *out = result;
        return true;
    }
    range->indices = ok ? result : DrawIndices{};
    if (!ok)
    {
        return false;
    }
    g_copy_binding.Bind(range->buffer);
    Gl().BufferData(kGlCopyWriteBuffer, static_cast<GLsizeiptr>(bytes.size()), bytes.data(), kGlStaticDraw);
    profile::CountUpload(0, bytes.size());
    range->indices.buffer = range->buffer;
    *out = range->indices;
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
    out->buffer = g_index_stream.buffer();
    profile::CountUpload(0, bytes.size());
    return true;
}

void BindVertexInput(const VertexInput& input, const VertexData& data, const DrawIndices* indices)
{
    if (!data.cached)
    {
        BindShared(input, data);
        BindElements(&g_shared, indices);
        return;
    }
    auto [it, made] = g_arrays.try_emplace(ArrayKey{data.buffer, input});
    VertexArray& array = it->second;
    if (made)
    {
        GlFunctions& gl = Gl();
        gl.GenVertexArrays(1, &array.name);
        BindArray(array);
        g_array_binding.Bind(data.buffer);
        for (uint32_t i = 0; i < input.count; ++i)
        {
            gl.EnableVertexAttribArray(input.attributes[i].index);
            PointAttribute(input.attributes[i], input.stride, data.offset + input.attributes[i].offset);
        }
    }
    else
    {
        BindArray(array);
    }
    BindElements(&array, indices);
}

void EndVertexDataFrame()
{
    const uint32_t frame = State().frame_count;
    if (frame - g_last_sweep < kIdleFrames)
    {
        return;
    }
    g_last_sweep = frame;
    GlFunctions& gl = Gl();
    const std::unordered_set<GLuint> vertex_buffers = Sweep(&g_vertex_ranges, frame);
    const std::unordered_set<GLuint> index_buffers = Sweep(&g_index_ranges, frame);
    if (!vertex_buffers.empty())
    {
        // The arrays of deleted vertex buffers go with them.
        for (auto it = g_arrays.begin(); it != g_arrays.end();)
        {
            if (vertex_buffers.count(it->first.buffer) != 0)
            {
                if (g_bound_array == it->second.name)
                {
                    g_bound_epoch = 0;
                }
                gl.DeleteVertexArrays(1, &it->second.name);
                it = g_arrays.erase(it);
            }
            else
            {
                ++it;
            }
        }
        ForgetSharedPointers();
    }
    if (!index_buffers.empty())
    {
        // A deleted name can return for a new buffer while an array still
        // holds the old one: forget every element binding.
        for (auto& entry : g_arrays)
        {
            entry.second.element_known = false;
        }
        g_shared.element_known = false;
    }
}

}  // namespace rexbox::hle::d3d8
