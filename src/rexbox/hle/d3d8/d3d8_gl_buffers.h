// Vertex and index data for GL draws (docs/design/20261013-i040-draw-performance.md).
//
// Draws read their vertices and indices from guest memory. Ranges that come
// from the title's buffers are kept in GL buffers: a draw compares the guest
// bytes with the copy made at the last upload and uploads only when they
// differ, so unchanged geometry is not sent again. Data passed with the draw
// (the UP calls) and generated indices are appended to streaming buffers,
// which are refilled from the start (orphaned) when full.
//
// Vertex arrays (docs/design/20261014-i050-gl-calls.md): a cached vertex
// range gets one vertex array object per attribute layout, pointed once, so
// a draw that uses it binds one object instead of re-pointing attributes.
// Streamed vertices use a shared array whose attribute state is sent only
// where it changes. Uploads never touch a vertex array's element binding.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H
#define REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

// The attributes a draw reads, with offsets inside one vertex.
struct VertexInput
{
    struct Attribute
    {
        uint32_t index = 0;
        int size = 0;
        uint32_t type = 0;
        bool normalized = false;
        uint32_t offset = 0;
        bool operator==(const Attribute&) const = default;
    };
    static constexpr uint32_t kMaxAttributes = 16;
    Attribute attributes[kMaxAttributes];
    uint32_t count = 0;
    uint32_t stride = 0;

    void Add(uint32_t index, int size, uint32_t type, bool normalized, uint32_t offset)
    {
        attributes[count++] = {index, size, type, normalized, offset};
    }
    bool operator==(const VertexInput& other) const;
};

// Vertex bytes in a GL buffer.
struct VertexData
{
    uint32_t buffer = 0;
    size_t offset = 0;    // of vertex 0 in the buffer
    bool cached = false;  // a kept range (its own buffer), not streamed
};

// Puts `bytes` vertex bytes from guest `address` in a GL buffer. `cacheable`
// ranges (from a vertex buffer) are kept and reused while the guest bytes
// stay the same; others are streamed.
VertexData PrepareVertexData(uint32_t address, size_t bytes, bool cacheable);

// Indices ready for glDrawElements, rebased so that vertex `first` is 0.
struct DrawIndices
{
    uint32_t first = 0;         // lowest vertex number used
    uint32_t vertex_count = 0;  // highest - lowest + 1
    uint32_t count = 0;         // indices to draw
    bool use_short = true;      // 16-bit, else 32-bit
    uint32_t buffer = 0;        // the element buffer
    size_t offset = 0;          // in the element buffer
};

// `count` 16-bit guest indices at `address`, each plus `base`; quad lists
// are expanded to triangles. `cacheable` index data (the title's index
// buffers) is kept and reused while the guest bytes stay the same. False
// when the data is outside guest memory or no index is left to draw.
bool PrepareGuestIndices(uint32_t address, uint32_t count, uint32_t base, bool quads, bool cacheable,
                         DrawIndices* out);

// Indices built on the host (vertex numbers), streamed.
bool PrepareHostIndices(const std::vector<uint32_t>& indices, DrawIndices* out);

// Binds a vertex array reading `input` from `data`, with `indices`' buffer
// as its element buffer (none for a draw without indices).
void BindVertexInput(const VertexInput& input, const VertexData& data, const DrawIndices* indices);

// Once per present: drops cached ranges (and their vertex arrays) no draw
// has used for a while.
void EndVertexDataFrame();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H
