// Platform contract: host game controllers, as plain values.
//
// A snapshot is shaped like a gamepad with analog face buttons (the layout an
// Xbox title expects) but carries no runtime or title types. Index 0 falls
// back to the keyboard when no host gamepad is present, so a title always has
// a first controller. The host backend lives in src/platform/<library>/.

#ifndef REXBOX_PLATFORM_INPUT_H
#define REXBOX_PLATFORM_INPUT_H

#include <cstdint>
#include <string>

namespace rexbox::platform
{

struct GamepadSnapshot
{
    bool connected = false;
    bool from_keyboard = false;  // the keyboard fallback produced it

    // Digital controls.
    bool dpad_up = false;
    bool dpad_down = false;
    bool dpad_left = false;
    bool dpad_right = false;
    bool start = false;
    bool back = false;
    bool left_thumb = false;   // stick click
    bool right_thumb = false;

    // Analog controls, 0 (released) to 255 (fully pressed).
    uint8_t a = 0;
    uint8_t b = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t black = 0;
    uint8_t white = 0;
    uint8_t left_trigger = 0;
    uint8_t right_trigger = 0;

    // Sticks, -32768 to 32767; +X right, +Y up.
    int16_t left_x = 0;
    int16_t left_y = 0;
    int16_t right_x = 0;
    int16_t right_y = 0;
};

// Starts the host's game-controller support. Safe to call more than once.
// Returns false (with a reason) when gamepads are unavailable; the keyboard
// fallback still works then.
bool InitializeInput(std::string* error);

// The state of the index-th host controller (0-based). A missing controller
// reads as not connected, except index 0, which falls back to the keyboard.
void ReadGamepad(uint32_t index, GamepadSnapshot* snapshot);

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_INPUT_H
