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
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLubyte = unsigned char;

constexpr GLenum kGlVendor = 0x1F00;
constexpr GLenum kGlRenderer = 0x1F01;
constexpr GLenum kGlVersion = 0x1F02;
constexpr GLbitfield kGlDepthBufferBit = 0x00000100;
constexpr GLbitfield kGlStencilBufferBit = 0x00000400;
constexpr GLbitfield kGlColorBufferBit = 0x00004000;

struct GlFunctions
{
    const GLubyte*(REXBOX_GL_APIENTRY* GetString)(GLenum name) = nullptr;
    void(REXBOX_GL_APIENTRY* GetIntegerv)(GLenum pname, GLint* data) = nullptr;
    void(REXBOX_GL_APIENTRY* Viewport)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void(REXBOX_GL_APIENTRY* ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = nullptr;
    void(REXBOX_GL_APIENTRY* Clear)(GLbitfield mask) = nullptr;
};

// Fills the table from the current display's context. Returns false and
// names the first missing entry point in `missing`.
bool LoadGlFunctions(GlFunctions* functions, std::string* missing);

// The loaded table (valid after LoadGlFunctions succeeded).
GlFunctions& Gl();

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_H
