// The settings menu and the FPS panel drawn with Dear ImGui
// (docs/design/20261017-i084-settings-overlay.md section 2.5).

#ifndef REJSRF_APP_SETTINGS_UI_H
#define REJSRF_APP_SETTINGS_UI_H

#include "settings.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>

namespace rejsrf
{

// What the Info tab shows about this build and the game.
struct BuildInfo
{
    std::string version;
    std::string build_time;
    std::string identity;  // platform and configuration
    std::string commit;
    std::string xbe;  // file and title ID
};

class SettingsUi
{
public:
    SettingsUi(BuildInfo build, Settings settings, SettingsFile* file);

    // The overlay's builder: runs on every present, on the thread that
    // presents.
    void Build();

private:
    // A setting's actual state as last seen, and the value the menu just
    // asked for (some apply only after the next present).
    template <typename T>
    struct Tracked
    {
        T seen{};
        T expected{};
    };

    void ObserveActualState();
    void Save();
    void DrawMenu();
    void DrawInfoTab();
    void DrawGraphicsTab();
    void DrawFpsPanel();

    BuildInfo build_;
    Settings settings_;  // what rejsrf.ini holds
    SettingsFile* file_;
    bool observed_ = false;
    Tracked<uint32_t> render_scale_;
    Tracked<bool> fullscreen_;
    Tracked<bool> widescreen_;
    Tracked<bool> vsync_;

    // Present times over the last 60 s, in seconds since start_.
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    std::deque<double> presents_;
};

}  // namespace rejsrf

#endif  // REJSRF_APP_SETTINGS_UI_H
