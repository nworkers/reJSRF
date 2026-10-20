#include "settings_ui.h"

#include "rexbox/hle/d3d8/d3d8_display.h"
#include "rexbox/platform/display.h"
#include "rexbox/platform/input.h"
#include "rexbox/platform/overlay.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace rejsrf
{

namespace
{

// JSRF's back buffer; the render scale multiplies it.
constexpr uint32_t kGuestWidth = 640;
constexpr uint32_t kGuestHeight = 480;
constexpr uint32_t kMaxScale = 8;

// The stats history: the last 60 s in 0.5 s bins fixed in time.
constexpr double kBinSeconds = 0.5;
constexpr size_t kBins = 120;
constexpr float kFpsAxisMax = 70.0f;
constexpr double kMib = 1024.0 * 1024.0;

// A setting changed elsewhere (a hotkey, a double click) follows the actual
// state into the file; the menu's own change, arriving late, does not.
template <typename T>
bool Follow(T actual, T* seen, T* expected, T* stored)
{
    if (actual == *seen)
    {
        return false;
    }
    *seen = actual;
    if (actual == *expected)
    {
        return false;
    }
    *expected = actual;
    *stored = actual;
    return true;
}

void Row(const char* label, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
}

bool BeginInfoTable(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

std::string Mib(uint64_t bytes)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f MiB", static_cast<double>(bytes) / kMib);
    return buffer;
}

std::string Printf(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

// A memory axis: the next "round" value at least 25% above the peak.
float MemoryAxisMax(float peak)
{
    const float target = std::max(peak * 1.25f, 16.0f);
    float step = 16.0f;
    while (step * 8.0f < target)
    {
        step *= 2.0f;
    }
    return std::ceil(target / step) * step;
}

struct Tick
{
    float value;
    std::string label;
    ImU32 color;
    float thickness;
};

// One stats section's graph: the history right-aligned (newest at the right
// edge), ticks across with their labels to the right.
void DrawGraph(const std::vector<float>& values, float axis_max, const std::vector<Tick>& ticks, ImU32 line_color,
               float label_width)
{
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float graph_width = 240.0f * scale;
    const float graph_height = 64.0f * scale;
    const float pad = ImGui::GetFontSize() * 0.5f;  // room for the top and bottom labels
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(graph_width + label_width, graph_height + pad * 2.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float top = origin.y + pad;
    const auto y_of = [&](float value) {
        return top + graph_height * (1.0f - std::clamp(value, 0.0f, axis_max) / axis_max);
    };
    draw->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + graph_width, top + graph_height),
                        IM_COL32(0, 0, 0, 110));
    for (const Tick& tick : ticks)
    {
        const float y = y_of(tick.value);
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + graph_width, y), tick.color, tick.thickness);
        draw->AddText(ImVec2(origin.x + graph_width + 6.0f * scale, y - ImGui::GetFontSize() * 0.5f), tick.color,
                      tick.label.c_str());
    }
    std::vector<ImVec2> points;
    const size_t first_slot = kBins - values.size();
    for (size_t i = 0; i < values.size(); ++i)
    {
        const float x = origin.x + graph_width * (static_cast<float>(first_slot + i) + 0.5f) / kBins;
        points.push_back(ImVec2(x, y_of(values[i])));
    }
    if (points.size() >= 2)
    {
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), line_color, 1.5f * scale);
    }
}

}  // namespace

SettingsUi::SettingsUi(BuildInfo build, Settings settings, Language running_language, SettingsFile* file)
    : build_(std::move(build)), settings_(settings), file_(file), running_language_(running_language)
{
}

void SettingsUi::Save()
{
    if (file_ != nullptr)
    {
        file_->Save(settings_);
    }
}

