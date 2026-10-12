// The OpenGL entry points the D3D8 HLE uses, loaded through the platform
// display contract. Only what is called is declared here, so no GL header or
// loader library is needed; the types follow the GL specification.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_H
#define REXBOX_HLE_D3D8_D3D8_GL_H

#include "rexbox/platform/display.h"

#include <cstdint>
#include <string>

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

constexpr GLenum kGlVendor = 0x1F00;
constexpr GLenum kGlRenderer = 0x1F01;
constexpr GLenum kGlVersion = 0x1F02;
constexpr GLenum kGlScissorTest = 0x0C11;
constexpr GLbitfield kGlDepthBufferBit = 0x00000100;
constexpr GLbitfield kGlStencilBufferBit = 0x00000400;
constexpr GLbitfield kGlColorBufferBit = 0x00004000;

struct GlFunctions
{
    const GLubyte*(REXBOX_GL_APIENTRY* GetString)(GLenum name) = nullptr;
    void(REXBOX_GL_APIENTRY* GetIntegerv)(GLenum pname, GLint* data) = nullptr;
    void(REXBOX_GL_APIENTRY* Viewport)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* Scissor)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* Enable)(GLenum cap) = nullptr;
    void(REXBOX_GL_APIENTRY* Disable)(GLenum cap) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearStencil)(GLint s) = nullptr;
    void(REXBOX_GL_APIENTRY* Clear)(GLbitfield mask) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthMask)(GLboolean flag) = nullptr;
    void(REXBOX_GL_APIENTRY* ColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a) = nullptr;
    void(REXBOX_GL_APIENTRY* StencilMask)(GLuint mask) = nullptr;
    // One of the two exists on every host: glClearDepthf (GLES, GL 4.1) or
    // glClearDepth (desktop GL); likewise glDepthRangef / glDepthRange.
    void(REXBOX_GL_APIENTRY* ClearDepthf)(GLfloat depth) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearDepth)(GLdouble depth) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthRangef)(GLfloat near_value, GLfloat far_value) = nullptr;
    void(REXBOX_GL_APIENTRY* DepthRange)(GLdouble near_value, GLdouble far_value) = nullptr;
};

// Fills the table from the current display's context. Returns false and
// names the first missing entry point in `missing`.
bool LoadGlFunctions(GlFunctions* functions, std::string* missing);

// The loaded table (valid after LoadGlFunctions succeeded).
GlFunctions& Gl();

// Helpers over the float/double pairs.
void GlClearDepth(float depth);
void GlDepthRange(float near_value, float far_value);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_H
