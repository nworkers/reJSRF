// Linked GL programs kept on disk across runs
// (docs/design/20261014-i051-program-cache.md).
//
// Compiling and linking a program takes tens of milliseconds on some
// drivers, and the title asks for new programs in the middle of scenes. A
// program the driver can hand back as a binary (ARB_get_program_binary,
// core in GL 4.1 and GLES 3.0) is stored with the driver's identity and the
// full sources; the next run creates it from the binary when both match, and
// compiles as usual when there is no entry or the driver rejects it.
//
// REXBOX_SHADER_CACHE=<dir> chooses the directory (default cache/shaders in
// the current folder); REXBOX_SHADER_CACHE=0 turns the cache off.

#ifndef REXBOX_HLE_D3D8_D3D8_GL_PROGRAM_CACHE_H
#define REXBOX_HLE_D3D8_D3D8_GL_PROGRAM_CACHE_H

#include <cstdint>
#include <string>

namespace rexbox::hle::d3d8
{

// Whether programs are cached: on, and the driver offers binary formats.
bool ProgramCacheEnabled();

// A linked program made from the stored binary of these sources, or 0.
uint32_t LoadCachedProgram(const std::string& vertex_source, const std::string& fragment_source);

// Stores a program linked from these sources. The program must have been
// linked with the retrievable hint (MarkProgramRetrievable).
void StoreProgram(uint32_t program, const std::string& vertex_source, const std::string& fragment_source);

// Asks the driver, before linking, to keep the program's binary.
void MarkProgramRetrievable(uint32_t program);

}  // namespace rexbox::hle::d3d8

#endif  // REXBOX_HLE_D3D8_D3D8_GL_PROGRAM_CACHE_H
