#include "settings_ui.h"

#include "rexbox/hle/d3d8/d3d8_display.h"
#include "rexbox/platform/display.h"
#include "rexbox/platform/overlay.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"

#include "imgui.h"

#include <algorithm>
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

// The FPS panel: the last 60 s in 0.5 s bins, on a 0..70 FPS axis.
constexpr double kHistorySeconds = 60.0;
constexpr double kBinSeconds = 0.5;
constexpr int kBins = static_cast<int>(kHistorySeconds / kBinSeconds);
constexpr float kAxisMax = 70.0f;

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

std::string Mib(uint64_t bytes)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
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

}  // namespace

SettingsUi::SettingsUi(BuildInfo build, Settings settings, SettingsFile* file)
    : build_(std::move(build)), settings_(settings), file_(file)
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
    const bool widescreen = rexbox::hle::d3d8::Widescreen();
    const bool vsync = info.swap_interval != 0;
    if (!observed_)
    {
        // The state at start (environment variables may have overridden the
        // file) is not written until something changes.
        observed_ = true;
        render_scale_ = {scale, scale};
        fullscreen_ = {fullscreen, fullscreen};
        widescreen_ = {widescreen, widescreen};
        vsync_ = {vsync, vsync};
        return;
    }
    bool changed = Follow(scale, &render_scale_.seen, &render_scale_.expected, &settings_.render_scale);
    changed |= Follow(fullscreen, &fullscreen_.seen, &fullscreen_.expected, &settings_.fullscreen);
    changed |= Follow(widescreen, &widescreen_.seen, &widescreen_.expected, &settings_.widescreen);
    changed |= Follow(vsync, &vsync_.seen, &vsync_.expected, &settings_.vsync);
    if (changed)
    {
        Save();
    }
}

void SettingsUi::Build()
{
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    presents_.push_back(now);
    while (!presents_.empty() && now - presents_.front() > kHistorySeconds)
    {
        presents_.pop_front();
    }
    ObserveActualState();
    const bool menu = rexbox::platform::OverlayVisible();
    if (menu)
    {
        DrawMenu();
    }
    if (menu || settings_.show_fps_overlay)
    {
        DrawFpsPanel();
    }
}

