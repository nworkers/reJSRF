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
#include <array>
#include <atomic>
#include <mutex>

namespace rexbox::platform
{

namespace
{

std::mutex g_mutex;
bool g_initialized = false;
bool g_available = false;

// Rumble per controller (docs/design/20261018-i094-gamepad-vibration.md).
// SDL_RumbleGamepad takes a duration and an Xbox rumble lasts until changed,
// so a running rumble is applied for kRumbleSpanMs and renewed every
// kRumbleRenewMs.
constexpr uint32_t kRumbleControllers = 4;
constexpr uint64_t kRumbleSpanMs = 2000;
constexpr uint64_t kRumbleRenewMs = 1000;

struct Rumble
{
    uint16_t want_left = 0;  // as the title asked
    uint16_t want_right = 0;
    uint16_t left = 0;  // as applied
    uint16_t right = 0;
    uint64_t applied_at = 0;
    uint64_t test_until = 0;  // a test owns the motors until then
};

std::array<Rumble, kRumbleControllers> g_rumble;
std::atomic<uint32_t> g_rumble_percent{100};

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

// The index-th SDL gamepad, or null. Call with g_mutex held.
SDL_Gamepad* GamepadAt(uint32_t index)
{
    if (!g_available)
    {
        return nullptr;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_Gamepad* gamepad = nullptr;
    if (ids != nullptr && static_cast<int>(index) < count)
    {
        // Opening an open gamepad returns it again; SDL keeps it open.
        gamepad = SDL_OpenGamepad(ids[index]);
    }
    SDL_free(ids);
    return gamepad;
}

uint16_t Scale(uint16_t speed, uint32_t percent)
{
    return static_cast<uint16_t>((static_cast<uint32_t>(speed) * percent) / 100u);
}

// Applies the index-th controller's rumble when it changed or is due for
// renewal. Call with g_mutex held.
void UpdateRumble(uint32_t index)
{
    if (index >= kRumbleControllers)
    {
        return;
    }
    Rumble& rumble = g_rumble[index];
    const uint64_t now = SDL_GetTicks();
    if (now < rumble.test_until)
    {
        return;
    }
    const uint32_t percent = OverlayVisible() ? 0u : g_rumble_percent.load();
    const uint16_t left = Scale(rumble.want_left, percent);
    const uint16_t right = Scale(rumble.want_right, percent);
    const bool running = left != 0 || right != 0;
    const bool changed = left != rumble.left || right != rumble.right || rumble.test_until != 0;
    if (!changed && !(running && now - rumble.applied_at >= kRumbleRenewMs))
    {
        return;
    }
    SDL_Gamepad* gamepad = GamepadAt(index);
    if (gamepad == nullptr)
    {
        return;
    }
    SDL_RumbleGamepad(gamepad, left, right, running ? static_cast<Uint32>(kRumbleSpanMs) : 0);
    rumble.left = left;
    rumble.right = right;
    rumble.applied_at = now;
    rumble.test_until = 0;
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
    std::lock_guard<std::mutex> lock(g_mutex);
    // The title reads every frame, which renews a running rumble and stops it
    // while the settings menu is open.
    UpdateRumble(index);
    // The settings menu has the input; the title sees its controller idle.
    if (OverlayVisible())
    {
        snapshot->connected = index == 0;
        return;
    }
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

void SetGamepadRumble(uint32_t index, uint16_t left, uint16_t right)
{
    if (index >= kRumbleControllers)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_rumble[index].want_left = left;
    g_rumble[index].want_right = right;
    UpdateRumble(index);
}

void SetRumbleStrength(uint32_t percent)
{
    g_rumble_percent.store(std::min<uint32_t>(percent, 100));
}

void TestGamepadRumble(uint32_t index, uint32_t milliseconds)
{
    if (index >= kRumbleControllers)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    SDL_Gamepad* gamepad = GamepadAt(index);
    if (gamepad == nullptr)
    {
        return;
    }
    const uint16_t speed = Scale(0xFFFF, g_rumble_percent.load());
    SDL_RumbleGamepad(gamepad, speed, speed, milliseconds);
    // The next update after the test puts the title's rumble back.
    g_rumble[index].test_until = SDL_GetTicks() + milliseconds;
}

}  // namespace rexbox::platform
