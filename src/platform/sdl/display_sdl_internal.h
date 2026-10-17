// Between the SDL display and the overlay built on it (not a platform
// contract; only src/platform/sdl/ includes this).

#ifndef REXBOX_PLATFORM_SDL_DISPLAY_SDL_INTERNAL_H
#define REXBOX_PLATFORM_SDL_DISPLAY_SDL_INTERNAL_H

#include <SDL3/SDL.h>

namespace rexbox::platform::sdl_internal
{

// display_sdl.cpp: the visible window (nullptr when hidden or absent), its
// GL context, and whether that context is OpenGL ES.
SDL_Window* OverlayWindow();
SDL_GLContext OverlayContext();
bool DisplayIsEs();

// overlay_sdl.cpp: an event for the overlay; true when it was the menu
// toggle (Tab) and must go no further.
bool OverlayProcessEvent(const SDL_Event& event);
// Gamepad shortcuts: LT+RT+Y toggles the menu; true once LT+RT+L3+R3 has
// been held for a second (a quit, reported like closing the window).
bool PollGamepadShortcuts();
// Releases the ImGui context and backends; called with the display's lock
// held, before the GL context goes.
void ShutdownOverlay();

}  // namespace rexbox::platform::sdl_internal

#endif  // REXBOX_PLATFORM_SDL_DISPLAY_SDL_INTERNAL_H
