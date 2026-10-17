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

#include <atomic>
#include <chrono>
#include <cstdio>
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
    uint32_t picture_width = 0;  // DisplayConfig width/height, the base of the fixed scales
    uint32_t picture_height = 0;

    // The title's FPS: presents counted over about a second.
    std::chrono::steady_clock::time_point fps_period_start;
    uint32_t fps_presents = 0;
};

std::mutex g_mutex;
SdlDisplay g_display;
std::atomic<bool> g_capture_requested{false};
std::atomic<uint32_t> g_render_scale_requested{0};
std::string g_title;  // SetDisplayTitle; outlives the display

void SetError(std::string* error, const char* what)
{
    if (error != nullptr)
    {
        const char* detail = SDL_GetError();
        *error = std::string(what) + (detail != nullptr && detail[0] != '\0' ? std::string(": ") + detail : "");
    }
}

std::string FullTitleLocked(const char* fallback, double frames_per_second)
{
    char suffix[96];
    std::snprintf(suffix, sizeof(suffix), " - %s %d.%d - FPS : %.1f", g_display.info.gl_es ? "OpenGL ES" : "OpenGL",
                  g_display.info.gl_major, g_display.info.gl_minor, frames_per_second);
    return (g_title.empty() ? std::string(fallback != nullptr ? fallback : "") : g_title) + suffix;
}

void UpdateDrawableSizeLocked()
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(g_display.window, &width, &height);
    g_display.info.drawable_width = static_cast<uint32_t>(width);
    g_display.info.drawable_height = static_cast<uint32_t>(height);
}

// The refresh of the monitor the window is on (it can move or go fullscreen).
void UpdateRefreshLocked()
{
    g_display.info.refresh_hz = 0;
    const SDL_DisplayID id = SDL_GetDisplayForWindow(g_display.window);
    const SDL_DisplayMode* mode = id != 0 ? SDL_GetCurrentDisplayMode(id) : nullptr;
    if (mode == nullptr)
    {
        return;
    }
    g_display.info.refresh_hz = mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0
                                    ? static_cast<double>(mode->refresh_rate_numerator) / mode->refresh_rate_denominator
                                    : static_cast<double>(mode->refresh_rate);
}

// Alt+1/2/3: the picture at a fixed scale, leaving fullscreen or a maximized
// window first.
void ApplyScale(SDL_Window* window, uint32_t picture_width, uint32_t picture_height, uint32_t scale)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
    {
        SDL_SetWindowFullscreen(window, false);
    }
    if ((flags & SDL_WINDOW_MAXIMIZED) != 0)
    {
        SDL_RestoreWindow(window);
    }
    SDL_SetWindowSize(window, static_cast<int>(picture_width * scale), static_cast<int>(picture_height * scale));
    std::fprintf(stderr, "[display] window %ux%u (%ux)\n", picture_width * scale, picture_height * scale, scale);
}