void SettingsUi::ObserveActualState()
{
    rexbox::platform::DisplayInfo info;
    rexbox::platform::GetDisplayInfo(&info);
    const uint32_t scale = rexbox::hle::d3d8::RenderScale();
    const bool fullscreen = rexbox::platform::IsFullscreen();
    const uint32_t window_scale = rexbox::platform::WindowScale();
    const uint32_t aspect = static_cast<uint32_t>(rexbox::hle::d3d8::Aspect());
    const bool vsync = info.swap_interval != 0;
    if (!observed_)
    {
        // The state at start (environment variables may have overridden the
        // file) is not written until something changes.
        observed_ = true;
        render_scale_ = {scale, scale};
        fullscreen_ = {fullscreen, fullscreen};
        window_scale_ = {window_scale, window_scale};
        aspect_ = {aspect, aspect};
        vsync_ = {vsync, vsync};
        return;
    }
    bool changed = Follow(scale, &render_scale_.seen, &render_scale_.expected, &settings_.render_scale);
    changed |= Follow(fullscreen, &fullscreen_.seen, &fullscreen_.expected, &settings_.fullscreen);
    // A window resized by hand (0) keeps the last preset for the next start.
    if (window_scale != 0)
    {
        changed |= Follow(window_scale, &window_scale_.seen, &window_scale_.expected, &settings_.window_scale);
    }
    changed |= Follow(aspect, &aspect_.seen, &aspect_.expected, &settings_.aspect);
    changed |= Follow(vsync, &vsync_.seen, &vsync_.expected, &settings_.vsync);
    if (changed)
    {
        Save();
    }
}

void SettingsUi::TakeMemorySnapshot()
{
    process_memory_ = rexbox::platform::QueryProcessMemory();
    video_memory_ = rexbox::hle::d3d8::GetVideoMemoryInfo();
    const rexbox::runtime::GuestHeap* heap = rexbox::runtime::Env().heap;
    guest_known_ = heap != nullptr;
    if (guest_known_)
    {
        guest_total_ = rexbox::runtime::GuestMemory::kAddressSpaceSize;
        guest_used_ = guest_total_ - heap->free_bytes();
    }
}

// FPS per bin comes from the frame intervals ending in it (counting frames
// would jitter by one), and each bin is fixed once its 0.5 s are over, so
// the graph only scrolls; it never redraws the past.
void SettingsUi::Record(double now)
{
    const int64_t bin = static_cast<int64_t>(std::floor(now / kBinSeconds));
    if (bin != bin_)
    {
        if (bin_ >= 0)
        {
            TakeMemorySnapshot();
            Sample sample;
            sample.fps = bin_time_ > 0.0 ? static_cast<float>(bin_frames_ / bin_time_) : 0.0f;
            sample.memory_mib = static_cast<float>(process_memory_.working_set / kMib);
            const rexbox::hle::d3d8::VideoMemoryInfo& video = video_memory_;
            sample.video_mib = static_cast<float>(
                (video.driver_total_known ? video.driver_total - video.driver_free
                                          : video.render_target_bytes + video.texture_bytes) /
                kMib);
            history_.push_back(sample);
            // Bins with no present (a stall) count as 0 FPS.
            for (int64_t missing = bin_ + 1; missing < bin && history_.size() < 2 * kBins; ++missing)
            {
                Sample stalled = sample;
                stalled.fps = 0.0f;
                history_.push_back(stalled);
            }
            while (history_.size() > kBins)
            {
                history_.pop_front();
            }
        }
        bin_ = bin;
        bin_frames_ = 0;
        bin_time_ = 0.0;
    }
    if (last_present_ >= 0.0)
    {
        ++bin_frames_;
        bin_time_ += now - last_present_;
    }
    last_present_ = now;
}

void SettingsUi::Build()
{
    Record(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count());
    ObserveActualState();
    const bool menu = rexbox::platform::OverlayVisible();
    if (menu && !menu_was_open_)
    {
        // Measured afresh each time it opens (see DrawTab).
        tab_height_ = 0.0f;
        menu_frames_ = 0;
    }
    menu_was_open_ = menu;
    if (menu)
    {
        DrawMenu();
        ++menu_frames_;
    }
    DrawStatsWindow();
}

