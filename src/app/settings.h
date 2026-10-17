// The application's settings and rejsrf.ini
// (docs/design/20261017-i084-settings-overlay.md section 2.4).

#ifndef REJSRF_APP_SETTINGS_H
#define REJSRF_APP_SETTINGS_H

#include <cstdint>
#include <string>

namespace rejsrf
{

struct Settings
{
    uint32_t render_scale = 1;  // 1..8
    bool fullscreen = false;
    uint32_t window_scale = 2;  // 1..3: the window at 1x-3x the picture (docs/design/20261017-i090-window-size.md)
    bool widescreen = false;
    bool vsync = true;  // adaptive vsync; false: off
    // Anti-aliasing and filtering (docs/design/20261017-i089-anti-aliasing.md).
    // MSAA and SMAA exclude each other; with both set, MSAA wins.
    uint32_t msaa = 0;        // 0 (off), 2, 4, 8, 16
    uint32_t smaa = 0;        // 0 off, 1 1x
    uint32_t anisotropy = 1;  // 1 (off), 2, 4, 8, 16
    // [Input] (docs/design/20261018-i094-gamepad-vibration.md).
    bool vibration = true;
    uint32_t vibration_strength = 100;  // percent, 10..100
    // The stats window's sections (shown while any is on) and where its
    // top-right corner sits, as fractions of the window (negative: the
    // default, top right).
    bool show_fps = false;
    bool show_memory = false;
    bool show_video_memory = false;
    float stats_x = -1.0f;
    float stats_y = -1.0f;
};

// rejsrf.ini next to the executable, or in the user's settings folder when
// that one exists instead or the executable's folder is not writable.
class SettingsFile
{
public:
    SettingsFile();

    // Reads the file if there is one; unknown keys and malformed values keep
    // the defaults.
    Settings Load();

    // Writes every setting (through a temporary file). Falls back to the
    // user's folder once if the executable's folder refuses the write.
    bool Save(const Settings& settings);

    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::string fallback_path_;
};

}  // namespace rejsrf

#endif  // REJSRF_APP_SETTINGS_H
