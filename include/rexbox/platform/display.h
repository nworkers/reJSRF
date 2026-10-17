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

// Host vertical sync for swaps (docs/design/20261017-i082-vsync.md).
enum class VsyncMode
{
    kOff,       // swap at once (swap interval 0)
    kOn,        // wait for the vertical blank (1)
    kAdaptive,  // wait, but swap at once when late (-1); kOn where unsupported
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
    // A hidden window always swaps at once: waiting for its blank can block
    // on some drivers.
    VsyncMode vsync = VsyncMode::kAdaptive;
    bool fullscreen = false;  // open borderless fullscreen (ignored for a hidden window)
};

struct DisplayInfo
{
    int gl_major = 0;
    int gl_minor = 0;
    bool gl_es = false;
    uint32_t drawable_width = 0;   // in pixels (may differ from the window size)
    uint32_t drawable_height = 0;
    int swap_interval = 0;  // as applied: 0 (no vsync), 1 (vsync), -1 (adaptive)
    double refresh_hz = 0;  // the refresh of the monitor showing the window; 0 if unknown
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

// True once after the user pressed F9 (a diagnostic capture request; the
// caller decides what to capture). Seen by PumpDisplayEvents.
bool ConsumeCaptureRequest();

// Borderless fullscreen at the desktop resolution, as the double click and
// Alt+Enter toggle it. Call on the thread that pumps events.
void SetFullscreen(bool fullscreen);
bool IsFullscreen();

// Changes host vsync while running (a hidden window keeps swapping at once).
// Call on the thread that owns the GL context; DisplayInfo.swap_interval
// reports the result.
void SetVsync(VsyncMode mode);

// The picture's shape: the window keeps this aspect ratio when resized, and
// Alt+1/2/3 size it to 1x-3x this size (854x480 for a 16:9 picture shown
// from a 640x480 one).
void SetPictureAspect(uint32_t width, uint32_t height);

// The render scale the user asked for with Ctrl+1..8 since the last call
// (1..8), or 0. Seen by PumpDisplayEvents; the caller applies it.
uint32_t ConsumeRenderScaleRequest();

// Swaps the back buffer to the window (no vertical sync; the caller paces) and
// counts the present for the title's FPS.
void PresentDisplay();

void GetDisplayInfo(DisplayInfo* info);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_DISPLAY_H