void SettingsUi::DrawMenu()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // The menu keeps its size in proportion to the window, centered (the
    // platform scales the style with the window).
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const float scale = ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSize(ImVec2(560.0f * scale, 450.0f * scale), ImGuiCond_Always);
    bool open = true;
    if (ImGui::Begin("reJSRF Settings", &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Tab (or holding Back+Start) closes this menu. The game keeps running without input.");
        ImGui::PopStyleColor();
        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("Info"))
            {
                DrawInfoTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Graphics"))
            {
                DrawGraphicsTab();
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

void SettingsUi::RefreshMemory(double now)
{
    if (memory_time_ >= 0.0 && now - memory_time_ < 0.5)
    {
        return;
    }
    memory_time_ = now;
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

void SettingsUi::DrawInfoTab()
{
    rexbox::platform::DisplayInfo info;
    rexbox::platform::GetDisplayInfo(&info);
    const rexbox::hle::d3d8::RendererInfo renderer = rexbox::hle::d3d8::GetRendererInfo();
    const uint32_t scale = rexbox::hle::d3d8::RenderScale();
    if (ImGui::BeginTable("info", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        Row("reJSRF", "v" + build_.version);
        Row("Build", build_.build_time);
        Row("Configuration", build_.identity);
        Row("Commit", build_.commit);
        Row("Game", build_.xbe);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Driver");
    if (ImGui::BeginTable("driver", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        Row("Vendor", renderer.vendor);
        Row("Renderer", renderer.renderer);
        Row(info.gl_es ? "OpenGL ES" : "OpenGL", renderer.version);
        Row("GLSL", renderer.shading_language);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Display");
    if (ImGui::BeginTable("display", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
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
    RefreshMemory(presents_.empty() ? 0.0 : presents_.back());
    ImGui::SeparatorText("Memory");
    if (ImGui::BeginTable("memory", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
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
    if (ImGui::BeginTable("vram", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        const rexbox::hle::d3d8::VideoMemoryInfo& video = video_memory_;
        std::string gpu = "not reported by this driver";
        if (video.driver_total_known)
        {
            gpu = Mib(video.driver_total - video.driver_free) + " of " + Mib(video.driver_total) + " in use (all programs)";
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
    // Rendering resolution.
    static std::vector<std::string> labels;
    if (labels.empty())
    {
        for (uint32_t scale = 1; scale <= kMaxScale; ++scale)
        {
            labels.push_back(Printf("%uX (%ux%u)", scale, kGuestWidth * scale, kGuestHeight * scale));
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

    bool widescreen = widescreen_.expected;
    if (ImGui::Checkbox("Widescreen", &widescreen))
    {
        settings_.widescreen = widescreen;
        widescreen_.expected = widescreen;
        rexbox::hle::d3d8::SetWidescreen(widescreen);
        Save();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Shows the picture at 16:9. For now the 4:3 picture is stretched;\n"
                          "a true widescreen view (Hor+) is planned (issue #85).");

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

    if (ImGui::Checkbox("Show FPS overlay", &settings_.show_fps_overlay))
    {
        Save();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Keeps the FPS graph at the top right with the menu closed.");

    ImGui::Spacing();
    ImGui::TextDisabled("Shortcuts: Ctrl+1..8 resolution, Alt+Enter or double click fullscreen,\n"
                        "Alt+1/2/3 window size, F9 diagnostic capture.");
}

void SettingsUi::DrawFpsPanel()
{
    const double now = presents_.empty() ? 0.0 : presents_.back();
    // FPS from the frame intervals themselves (counting presents per bin
    // would jitter by a frame): intervals ending in a bin over their sum.
    // The current FPS is the same over the last second.
    std::vector<int> bin_frames(kBins, 0);
    std::vector<double> bin_time(kBins, 0.0);
    int recent_frames = 0;
    double recent_time = 0.0;
    const double history_start = now - kHistorySeconds;
    for (size_t i = 1; i < presents_.size(); ++i)
    {
        const double time = presents_[i];
        const double interval = time - presents_[i - 1];
        if (now - time < 1.0)
        {
            ++recent_frames;
            recent_time += interval;
        }
        const int bin = static_cast<int>((time - history_start) / kBinSeconds);
        if (bin >= 0 && bin < kBins)
        {
            ++bin_frames[bin];
            bin_time[bin] += interval;
        }
    }
    const double current_fps = recent_time > 0.0 ? recent_frames / recent_time : 0.0;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 10.0f * ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + margin),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##fps", nullptr, flags))
    {
        ImGui::Text("FPS %.1f", current_fps);
        ImGui::SameLine();
        ImGui::TextDisabled("last 60 s");

        const float scale = ImGui::GetStyle().FontScaleMain;
        const float graph_width = 240.0f * scale;
        const float graph_height = 80.0f * scale;
        const float label_width = ImGui::CalcTextSize("30  half speed").x + 10.0f * scale;
        const float pad = ImGui::GetFontSize() * 0.5f;  // room for the 70 and 0 labels
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(graph_width + label_width, graph_height + pad * 2.0f));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float top = origin.y + pad;
        const auto y_of = [&](float fps) { return top + graph_height * (1.0f - std::clamp(fps, 0.0f, kAxisMax) / kAxisMax); };
        draw->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + graph_width, top + graph_height),
                            IM_COL32(0, 0, 0, 110));

        struct Tick
        {
            float fps;
            const char* label;
            ImU32 color;
            float thickness;
        };
        const Tick ticks[] = {
            {60.0f, "60  full speed", IM_COL32(90, 220, 110, 255), 1.5f},
            {30.0f, "30  half speed", IM_COL32(160, 160, 160, 200), 1.0f},
            {0.0f, "0  stopped", IM_COL32(160, 160, 160, 200), 1.0f},
        };
        for (const Tick& tick : ticks)
        {
            const float y = y_of(tick.fps);
            draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + graph_width, y), tick.color, tick.thickness);
            draw->AddText(ImVec2(origin.x + graph_width + 6.0f * scale, y - ImGui::GetFontSize() * 0.5f), tick.color,
                          tick.label);
        }

        // Bins from before the start have no data and are left out.
        std::vector<ImVec2> points;
        for (int i = 0; i < kBins; ++i)
        {
            // The newest bin is still filling.
            if (history_start + i * kBinSeconds >= 0.0 && i < kBins - 1 && bin_time[i] > 0.0)
            {
                const float fps = static_cast<float>(bin_frames[i] / bin_time[i]);
                points.push_back(ImVec2(origin.x + graph_width * (i + 0.5f) / kBins, y_of(fps)));
            }
        }
        if (points.size() >= 2)
        {
            draw->AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(255, 210, 80, 255), 1.5f);
        }
    }
    ImGui::End();
}

}  // namespace rejsrf
