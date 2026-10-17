// Platform contract: the overlay drawn over the picture (Dear ImGui), such as
// the application's settings screen (docs/design/20261017-i084-settings-overlay.md).
//
// The platform owns the ImGui context and its window and GL backends; the
// application registers a builder that makes ImGui calls (it includes
// imgui.h itself). The overlay draws only in a visible window.

#ifndef REXBOX_PLATFORM_OVERLAY_H
#define REXBOX_PLATFORM_OVERLAY_H

#include <functional>

namespace rexbox::platform
{

// The function that issues the overlay's ImGui calls. It runs on every
// RenderOverlay, whether the menu is visible or not (the caller decides what
// to draw), between ImGui::NewFrame and ImGui::Render.
void SetOverlayBuilder(std::function<void()> builder);

// Draws the overlay into the window's framebuffer, which must be bound, just
// before PresentDisplay, on the thread that owns the GL context. The first
// call creates the ImGui context; GL state is restored afterwards.
void RenderOverlay();

// Whether the menu is open. Tab toggles it in the window, and LT+RT+Y on a
// gamepad (seen by PumpDisplayEvents, which also reports LT+RT+L3+R3 held
// for a second as a close request).
// While it is open the game reads no input (ReadGamepad reports a connected
// controller with nothing pressed) and the input goes to the overlay.
bool OverlayVisible();
void SetOverlayVisible(bool visible);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_OVERLAY_H
