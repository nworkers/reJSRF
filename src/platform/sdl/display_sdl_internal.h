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
// Back+Start held for a second on a gamepad toggles the menu.
void PollOverlayGamepadToggle();
// Releases the ImGui context and backends; called with the display's lock
// held, before the GL context goes.
void ShutdownOverlay();

}  // namespace rexbox::platform::sdl_internal

#endif  // REXBOX_PLATFORM_SDL_DISPLAY_SDL_INTERNAL_H
