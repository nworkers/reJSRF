// Display contract on SDL3: one window with an OpenGL-family context.
//
// SDL3 (zlib) is the host library every target OS shares, so this file lives
// under src/platform/sdl/ and each OS lists it in src/platform/CMakeLists.txt.
// The window is created and its events are pumped on the thread that calls
// CreateDisplay (the guest's main thread). That is valid on Win32 and X11 or
// Wayland; platforms that require the process main thread get a display
// service later (docs/design/20261012-i007-d3d8-hle-first-batch.md section 2.5).

#include "rexbox/platform/display.h"

#include <SDL3/SDL.h>

#include <mutex>

namespace rexbox::platform
{

namespace
{

struct SdlDisplay
{
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    DisplayInfo info;
    bool sdl_video_initialized = false;
};

std::mutex g_mutex;
SdlDisplay g_display;

void SetError(std::string* error, const char* what)
{
    if (error != nullptr)
    {
        const char* detail = SDL_GetError();
        *error = std::string(what) + (detail != nullptr && detail[0] != '\0' ? std::string(": ") + detail : "");
    }
}

void ReleaseLocked()
{
    if (g_display.context != nullptr)
    {
        SDL_GL_DestroyContext(g_display.context);
        g_display.context = nullptr;
    }
    if (g_display.window != nullptr)
    {
        SDL_DestroyWindow(g_display.window);
        g_display.window = nullptr;
    }
    if (g_display.sdl_video_initialized)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        g_display.sdl_video_initialized = false;
    }
    g_display.info = DisplayInfo{};
}

}  // namespace

bool CreateDisplay(const DisplayConfig& config, std::string* error)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_display.window != nullptr)
    {
        if (error != nullptr)
        {
            *error = "a display already exists";
        }
        return false;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        SetError(error, "SDL_InitSubSystem(VIDEO) failed");
        return false;
    }
    g_display.sdl_video_initialized = true;

    if (config.api == GlApi::kGlEs)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    g_display.window = SDL_CreateWindow(config.title, static_cast<int>(config.width), static_cast<int>(config.height),
                                        SDL_WINDOW_OPENGL);
    if (g_display.window == nullptr)
    {
        SetError(error, "SDL_CreateWindow failed");
        ReleaseLocked();
        return false;
    }
    g_display.context = SDL_GL_CreateContext(g_display.window);
    if (g_display.context == nullptr)
    {
        SetError(error, "SDL_GL_CreateContext failed");
        ReleaseLocked();
        return false;
    }
    if (!SDL_GL_MakeCurrent(g_display.window, g_display.context))
    {
        SetError(error, "SDL_GL_MakeCurrent failed");
        ReleaseLocked();
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    int value = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &value);
    g_display.info.gl_major = value;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &value);
    g_display.info.gl_minor = value;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &value);
    g_display.info.gl_es = (value & SDL_GL_CONTEXT_PROFILE_ES) != 0;
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(g_display.window, &width, &height);
    g_display.info.drawable_width = static_cast<uint32_t>(width);
    g_display.info.drawable_height = static_cast<uint32_t>(height);
    return true;
}

void DestroyDisplay()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ReleaseLocked();
}

bool DisplayCreated()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_display.window != nullptr;
}

void* GetGlProcAddress(const char* name)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_display.context == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

void PumpDisplayEvents(bool* close_requested)
{
    bool close = false;
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            close = true;
        }
    }
    if (close_requested != nullptr)
    {
        *close_requested = close;
    }
}

void PresentDisplay()
{
    SDL_Window* window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        window = g_display.window;
    }
    if (window != nullptr)
    {
        SDL_GL_SwapWindow(window);
    }
}

void GetDisplayInfo(DisplayInfo* info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (info != nullptr)
    {
        *info = g_display.info;
    }
}

}  // namespace rexbox::platform
