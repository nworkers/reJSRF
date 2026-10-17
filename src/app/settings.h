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
    bool widescreen = false;
    bool vsync = true;  // adaptive vsync; false: off
    bool show_fps_overlay = false;
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