void SettingsUi::DrawMenu()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // The menu keeps its width in proportion to the window, centered (the
    // platform scales the style with the window); its height fits the tab.
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const float width = 560.0f * ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
    bool open = true;
    if (ImGui::Begin("reJSRF Settings", &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Tab (LT+RT+Y on a gamepad) closes this menu. The game keeps running without input.");
        ImGui::PopStyleColor();
        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("Info"))
            {
                DrawTab(&SettingsUi::DrawInfoTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Game"))
            {
                DrawTab(&SettingsUi::DrawGameTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Graphics"))
            {
                DrawTab(&SettingsUi::DrawGraphicsTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Input"))
            {
                DrawTab(&SettingsUi::DrawInputTab);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open)
    {
        rexbox::platform::SetOverlayVisible(false);
    }
}

// Every tab is as tall as the tallest seen since the menu opened, so the
// menu keeps one size whichever tab is open. Heights are kept divided by the
// OSD scale, so they still fit after the window changes size. Info, the
// tallest, is the tab shown first. A window's first frames are laid out
// before it has its width (wrapped text then runs very tall), so they are
// not measured, nor is any frame narrower than the menu's width.
void SettingsUi::DrawTab(void (SettingsUi::*draw)())
{
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float top = ImGui::GetCursorPosY();
    (this->*draw)();
    const float height = ImGui::GetCursorPosY() - top;
    if (menu_frames_ >= 2 && ImGui::GetWindowWidth() >= 560.0f * scale - 1.0f)
    {
        tab_height_ = std::max(tab_height_, height / scale);
    }
    if (height < tab_height_ * scale)
    {
        ImGui::Dummy(ImVec2(0.0f, tab_height_ * scale - height));
    }
}

void SettingsUi::DrawInfoTab()
{
    rexbox::platform::DisplayInfo info;
    rexbox::platform::GetDisplayInfo(&info);
    const rexbox::hle::d3d8::RendererInfo renderer = rexbox::hle::d3d8::GetRendererInfo();
    const uint32_t scale = rexbox::hle::d3d8::RenderScale();
    if (BeginInfoTable("info"))
    {
        Row("reJSRF", "v" + build_.version);
        Row("Build", build_.build_time);
        Row("Configuration", build_.identity);
        Row("Commit", build_.commit);
        Row("Game", build_.xbe);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Driver");
    if (BeginInfoTable("driver"))
    {
        Row("Vendor", renderer.vendor);
        Row("Renderer", renderer.renderer);
        Row(info.gl_es ? "OpenGL ES" : "OpenGL", renderer.version);
        Row("GLSL", renderer.shading_language);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Display");
    if (BeginInfoTable("display"))
    {
        Row("Monitor", info.refresh_hz > 0 ? Printf("%.2f Hz", info.refresh_hz) : std::string());
        Row("Swap interval", Printf("%d (%s)", info.swap_interval,
                                    info.swap_interval < 0   ? "adaptive vsync"
                                    : info.swap_interval > 0 ? "vsync"
                                                             : "no vsync"));
        Row("Window", Printf("%ux%u", info.drawable_width, info.drawable_height));
        Row("Render size", Printf("%ux%u (%ux)", kGuestWidth * scale, kGuestHeight * scale, scale));
        Row("Settings file", file_ != nullptr ? file_->path() : std::string());
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Memory");
    if (BeginInfoTable("memory"))
    {
        const rexbox::platform::ProcessMemory& process = process_memory_;
        Row("Process", process.known ? Mib(process.working_set) + " in use, " + Mib(process.private_bytes) +
                                           " committed"
                                     : std::string());
        Row("Address space", process.address_space_total != 0
                                 ? Mib(process.address_space_used) + " of " + Mib(process.address_space_total)
                                 : std::string());
        Row("Xbox memory", guest_known_ ? Mib(guest_used_) + " of " + Mib(guest_total_) + " allocated" : std::string());
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Video memory");
    if (BeginInfoTable("vram"))
    {
        const rexbox::hle::d3d8::VideoMemoryInfo& video = video_memory_;
        std::string gpu = "not reported by this driver";
        if (video.driver_total_known)
        {
            gpu = Mib(video.driver_total - video.driver_free) + " of " + Mib(video.driver_total) +
                  " in use (all programs)";
        }
        else if (video.driver_free_known)
        {
            gpu = Mib(video.driver_free) + " free";
        }
        Row("GPU", gpu);
        Row("Render targets", Printf("%s (%u surfaces)", Mib(video.render_target_bytes).c_str(), video.render_targets));
        Row("Textures", Printf("%s (%u)", Mib(video.texture_bytes).c_str(), video.textures));
        ImGui::EndTable();
    }
}

void SettingsUi::DrawGraphicsTab()
{
    // Window size: the Alt+1/2/3 presets (docs/design/20261017-i090-window-size.md).
    {
        uint32_t picture_width = 0;
        uint32_t picture_height = 0;
        rexbox::platform::GetPictureSize(&picture_width, &picture_height);
        const uint32_t current = rexbox::platform::WindowScale();
        const auto label = [&](uint32_t scale) {
            return Printf("%ux (%ux%u)", scale, picture_width * scale, picture_height * scale);
        };
        const std::string preview = current != 0       ? label(current)
                                    : fullscreen_.seen ? std::string("Fullscreen")
                                                       : std::string("Custom");
        if (ImGui::BeginCombo("Window Size", preview.c_str()))
        {
            for (uint32_t scale = 1; scale <= rexbox::platform::kMaxWindowScale; ++scale)
            {
                const bool selected = scale == current;
                if (ImGui::Selectable(label(scale).c_str(), selected) && !selected)
                {
                    settings_.window_scale = scale;
                    window_scale_.expected = scale;
                    // Leaving fullscreen is part of the preset.
                    settings_.fullscreen = false;
                    fullscreen_.expected = false;
                    rexbox::platform::SetWindowScale(scale);
                    Save();
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        ImGui::SetItemTooltip("The window at 1x-3x the picture (Alt+1/2/3 in the window).\n"
                              "The last preset is the window's size at the next start.");
    }

    // Rendering resolution.
    static std::vector<std::string> labels;
    if (labels.empty())
    {
        for (uint32_t scale = 1; scale <= kMaxScale; ++scale)
        {
            labels.push_back(Printf("%ux (%ux%u)", scale, kGuestWidth * scale, kGuestHeight * scale));
        }
    }
    const uint32_t current = std::clamp<uint32_t>(render_scale_.expected, 1, kMaxScale);
    if (ImGui::BeginCombo("Rendering Resolution", labels[current - 1].c_str()))
    {
        for (uint32_t scale = 1; scale <= kMaxScale; ++scale)
        {
            const bool selected = scale == current;
            if (ImGui::Selectable(labels[scale - 1].c_str(), selected) && !selected)
            {
                settings_.render_scale = scale;
                render_scale_.expected = scale;
                rexbox::hle::d3d8::SetRenderScale(scale);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (std::getenv("REXBOX_RENDER_SCALE") != nullptr)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(REXBOX_RENDER_SCALE set at start)");
    }

    bool fullscreen = fullscreen_.expected;
    if (ImGui::Checkbox("Fullscreen", &fullscreen))
    {
        settings_.fullscreen = fullscreen;
        fullscreen_.expected = fullscreen;
        rexbox::platform::SetFullscreen(fullscreen);
        Save();
    }

    // Hor+ widescreen (docs/design/20261018-i085-widescreen-horplus.md).
    static const char* const kAspectNames[] = {"4:3", "16:9", "21:9"};
    const uint32_t current_aspect = std::min<uint32_t>(aspect_.expected, 2);
    if (ImGui::BeginCombo("Aspect ratio", kAspectNames[current_aspect]))
    {
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (ImGui::Selectable(kAspectNames[i], i == current_aspect) && i != current_aspect)
            {
                settings_.aspect = i;
                aspect_.expected = i;
                rexbox::hle::d3d8::SetAspect(static_cast<rexbox::hle::d3d8::AspectMode>(i));
                Save();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("16:9 and 21:9 show more of the scene at the sides (Hor+): everything the 4:3\n"
                          "picture shows stays where it is, and the HUD and 2D screens stay centered.\n"
                          "Objects near the edges may appear a little late (the game culls for 4:3).");

    bool vsync = vsync_.expected;
    if (ImGui::Checkbox("VSync", &vsync))
    {
        settings_.vsync = vsync;
        vsync_.expected = vsync;
        rexbox::platform::SetVsync(vsync ? rexbox::platform::VsyncMode::kAdaptive : rexbox::platform::VsyncMode::kOff);
        Save();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Swaps on the monitor's vertical blank (adaptive), so the picture does not tear.\n"
                          "The game's speed stays 60 frames per second either way.");
    if (std::getenv("REXBOX_VSYNC") != nullptr)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(REXBOX_VSYNC set at start)");
    }

    // docs/design/20261018-i095-vblank-wait.md
    bool vblank_wait = settings_.vblank_wait;
    if (ImGui::Checkbox("VBlank wait", &vblank_wait))
    {
        settings_.vblank_wait = vblank_wait;
        rexbox::hle::d3d8::SetVerticalBlankWaitEnabled(vblank_wait);
        Save();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("On: the game is paced by the Xbox's 60 Hz vertical blank, as on the console.\n"
                          "Off: it runs as fast as it can draw, so game speed rises with the frame rate\n"
                          "(VSync still holds it to the monitor). For benchmarks.");

    DrawAntiAliasing();

    ImGui::SeparatorText("Stats window");
    bool changed = ImGui::Checkbox("FPS", &settings_.show_fps);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Memory", &settings_.show_memory);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Video memory", &settings_.show_video_memory);
    if (changed)
    {
        Save();
    }
    ImGui::TextDisabled("The checked sections share one window over the game; drag it by its title bar.");

    ImGui::Spacing();
    ImGui::TextDisabled("Shortcuts: Ctrl+1..8 resolution, Alt+Enter or double click fullscreen,\n"
                        "Alt+1/2/3 window size, F9 diagnostic capture.\n"
                        "Gamepad: LT+RT+Y menu, LT+RT+L3+R3 held 1 s quits.");
}

// Anti-aliasing (MSAA or SMAA, never both) and anisotropic filtering
// (docs/design/20261017-i089-anti-aliasing.md). Values over the driver's
// limits cannot be picked.
void SettingsUi::DrawAntiAliasing()
{
    namespace d3d8 = rexbox::hle::d3d8;
    ImGui::SeparatorText("Image quality");

    // One list: Off, MSAA 2x..16x, SMAA 1x. The menu's own value, since an
    // MSAA change applies only after the next present.
    static const uint32_t kMsaa[] = {2, 4, 8, 16};
    const std::string preview = settings_.msaa != 0   ? Printf("MSAA %ux", settings_.msaa)
                                : settings_.smaa != 0 ? std::string("SMAA 1x")
                                                      : std::string("Off");
    if (ImGui::BeginCombo("Anti-aliasing", preview.c_str()))
    {
        // {msaa, smaa} per entry.
        const auto choose = [&](const char* name, uint32_t msaa, uint32_t smaa, bool enabled) {
            const bool selected = settings_.msaa == msaa && settings_.smaa == smaa;
            ImGui::BeginDisabled(!enabled);
            if (ImGui::Selectable(name, selected) && !selected)
            {
                settings_.msaa = msaa;
                settings_.smaa = smaa;
                d3d8::SetMsaaSamples(msaa);
                d3d8::SetSmaa(smaa != 0 ? d3d8::SmaaMode::k1x : d3d8::SmaaMode::kOff);
                Save();
            }
            ImGui::EndDisabled();
        };
        choose("Off", 0, 0, true);
        for (const uint32_t samples : kMsaa)
        {
            choose(Printf("MSAA %ux", samples).c_str(), samples, 0, samples <= d3d8::MaxMsaaSamples());
        }
        choose("SMAA 1x", 0, 1, true);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("MSAA: multisampling on every render target (up to %ux on this GPU); it costs\n"
                          "video memory, multiplied by the rendering resolution.\n"
                          "SMAA: post-process anti-aliasing (Jimenez et al.) on the final picture,\n"
                          "finding edges by color. Lighter than MSAA, slightly softer.",
                          d3d8::MaxMsaaSamples());

    uint32_t anisotropy = d3d8::Anisotropy();
    if (ImGui::BeginCombo("Anisotropic filtering", anisotropy <= 1 ? "Off" : Printf("%ux", anisotropy).c_str()))
    {
        static const uint32_t kRatios[] = {1, 2, 4, 8, 16};
        for (const uint32_t ratio : kRatios)
        {
            const std::string name = ratio == 1 ? std::string("Off") : Printf("%ux", ratio);
            ImGui::BeginDisabled(ratio > d3d8::MaxAnisotropy() && ratio != 1);
            if (ImGui::Selectable(name.c_str(), ratio == anisotropy) && ratio != anisotropy)
            {
                settings_.anisotropy = ratio;
                d3d8::SetAnisotropy(ratio);
                Save();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Sharper textures at grazing angles (up to %ux on this GPU).", d3d8::MaxAnisotropy());
}

// Vibration (docs/design/20261018-i094-gamepad-vibration.md). The platform
// gets the strength, or 0 with vibration off.
void SettingsUi::DrawInputTab()
{
    ImGui::SeparatorText("Vibration");
    const auto apply = [this]() {
        rexbox::platform::SetRumbleStrength(settings_.vibration ? settings_.vibration_strength : 0);
        Save();
    };
    if (ImGui::Checkbox("Vibration", &settings_.vibration))
    {
        apply();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Gamepad rumble as the game asks for it. It pauses while this menu is open.");

    ImGui::BeginDisabled(!settings_.vibration);
    int strength = static_cast<int>(settings_.vibration_strength);
    ImGui::SliderInt("Strength", &strength, 10, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemDeactivatedAfterEdit() && static_cast<uint32_t>(strength) != settings_.vibration_strength)
    {
        settings_.vibration_strength = static_cast<uint32_t>(strength);
        apply();
    }
    else if (ImGui::IsItemActive())
    {
        // Follow the slider while it is dragged; save once on release.
        rexbox::platform::SetRumbleStrength(static_cast<uint32_t>(strength));
    }
    if (ImGui::Button("Test"))
    {
        rexbox::platform::TestGamepadRumble(0, 500);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Rumbles the first controller for half a second.");
    ImGui::EndDisabled();
}

// docs/design/20261020-i104-language-option.md: the game reads the language
// while it builds objects and loads assets, so a change applies from the next
// start.
void SettingsUi::DrawGameTab()
{
    ImGui::SeparatorText("Language");
    if (ImGui::BeginCombo("Language", LanguageName(settings_.language)))
    {
        for (uint32_t index = 0; index < kLanguageCount; ++index)
        {
            const auto language = static_cast<Language>(index);
            const bool selected = language == settings_.language;
            if (ImGui::Selectable(LanguageName(language), selected) && !selected)
            {
                settings_.language = language;
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Text, voice and pictures in the game's own menus and scenes.\n"
                          "Every language comes from the same game disc.");
    if (std::getenv("REJSRF_LANGUAGE") != nullptr)
    {
        ImGui::TextDisabled("REJSRF_LANGUAGE set at start: running in %s.", LanguageName(running_language_));
    }
    else if (settings_.language != running_language_)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Restart to apply. Running in %s.",
                           LanguageName(running_language_));
    }
    else
    {
        ImGui::TextDisabled("Applies from the next start.");
    }
}

void SettingsUi::DrawStatsWindow()
{
    if (!settings_.show_fps && !settings_.show_memory && !settings_.show_video_memory)
    {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleMain;
    // Anchored every frame by its top-right corner, as fractions of the game
    // window, so it keeps its place in proportion to the window and grows
    // leftward and down as sections are added. While the user drags it,
    // ImGui moves it and the fractions follow.
    if (!stats_moved_)
    {
        const float margin = 10.0f * scale;
        const float x = settings_.stats_x >= 0.0f ? settings_.stats_x * viewport->WorkSize.x
                                                  : viewport->WorkSize.x - margin;
        const float y = settings_.stats_y >= 0.0f ? settings_.stats_y * viewport->WorkSize.y : margin;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + x, viewport->WorkPos.y + y), ImGuiCond_Always,
                                ImVec2(1.0f, 0.0f));
    }
    ImGui::SetNextWindowBgAlpha(0.7f);
    bool open = true;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("Stats", &open, flags))
    {
        // One label column for every section, so the graphs line up.
        const float label_width = ImGui::CalcTextSize("30  half speed").x + 10.0f * scale;
        const size_t count = history_.size();
        const auto latest = [&](float Sample::*field) {
            // The last two bins (one second), so the number moves calmly.
            if (count == 0)
            {
                return 0.0f;
            }
            if (count == 1)
            {
                return history_.back().*field;
            }
            return (history_[count - 1].*field + history_[count - 2].*field) * 0.5f;
        };
        const auto series = [&](float Sample::*field) {
            std::vector<float> values;
            values.reserve(count);
            for (const Sample& sample : history_)
            {
                values.push_back(sample.*field);
            }
            return values;
        };
        const auto peak = [&](float Sample::*field) {
            float result = 0.0f;
            for (const Sample& sample : history_)
            {
                result = std::max(result, sample.*field);
            }
            return result;
        };
        const ImU32 grey = IM_COL32(160, 160, 160, 200);
        bool first = true;
        const auto separate = [&]() {
            if (!first)
            {
                ImGui::Separator();
            }
            first = false;
        };

        if (settings_.show_fps)
        {
            separate();
            ImGui::Text("FPS %.1f", latest(&Sample::fps));
            ImGui::SameLine();
            ImGui::TextDisabled("last 60 s");
            DrawGraph(series(&Sample::fps), kFpsAxisMax,
                      {{60.0f, "60  full speed", IM_COL32(90, 220, 110, 255), 1.5f},
                       {30.0f, "30  half speed", grey, 1.0f},
                       {0.0f, "0  stopped", grey, 1.0f}},
                      IM_COL32(255, 210, 80, 255), label_width);
        }
        if (settings_.show_memory)
        {
            separate();
            const rexbox::platform::ProcessMemory& process = process_memory_;
            ImGui::Text("Memory %.0f MiB", latest(&Sample::memory_mib));
            ImGui::SameLine();
            if (process.address_space_total != 0)
            {
                ImGui::TextDisabled("address space %.0f / %.0f MiB", process.address_space_used / kMib,
                                    process.address_space_total / kMib);
            }
            const float axis = MemoryAxisMax(peak(&Sample::memory_mib));
            DrawGraph(series(&Sample::memory_mib), axis,
                      {{axis, Printf("%.0f MiB", axis), grey, 1.0f},
                       {axis * 0.5f, Printf("%.0f MiB", axis * 0.5f), grey, 1.0f},
                       {0.0f, "0", grey, 1.0f}},
                      IM_COL32(110, 180, 255, 255), label_width);
        }
        if (settings_.show_video_memory)
        {
            separate();
            const rexbox::hle::d3d8::VideoMemoryInfo& video = video_memory_;
            ImGui::Text("Video memory %.0f MiB", latest(&Sample::video_mib));
            ImGui::SameLine();
            if (video.driver_total_known)
            {
                ImGui::TextDisabled("of %.0f MiB (all programs)", video.driver_total / kMib);
            }
            else
            {
                ImGui::TextDisabled("reJSRF's own (driver does not tell)");
            }
            // With the card's size known, the axis is the card.
            const float axis = video.driver_total_known ? static_cast<float>(video.driver_total / kMib)
                                                        : MemoryAxisMax(peak(&Sample::video_mib));
            DrawGraph(series(&Sample::video_mib), axis,
                      {{axis, Printf("%.0f MiB", axis), grey, 1.0f},
                       {axis * 0.5f, Printf("%.0f MiB", axis * 0.5f), grey, 1.0f},
                       {0.0f, "0", grey, 1.0f}},
                      IM_COL32(220, 130, 255, 255), label_width);
        }

        // A press on the window starts a possible drag: from the next frame
        // ImGui places it, and its top-right corner is kept as fractions; the
        // place is saved when the button is let go.
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            stats_moved_ = true;
        }
        if (stats_moved_)
        {
            const ImVec2 position = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            stats_drag_x_ =
                std::clamp((position.x + size.x - viewport->WorkPos.x) / viewport->WorkSize.x, 0.0f, 1.0f);
            stats_drag_y_ = std::clamp((position.y - viewport->WorkPos.y) / viewport->WorkSize.y, 0.0f, 1.0f);
        }
    }
    ImGui::End();
    if (stats_moved_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        stats_moved_ = false;
        if (std::fabs(stats_drag_x_ - settings_.stats_x) > 0.001f ||
            std::fabs(stats_drag_y_ - settings_.stats_y) > 0.001f)
        {
            settings_.stats_x = stats_drag_x_;
            settings_.stats_y = stats_drag_y_;
            Save();
        }
    }
    if (!open)
    {
        // The close box hides every section.
        settings_.show_fps = false;
        settings_.show_memory = false;
        settings_.show_video_memory = false;
        stats_moved_ = false;
        Save();
    }
}

}  // namespace rejsrf
