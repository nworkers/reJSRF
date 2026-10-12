// Platform contract: the one window and its OpenGL-family context.
//
// The D3D8 HLE draws through the OpenGL family (desktop GL 3.3 core, GLES
// 3.0, WebGL2) on every host. The platform owns the window and the context
// and hands out GL entry points; callers pass plain values and get plain
// values back. Everything here may be called from any thread; the host
// backend decides which thread does the work (src/platform/sdl/).

#ifndef REXBOX_PLATFORM_DISPLAY_H
#define REXBOX_PLATFORM_DISPLAY_H

#include <cstdint>
#include <string>

// Calling convention of OpenGL entry points obtained from GetGlProcAddress.
// 32-bit Windows uses stdcall for them; every other host uses the default.
#if defined(_WIN32) && !defined(_WIN64)
#define REXBOX_GL_APIENTRY __stdcall
#else
#define REXBOX_GL_APIENTRY
#endif

namespace rexbox::platform
{

enum class GlApi
{
    kDesktopGl,  // OpenGL 3.3 core profile
    kGlEs,       // OpenGL ES 3.0 (Android, WebGL2)
};

struct DisplayConfig
{
    // The picture's size; the window opens at `scale` times it and keeps its
    // aspect ratio when resized.
    uint32_t width = 640;
    uint32_t height = 480;
    uint32_t scale = 2;
    // Used when SetDisplayTitle gave none.
    const char* title = "";
    GlApi api = GlApi::kDesktopGl;
    bool hidden = false;  // create the window invisible (diagnostics that still need a GL context)
};

struct DisplayInfo
{
    int gl_major = 0;
    int gl_minor = 0;
    bool gl_es = false;
    uint32_t drawable_width = 0;   // in pixels (may differ from the window size)
    uint32_t drawable_height = 0;
};

// Sets the window title's fixed part (the application's name, version and
// build). The window shows it followed by " - <GL api and version> - FPS :
// <presents per second>", updated once a second. May be called before the
// display exists.
void SetDisplayTitle(const std::string& title);

// Creates the window and an OpenGL-family context, current on the calling
// thread. Only one display exists; a second call fails while it is open.
//
// The window is resizable (aspect ratio kept) and has these controls, handled
// in PumpDisplayEvents: Alt+1/2/3 size it to 1x/2x/3x the picture, and a left
// double click or Alt+Enter toggles borderless fullscreen at the desktop
// resolution. Callers fit the picture into the drawable size (GetDisplayInfo),
// which follows the window.
bool CreateDisplay(const DisplayConfig& config, std::string* error);

void DestroyDisplay();

bool DisplayCreated();

// GL entry point by name, or nullptr. Valid only while the display exists.
void* GetGlProcAddress(const char* name);

// Processes pending window events. Sets *close_requested when the user asked
// to close the window (the caller decides what that means).
void PumpDisplayEvents(bool* close_requested);

// Swaps the back buffer to the window (no vertical sync; the caller paces) and
// counts the present for the title's FPS.
void PresentDisplay();

void GetDisplayInfo(DisplayInfo* info);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_DISPLAY_H
