// Vertex and index data for GL draws (docs/design/20261013-i040-draw-performance.md).
//
// Draws read their vertices and indices from guest memory. Ranges that come
// from the title's buffers are kept in GL buffers: a draw compares the guest
// bytes with the copy made at the last upload and uploads only when they
// differ, so unchanged geometry is not sent again. Data passed with the draw
// (the UP calls) and generated indices are appended to streaming buffers,
// which are refilled from the start (orphaned) when full.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H
#define REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rexbox::hle::d3d8
{

// Binds the vertex array object the draws use. Call before preparing data.
void BindDrawVertexArray();

// The attributes of the draw vertex array object, sent only where they
// differ from what the previous draw left (enable bits, and per attribute the
// buffer, format, stride and offset). SetAttribute points `index` at
// `offset` in the bound array buffer and enables it; FinishAttributes
// disables the enabled attributes no SetAttribute named since the last call.
void SetAttribute(uint32_t index, int size, uint32_t type, bool normalized, uint32_t stride, size_t offset);
void FinishAttributes();

// Puts `bytes` vertex bytes from guest `address` in a GL buffer and binds it
// as the array buffer. `cacheable` ranges (from a vertex buffer) are kept
// and reused while the guest bytes stay the same; others are streamed.
// Returns the data's offset in the bound buffer.
size_t PrepareVertexData(uint32_t address, size_t bytes, bool cacheable);

// Indices ready for glDrawElements in the bound element buffer, rebased so
// that vertex `first` is 0.
struct DrawIndices
{
    uint32_t first = 0;         // lowest vertex number used
    uint32_t vertex_count = 0;  // highest - lowest + 1
    uint32_t count = 0;         // indices to draw
    bool use_short = true;      // 16-bit, else 32-bit
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

// Once per present: drops cached ranges no draw has used for a while.
void EndVertexDataFrame();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_BUFFERS_H
