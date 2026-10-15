// The OpenGL entry points the D3D8 HLE uses, loaded through the platform
// display contract. Only what is called is declared here, so no GL header or
// loader library is needed; the types and values follow the GL specification
// (GL 3.3 core and GLES 3.0 share everything below unless marked optional).

#ifndef REXBOX_HLE_D3D8_D3D8_GL_H
#define REXBOX_HLE_D3D8_D3D8_GL_H

#include "rexbox/platform/display.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rexbox::hle::d3d8
{

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLboolean = unsigned char;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLdouble = double;
using GLubyte = unsigned char;
using GLchar = char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;

constexpr GLenum kGlVendor = 0x1F00;
constexpr GLenum kGlRenderer = 0x1F01;
constexpr GLenum kGlVersion = 0x1F02;
constexpr GLenum kGlNoError = 0;
constexpr GLbitfield kGlDepthBufferBit = 0x00000100;
constexpr GLbitfield kGlStencilBufferBit = 0x00000400;
constexpr GLbitfield kGlColorBufferBit = 0x00004000;

// Capabilities.
constexpr GLenum kGlCullFace = 0x0B44;
constexpr GLenum kGlDepthTest = 0x0B71;
constexpr GLenum kGlStencilTest = 0x0B90;
constexpr GLenum kGlBlend = 0x0BE2;
constexpr GLenum kGlScissorTest = 0x0C11;
constexpr GLenum kGlPolygonOffsetFill = 0x8037;

// Faces, polygon modes.
constexpr GLenum kGlFront = 0x0404;
constexpr GLenum kGlBack = 0x0405;
constexpr GLenum kGlFrontAndBack = 0x0408;
constexpr GLenum kGlCw = 0x0900;
constexpr GLenum kGlCcw = 0x0901;
constexpr GLenum kGlPoint = 0x1B00;
constexpr GLenum kGlLine = 0x1B01;
constexpr GLenum kGlFill = 0x1B02;

// Primitives.
constexpr GLenum kGlPoints = 0x0000;
constexpr GLenum kGlLines = 0x0001;
constexpr GLenum kGlLineLoop = 0x0002;
constexpr GLenum kGlLineStrip = 0x0003;
constexpr GLenum kGlTriangles = 0x0004;
constexpr GLenum kGlTriangleStrip = 0x0005;
constexpr GLenum kGlTriangleFan = 0x0006;

// Types.
constexpr GLenum kGlUnsignedByte = 0x1401;
constexpr GLenum kGlShort = 0x1402;
constexpr GLenum kGlUnsignedShort = 0x1403;
constexpr GLenum kGlUnsignedInt = 0x1405;
constexpr GLenum kGlFloat = 0x1406;
constexpr GLenum kGlUnsignedInt248 = 0x84FA;

// Buffers.
constexpr GLenum kGlArrayBuffer = 0x8892;
constexpr GLenum kGlElementArrayBuffer = 0x8893;
constexpr GLenum kGlStreamDraw = 0x88E0;
constexpr GLenum kGlStaticDraw = 0x88E4;

// Shaders.
constexpr GLenum kGlFragmentShader = 0x8B30;
constexpr GLenum kGlVertexShader = 0x8B31;
constexpr GLenum kGlCompileStatus = 0x8B81;
constexpr GLenum kGlLinkStatus = 0x8B82;
constexpr GLenum kGlInfoLogLength = 0x8B84;
constexpr GLenum kGlProgramBinaryRetrievableHint = 0x8257;
constexpr GLenum kGlProgramBinaryLength = 0x8741;
constexpr GLenum kGlNumProgramBinaryFormats = 0x87FE;

// Textures and samplers.
constexpr GLenum kGlTexture2D = 0x0DE1;
constexpr GLenum kGlTexture3D = 0x806F;
constexpr GLenum kGlTextureCubeMap = 0x8513;
constexpr GLenum kGlTextureCubeMapPositiveX = 0x8515;
constexpr GLenum kGlTexture0 = 0x84C0;
constexpr GLenum kGlRgba = 0x1908;
constexpr GLenum kGlRgba8 = 0x8058;
constexpr GLenum kGlDepthStencil = 0x84F9;
constexpr GLenum kGlDepth24Stencil8 = 0x88F0;
constexpr GLenum kGlUnpackAlignment = 0x0CF5;
constexpr GLenum kGlPackAlignment = 0x0D05;
constexpr GLenum kGlTextureMagFilter = 0x2800;
constexpr GLenum kGlTextureMinFilter = 0x2801;
constexpr GLenum kGlTextureWrapS = 0x2802;
constexpr GLenum kGlTextureWrapT = 0x2803;
constexpr GLenum kGlTextureWrapR = 0x8072;
constexpr GLenum kGlTextureMinLod = 0x813A;
constexpr GLenum kGlTextureMaxLod = 0x813B;
constexpr GLenum kGlTextureBaseLevel = 0x813C;
constexpr GLenum kGlTextureMaxLevel = 0x813D;
constexpr GLenum kGlNearest = 0x2600;
constexpr GLenum kGlLinear = 0x2601;
constexpr GLenum kGlNearestMipmapNearest = 0x2700;
constexpr GLenum kGlLinearMipmapNearest = 0x2701;
constexpr GLenum kGlNearestMipmapLinear = 0x2702;
constexpr GLenum kGlLinearMipmapLinear = 0x2703;
constexpr GLenum kGlRepeat = 0x2901;
constexpr GLenum kGlClampToEdge = 0x812F;
constexpr GLenum kGlMirroredRepeat = 0x8370;

// Framebuffers.
constexpr GLenum kGlFramebuffer = 0x8D40;
constexpr GLenum kGlReadFramebuffer = 0x8CA8;
constexpr GLenum kGlDrawFramebuffer = 0x8CA9;
constexpr GLenum kGlRenderbuffer = 0x8D41;
constexpr GLenum kGlColorAttachment0 = 0x8CE0;
constexpr GLenum kGlDepthStencilAttachment = 0x821A;
constexpr GLenum kGlFramebufferComplete = 0x8CD5;

struct GlFunctions
{
    // Queries and global state.
    const GLubyte*(REXBOX_GL_APIENTRY* GetString)(GLenum name) = nullptr;
    void(REXBOX_GL_APIENTRY* GetIntegerv)(GLenum pname, GLint* data) = nullptr;
    GLenum(REXBOX_GL_APIENTRY* GetError)() = nullptr;
    void(REXBOX_GL_APIENTRY* Viewport)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* Scissor)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* Enable)(GLenum cap) = nullptr;
    void(REXBOX_GL_APIENTRY* Disable)(GLenum cap) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearStencil)(GLint s) = nullptr;
    void(REXBOX_GL_APIENTRY* Clear)(GLbitfield mask) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthMask)(GLboolean flag) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthFunc)(GLenum func) = nullptr;
    void(REXBOX_GL_APIENTRY* ColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a) = nullptr;
    void(REXBOX_GL_APIENTRY* StencilMask)(GLuint mask) = nullptr;
    void(REXBOX_GL_APIENTRY* StencilFunc)(GLenum func, GLint ref, GLuint mask) = nullptr;
    void(REXBOX_GL_APIENTRY* StencilOp)(GLenum fail, GLenum zfail, GLenum zpass) = nullptr;
    void(REXBOX_GL_APIENTRY* BlendFunc)(GLenum sfactor, GLenum dfactor) = nullptr;
    void(REXBOX_GL_APIENTRY* BlendEquation)(GLenum mode) = nullptr;
    void(REXBOX_GL_APIENTRY* BlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = nullptr;
    void(REXBOX_GL_APIENTRY* CullFace)(GLenum mode) = nullptr;
    void(REXBOX_GL_APIENTRY* FrontFace)(GLenum mode) = nullptr;
    void(REXBOX_GL_APIENTRY* PolygonOffset)(GLfloat factor, GLfloat units) = nullptr;
    void(REXBOX_GL_APIENTRY* PixelStorei)(GLenum pname, GLint param) = nullptr;
    void(REXBOX_GL_APIENTRY* ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                         void* data) = nullptr;
    // One of the two exists on every host: glClearDepthf (GLES, GL 4.1) or
    // glClearDepth (desktop GL); likewise glDepthRangef / glDepthRange.
    void(REXBOX_GL_APIENTRY* ClearDepthf)(GLfloat depth) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearDepth)(GLdouble depth) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthRangef)(GLfloat near_value, GLfloat far_value) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthRange)(GLdouble near_value, GLdouble far_value) = nullptr;
    // Optional (desktop GL only).
    void(REXBOX_GL_APIENTRY* PolygonMode)(GLenum face, GLenum mode) = nullptr;

    // Shaders and programs.
    GLuint(REXBOX_GL_APIENTRY* CreateShader)(GLenum type) = nullptr;
    void(REXBOX_GL_APIENTRY* ShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string,
                                           const GLint* length) = nullptr;
    void(REXBOX_GL_APIENTRY* CompileShader)(GLuint shader) = nullptr;
    void(REXBOX_GL_APIENTRY* GetShaderiv)(GLuint shader, GLenum pname, GLint* params) = nullptr;
    void(REXBOX_GL_APIENTRY* GetShaderInfoLog)(GLuint shader, GLsizei max_length, GLsizei* length,
                                               GLchar* info_log) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteShader)(GLuint shader) = nullptr;
    GLuint(REXBOX_GL_APIENTRY* CreateProgram)() = nullptr;
    void(REXBOX_GL_APIENTRY* AttachShader)(GLuint program, GLuint shader) = nullptr;
    void(REXBOX_GL_APIENTRY* LinkProgram)(GLuint program) = nullptr;
    void(REXBOX_GL_APIENTRY* GetProgramiv)(GLuint program, GLenum pname, GLint* params) = nullptr;
    void(REXBOX_GL_APIENTRY* GetProgramInfoLog)(GLuint program, GLsizei max_length, GLsizei* length,
                                                GLchar* info_log) = nullptr;
    void(REXBOX_GL_APIENTRY* UseProgram)(GLuint program) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteProgram)(GLuint program) = nullptr;
    // Optional: program binaries (GL 4.1, ARB_get_program_binary, GLES 3.0).
    void(REXBOX_GL_APIENTRY* GetProgramBinary)(GLuint program, GLsizei buffer_size, GLsizei* length,
                                               GLenum* format, void* binary) = nullptr;
    void(REXBOX_GL_APIENTRY* ProgramBinary)(GLuint program, GLenum format, const void* binary,
                                            GLsizei length) = nullptr;
    void(REXBOX_GL_APIENTRY* ProgramParameteri)(GLuint program, GLenum pname, GLint value) = nullptr;
    GLint(REXBOX_GL_APIENTRY* GetUniformLocation)(GLuint program, const GLchar* name) = nullptr;
    void(REXBOX_GL_APIENTRY* Uniform1i)(GLint location, GLint v0) = nullptr;
    void(REXBOX_GL_APIENTRY* Uniform1f)(GLint location, GLfloat v0) = nullptr;
    void(REXBOX_GL_APIENTRY* Uniform2f)(GLint location, GLfloat v0, GLfloat v1) = nullptr;
    void(REXBOX_GL_APIENTRY* Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) = nullptr;
    void(REXBOX_GL_APIENTRY* Uniform4fv)(GLint location, GLsizei count, const GLfloat* value) = nullptr;
    void(REXBOX_GL_APIENTRY* UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
                                               const GLfloat* value) = nullptr;

    // Buffers and vertex arrays.
    void(REXBOX_GL_APIENTRY* GenBuffers)(GLsizei n, GLuint* buffers) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteBuffers)(GLsizei n, const GLuint* buffers) = nullptr;
    void(REXBOX_GL_APIENTRY* BindBuffer)(GLenum target, GLuint buffer) = nullptr;
    void(REXBOX_GL_APIENTRY* BufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage) = nullptr;
    void(REXBOX_GL_APIENTRY* BufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
                                            const void* data) = nullptr;
    void(REXBOX_GL_APIENTRY* GenVertexArrays)(GLsizei n, GLuint* arrays) = nullptr;
    void(REXBOX_GL_APIENTRY* BindVertexArray)(GLuint array) = nullptr;
    void(REXBOX_GL_APIENTRY* EnableVertexAttribArray)(GLuint index) = nullptr;
    void(REXBOX_GL_APIENTRY* DisableVertexAttribArray)(GLuint index) = nullptr;
    void(REXBOX_GL_APIENTRY* VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                  GLsizei stride, const void* pointer) = nullptr;
    void(REXBOX_GL_APIENTRY* DrawArrays)(GLenum mode, GLint first, GLsizei count) = nullptr;
    void(REXBOX_GL_APIENTRY* DrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices) = nullptr;

    // Textures and samplers.
    void(REXBOX_GL_APIENTRY* GenTextures)(GLsizei n, GLuint* textures) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteTextures)(GLsizei n, const GLuint* textures) = nullptr;
    void(REXBOX_GL_APIENTRY* BindTexture)(GLenum target, GLuint texture) = nullptr;
    void(REXBOX_GL_APIENTRY* ActiveTexture)(GLenum texture) = nullptr;
    void(REXBOX_GL_APIENTRY* TexImage2D)(GLenum target, GLint level, GLint internal_format, GLsizei width,
                                         GLsizei height, GLint border, GLenum format, GLenum type,
                                         const void* pixels) = nullptr;
    void(REXBOX_GL_APIENTRY* TexImage3D)(GLenum target, GLint level, GLint internal_format, GLsizei width,
                                         GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                         const void* pixels) = nullptr;
    void(REXBOX_GL_APIENTRY* TexParameteri)(GLenum target, GLenum pname, GLint param) = nullptr;
    void(REXBOX_GL_APIENTRY* GenSamplers)(GLsizei count, GLuint* samplers) = nullptr;
    void(REXBOX_GL_APIENTRY* BindSampler)(GLuint unit, GLuint sampler) = nullptr;
    void(REXBOX_GL_APIENTRY* SamplerParameteri)(GLuint sampler, GLenum pname, GLint param) = nullptr;
    void(REXBOX_GL_APIENTRY* SamplerParameterf)(GLuint sampler, GLenum pname, GLfloat param) = nullptr;

    // Framebuffers.
    void(REXBOX_GL_APIENTRY* GenFramebuffers)(GLsizei n, GLuint* framebuffers) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteFramebuffers)(GLsizei n, const GLuint* framebuffers) = nullptr;
    void(REXBOX_GL_APIENTRY* BindFramebuffer)(GLenum target, GLuint framebuffer) = nullptr;
    void(REXBOX_GL_APIENTRY* FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
                                                   GLuint texture, GLint level) = nullptr;
    void(REXBOX_GL_APIENTRY* FramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                                      GLuint renderbuffer) = nullptr;
    GLenum(REXBOX_GL_APIENTRY* CheckFramebufferStatus)(GLenum target) = nullptr;
    void(REXBOX_GL_APIENTRY* GenRenderbuffers)(GLsizei n, GLuint* renderbuffers) = nullptr;
    void(REXBOX_GL_APIENTRY* DeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers) = nullptr;
    void(REXBOX_GL_APIENTRY* BindRenderbuffer)(GLenum target, GLuint renderbuffer) = nullptr;
    void(REXBOX_GL_APIENTRY* RenderbufferStorage)(GLenum target, GLenum internal_format, GLsizei width,
                                                  GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* BlitFramebuffer)(GLint src_x0, GLint src_y0, GLint src_x1, GLint src_y1, GLint dst_x0,
                                              GLint dst_y0, GLint dst_x1, GLint dst_y1, GLbitfield mask,
                                              GLenum filter) = nullptr;
};

// Fills the table from the current display's context. Returns false and
// names the first missing entry point in `missing`.
bool LoadGlFunctions(GlFunctions* functions, std::string* missing);

// The loaded table (valid after LoadGlFunctions succeeded).
GlFunctions& Gl();

// Whether the context is OpenGL ES (shader version, optional entry points).
bool GlIsEs();
void SetGlIsEs(bool es);

// Helpers over the float/double pairs.
void GlClearDepth(float depth);
void GlDepthRange(float near_value, float far_value);

// The draw path remembers the GL state it set and skips calls that would set
// it again (program, vertex arrays, textures and samplers, fixed per-fragment
// state). Code that changes that state behind its back (present, clears,
// texture uploads, object creation) calls InvalidateGlStateCache, and each
// cache compares the epoch it was filled in with GlStateEpoch.
void InvalidateGlStateCache();

// REXBOX_GL_CALLS=1: every GL entry point is called through a counting
// wrapper (diagnostic). TakeGlCallCounts returns the calls since the last
// take by entry point, most first, and starts counting again.
bool GlCallCountingEnabled();
std::vector<std::pair<const char*, uint64_t>> TakeGlCallCounts();
uint32_t GlStateEpoch();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_H
