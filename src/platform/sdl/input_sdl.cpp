// Input contract on SDL3: host gamepads through SDL's gamepad subsystem, and
// the keyboard as the first controller when no gamepad is present.
//
// SDL updates gamepad and keyboard state when events are pumped; the display
// backend pumps them on the thread that owns the window. Without a window
// (headless runs) ReadGamepad updates the gamepads itself.

#include "rexbox/platform/display.h"
#include "rexbox/platform/input.h"
#include "rexbox/platform/overlay.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <mutex>

namespace rexbox::platform
{

namespace
{

std::mutex g_mutex;
bool g_initialized = false;
bool g_available = false;

uint8_t ButtonAnalog(SDL_Gamepad* gamepad, SDL_GamepadButton button)
{
    return SDL_GetGamepadButton(gamepad, button) ? 255 : 0;
}

uint8_t TriggerAnalog(SDL_Gamepad* gamepad, SDL_GamepadAxis axis)
{
    const int value = std::max<int>(0, SDL_GetGamepadAxis(gamepad, axis));  // 0..32767
    return static_cast<uint8_t>((value * 255) / 32767);
}

int16_t FlipAxis(int16_t value)
{
    // SDL's +Y is down; the contract's is up. -(-32768) does not fit.
    return value == -32768 ? 32767 : static_cast<int16_t>(-value);
}

void ReadSdlGamepad(SDL_Gamepad* gamepad, GamepadSnapshot* out)
{
    out->connected = true;
    out->dpad_up = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    out->dpad_down = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    out->dpad_left = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    out->dpad_right = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    out->start = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    out->back = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK);
    out->left_thumb = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    out->right_thumb = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    out->a = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    out->b = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    out->x = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    out->y = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
    out->black = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    out->white = ButtonAnalog(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    out->left_trigger = TriggerAnalog(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    out->right_trigger = TriggerAnalog(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    out->left_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    out->left_y = FlipAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
    out->right_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    out->right_y = FlipAxis(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
}

// Keyboard fallback for the first controller (docs/kb/xbox-xinput.md).
void ReadKeyboard(GamepadSnapshot* out)
{
    out->connected = true;
    out->from_keyboard = true;
    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    if (keys == nullptr)
    {
        return;
    }
    auto held = [&](SDL_Scancode code) { return static_cast<int>(code) < count && keys[code]; };
    auto analog = [&](SDL_Scancode code) -> uint8_t { return held(code) ? 255 : 0; };
    out->dpad_up = held(SDL_SCANCODE_UP);
    out->dpad_down = held(SDL_SCANCODE_DOWN);
    out->dpad_left = held(SDL_SCANCODE_LEFT);
    out->dpad_right = held(SDL_SCANCODE_RIGHT);
    // Alt+Enter toggles fullscreen (display_sdl.cpp), not START.
    const bool alt = held(SDL_SCANCODE_LALT) || held(SDL_SCANCODE_RALT);
    out->start = held(SDL_SCANCODE_RETURN) && !alt;
    out->back = held(SDL_SCANCODE_BACKSPACE);
    out->a = analog(SDL_SCANCODE_Z);
    out->b = analog(SDL_SCANCODE_X);
    out->x = analog(SDL_SCANCODE_C);
    out->y = analog(SDL_SCANCODE_V);
    out->left_trigger = analog(SDL_SCANCODE_A);
    out->right_trigger = analog(SDL_SCANCODE_S);
    out->black = analog(SDL_SCANCODE_Q);
    out->white = analog(SDL_SCANCODE_W);
    out->left_x = static_cast<int16_t>((out->dpad_right ? 32767 : 0) - (out->dpad_left ? 32767 : 0));
    out->left_y = static_cast<int16_t>((out->dpad_up ? 32767 : 0) - (out->dpad_down ? 32767 : 0));
}

}  // namespace

bool InitializeInput(std::string* error)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized)
    {
        return g_available;
    }
    g_initialized = true;
    g_available = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (!g_available && error != nullptr)
    {
        const char* detail = SDL_GetError();
        *error = std::string("SDL_InitSubSystem(GAMEPAD) failed") +
                 (detail != nullptr && detail[0] != '\0' ? std::string(": ") + detail : "");
    }
    return g_available;
}

void ReadGamepad(uint32_t index, GamepadSnapshot* snapshot)
{
    *snapshot = GamepadSnapshot{};
    // The settings menu has the input; the title sees its controller idle.
    if (OverlayVisible())
    {
        snapshot->connected = index == 0;
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_available)
    {
        if (!DisplayCreated())
        {
            SDL_UpdateGamepads();
        }
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids != nullptr && static_cast<int>(index) < count)
        {
            // Opening an open gamepad returns it again; SDL keeps it open.
            SDL_Gamepad* gamepad = SDL_OpenGamepad(ids[index]);
            if (gamepad != nullptr)
            {
                ReadSdlGamepad(gamepad, snapshot);
            }
        }
        const bool have_pad = ids != nullptr && count > 0;
        SDL_free(ids);
        if (snapshot->connected || have_pad)
        {
            return;
        }
    }
    if (index == 0)
    {
        ReadKeyboard(snapshot);
    }
}

}  // namespace rexbox::platform