// Double click or Alt+Enter. No fullscreen mode is set on the window, so SDL
// makes it borderless at the desktop resolution; the display mode never
// changes.
void ToggleFullscreen(SDL_Window* window)
{
    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
    if (!SDL_SetWindowFullscreen(window, fullscreen))
    {
        std::fprintf(stderr, "[display] fullscreen toggle failed: %s\n", SDL_GetError());
        return;
    }
    std::fprintf(stderr, "[display] %s\n", fullscreen ? "fullscreen (desktop resolution)" : "windowed");
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

    const uint32_t scale = config.scale != 0 ? config.scale : 1;
    // The title's GL part is filled in once the context exists.
    g_display.window =
        SDL_CreateWindow(g_title.empty() ? config.title : g_title.c_str(), static_cast<int>(config.width * scale),
                         static_cast<int>(config.height * scale),
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (config.hidden ? SDL_WINDOW_HIDDEN : 0));
    if (g_display.window == nullptr)
    {
        SetError(error, "SDL_CreateWindow failed");
        ReleaseLocked();
        return false;
    }
    g_display.picture_width = config.width;
    g_display.picture_height = config.height;
    if (config.width != 0 && config.height != 0)
    {
        const float aspect = static_cast<float>(config.width) / static_cast<float>(config.height);
        SDL_SetWindowAspectRatio(g_display.window, aspect, aspect);
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
    // The title paces itself by the emulated vertical blank (the HLE's
    // virtual clock) whatever the monitor's refresh; host vsync only keeps
    // swaps out of the scan-out (no tearing). A hidden window swaps at once:
    // waiting for its blank can block indefinitely on some drivers
    // (docs/design/20261017-i082-vsync.md).
    int interval = 0;
    if (!config.hidden && config.vsync == VsyncMode::kAdaptive && SDL_GL_SetSwapInterval(-1))
    {
        interval = -1;
    }
    else if (!config.hidden && config.vsync != VsyncMode::kOff && SDL_GL_SetSwapInterval(1))
    {
        interval = 1;
    }
    else
    {
        SDL_GL_SetSwapInterval(0);
    }
    g_display.info.swap_interval = interval;

    int value = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &value);
    g_display.info.gl_major = value;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &value);
    g_display.info.gl_minor = value;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &value);
    g_display.info.gl_es = (value & SDL_GL_CONTEXT_PROFILE_ES) != 0;
    UpdateDrawableSizeLocked();
    UpdateRefreshLocked();
    SDL_SetWindowTitle(g_display.window, FullTitleLocked(config.title, 0.0).c_str());
    g_display.fps_presents = 0;
    return true;
}

void SetDisplayTitle(const std::string& title)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_title = title;
    if (g_display.window != nullptr)
    {
        SDL_SetWindowTitle(g_display.window, FullTitleLocked(nullptr, 0.0).c_str());
    }
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
    SDL_Window* window = nullptr;
    uint32_t picture_width = 0;
    uint32_t picture_height = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        window = g_display.window;
        picture_width = g_display.picture_width;
        picture_height = g_display.picture_height;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            close = true;
        }
        if (window == nullptr)
        {
            continue;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && (event.key.mod & SDL_KMOD_ALT) != 0)
        {
            switch (event.key.key)
            {
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
                ApplyScale(window, picture_width, picture_height, 1 + (event.key.key - SDLK_1));
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                ToggleFullscreen(window);
                break;
            default:
                break;
            }
        }
        else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && (event.key.mod & SDL_KMOD_CTRL) != 0 &&
                 event.key.key >= SDLK_1 && event.key.key <= SDLK_8)
        {
            g_render_scale_requested.store(1 + static_cast<uint32_t>(event.key.key - SDLK_1));
        }
        else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_F9)
        {
            g_capture_requested.store(true);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
                 event.button.clicks == 2)
        {
            ToggleFullscreen(window);
        }
        else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_RESIZED ||
                 event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            UpdateDrawableSizeLocked();
            UpdateRefreshLocked();
        }
    }
    if (close_requested != nullptr)
    {
        *close_requested = close;
    }
}

bool ConsumeCaptureRequest()
{
    return g_capture_requested.exchange(false);
}

uint32_t ConsumeRenderScaleRequest()
{
    return g_render_scale_requested.exchange(0);
}

void PresentDisplay()
{
    SDL_Window* window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        window = g_display.window;
    }
    if (window == nullptr)
    {
        return;
    }
    SDL_GL_SwapWindow(window);

    std::lock_guard<std::mutex> lock(g_mutex);
    const auto now = std::chrono::steady_clock::now();
    if (g_display.fps_presents == 0)
    {
        g_display.fps_period_start = now;
    }
    ++g_display.fps_presents;
    const double seconds = std::chrono::duration<double>(now - g_display.fps_period_start).count();
    if (seconds >= 1.0)
    {
        // Intervals between presents, so the first one only starts the period.
        const double frames_per_second = static_cast<double>(g_display.fps_presents - 1) / seconds;
        SDL_SetWindowTitle(g_display.window, FullTitleLocked(nullptr, frames_per_second).c_str());
        g_display.fps_period_start = now;
        g_display.fps_presents = 1;
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
