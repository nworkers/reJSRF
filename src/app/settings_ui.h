// The settings menu and the stats window drawn with Dear ImGui
// (docs/design/20261017-i084-settings-overlay.md section 2.5).

#ifndef REJSRF_APP_SETTINGS_UI_H
#define REJSRF_APP_SETTINGS_UI_H

#include "settings.h"

#include "rexbox/hle/d3d8/d3d8_display.h"
#include "rexbox/platform/system_info.h"

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

    // One finished 0.5 s bin of the stats history. Bins are fixed in time,
    // so a value never changes once drawn.
    struct Sample
    {
        float fps = 0.0f;
        float memory_mib = 0.0f;  // process working set
        float video_mib = 0.0f;   // GPU in use (all programs), or this HLE's resources
    };

    void ObserveActualState();
    void Save();
    void Record(double now);
    void TakeMemorySnapshot();
    void DrawMenu();
    void DrawTab(void (SettingsUi::*draw)());
    void DrawInfoTab();
    void DrawGraphicsTab();
    void DrawStatsWindow();

    BuildInfo build_;
    Settings settings_;  // what rejsrf.ini holds
    SettingsFile* file_;
    bool observed_ = false;
    Tracked<uint32_t> render_scale_;
    Tracked<bool> fullscreen_;
    Tracked<bool> widescreen_;
    Tracked<bool> vsync_;
    float tab_height_ = 0.0f;  // the tallest tab's content, divided by the OSD scale
    bool menu_was_open_ = false;
    int menu_frames_ = 0;  // frames since the menu opened

    // Stats: the bin being filled (index = floor(seconds / 0.5)) and the
    // finished ones, newest last.
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    double last_present_ = -1.0;
    int64_t bin_ = -1;
    int bin_frames_ = 0;
    double bin_time_ = 0.0;
    std::deque<Sample> history_;

    // The latest memory figures, taken with each finished bin.
    rexbox::platform::ProcessMemory process_memory_;
    rexbox::hle::d3d8::VideoMemoryInfo video_memory_;
    bool guest_known_ = false;
    uint64_t guest_used_ = 0;
    uint64_t guest_total_ = 0;

    // The stats window is placed from settings_.stats_x/y every frame except
    // while the user drags it (from a press on it until the release); the
    // drag's place, as fractions, is saved on release.
    bool stats_moved_ = false;
    float stats_drag_x_ = 0.0f;
    float stats_drag_y_ = 0.0f;
};

}  // namespace rejsrf

#endif  // REJSRF_APP_SETTINGS_UI_H
