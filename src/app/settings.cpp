#include "settings.h"

#include "rexbox/platform/paths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace rejsrf
{

namespace
{

constexpr const char* kFileName = "rejsrf.ini";

// Paths are UTF-8 (SDL hands them out so).
std::filesystem::path Utf8Path(const std::string& path)
{
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

std::string Trim(const std::string& text)
{
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); });
    return first < last.base() ? std::string(first, last.base()) : std::string();
}

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool ParseBool(const std::string& value, bool* out)
{
    const std::string v = Lower(value);
    if (v == "true" || v == "1" || v == "yes" || v == "on")
    {
        *out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off")
    {
        *out = false;
        return true;
    }
    return false;
}

bool Exists(const std::string& path)
{
    std::error_code error;
    return !path.empty() && std::filesystem::exists(Utf8Path(path), error);
}

bool WriteFile(const std::string& path, const std::string& text)
{
    const std::filesystem::path target = Utf8Path(path);
    const std::filesystem::path temporary = Utf8Path(path + ".tmp");
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        out << text;
        if (!out.flush())
        {
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);  // replaces the old file
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

}  // namespace

SettingsFile::SettingsFile()
{
    const std::string executable = rexbox::platform::ExecutableDirectory();
    const std::string user = rexbox::platform::UserSettingsDirectory();
    const std::string beside = executable.empty() ? std::string() : executable + kFileName;
    fallback_path_ = user.empty() ? std::string() : user + kFileName;
    // The executable's folder first; a file only in the user's folder means
    // an earlier run could not write beside the executable.
    path_ = beside;
    if (path_.empty() || (!Exists(beside) && Exists(fallback_path_)))
    {
        path_ = fallback_path_;
    }
    if (path_.empty())
    {
        path_ = kFileName;  // the current folder
    }
}

Settings SettingsFile::Load()
{
    Settings settings;
    std::ifstream in(Utf8Path(path_), std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "[settings] %s not found; defaults\n", path_.c_str());
        return settings;
    }
    std::string section;
    std::string line;
    while (std::getline(in, line))
    {
        const size_t comment = line.find_first_of(";#");
        line = Trim(comment == std::string::npos ? line : line.substr(0, comment));
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos || (section != "graphics" && section != "input"))
        {
            continue;
        }
        const std::string key = Lower(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));
        if (section == "input")
        {
            if (key == "vibration")
            {
                ParseBool(value, &settings.vibration);
            }
            else if (key == "vibrationstrength")
            {
                char* end = nullptr;
                const long percent = std::strtol(value.c_str(), &end, 10);
                if (end != value.c_str() && *end == '\0' && percent >= 10 && percent <= 100)
                {
                    settings.vibration_strength = static_cast<uint32_t>(percent);
                }
            }
            continue;
        }
        if (key == "renderscale")
        {
            char* end = nullptr;
            const long scale = std::strtol(value.c_str(), &end, 10);
            if (end != value.c_str() && *end == '\0' && scale >= 1 && scale <= 8)
            {
                settings.render_scale = static_cast<uint32_t>(scale);
            }
        }
        else if (key == "windowscale")
        {
            char* end = nullptr;
            const long scale = std::strtol(value.c_str(), &end, 10);
            if (end != value.c_str() && *end == '\0' && scale >= 1 && scale <= 3)
            {
                settings.window_scale = static_cast<uint32_t>(scale);
            }
        }
        else if (key == "msaa" || key == "anisotropy")
        {
            char* end = nullptr;
            const long count = std::strtol(value.c_str(), &end, 10);
            const bool power_of_two = count > 0 && (count & (count - 1)) == 0;
            if (end != value.c_str() && *end == '\0' && count >= 0 && count <= 16 && (count == 0 || power_of_two))
            {
                if (key == "msaa")
                {
                    settings.msaa = count >= 2 ? static_cast<uint32_t>(count) : 0;
                }
                else
                {
                    settings.anisotropy = count >= 1 ? static_cast<uint32_t>(count) : 1;
                }
            }
        }
        else if (key == "smaa")
        {
            const std::string mode = Lower(value);
            if (mode == "off" || mode == "1x")
            {
                settings.smaa = mode == "1x" ? 1 : 0;
            }
        }
        else if (key == "fullscreen")
        {
            ParseBool(value, &settings.fullscreen);
        }
        else if (key == "aspect")
        {
            const std::string mode = Lower(value);
            if (mode == "4:3" || mode == "16:9" || mode == "21:9")
            {
                settings.aspect = mode == "16:9" ? 1 : mode == "21:9" ? 2 : 0;
            }
        }
        else if (key == "widescreen")
        {
            // Before v0.0.40: true stretched the picture to 16:9.
            bool widescreen = false;
            if (ParseBool(value, &widescreen))
            {
                settings.aspect = widescreen ? 1 : 0;
            }
        }
        else if (key == "vsync")
        {
            ParseBool(value, &settings.vsync);
        }
        else if (key == "vblankwait")
        {
            ParseBool(value, &settings.vblank_wait);
        }
        else if (key == "showfpsoverlay" || key == "showfps")
        {
            ParseBool(value, &settings.show_fps);
        }
        else if (key == "showmemory")
        {
            ParseBool(value, &settings.show_memory);
        }
        else if (key == "showvideomemory")
        {
            ParseBool(value, &settings.show_video_memory);
        }
        else if (key == "statsx" || key == "statsy")
        {
            char* end = nullptr;
            const float fraction = std::strtof(value.c_str(), &end);
            if (end != value.c_str() && *end == '\0' && fraction >= 0.0f && fraction <= 1.0f)
            {
                (key == "statsx" ? settings.stats_x : settings.stats_y) = fraction;
            }
        }
    }
    if (settings.msaa != 0)
    {
        settings.smaa = 0;  // they exclude each other
    }
    std::fprintf(stderr, "[settings] loaded %s\n", path_.c_str());
    return settings;
}

bool SettingsFile::Save(const Settings& settings)
{
    std::ostringstream text;
    const auto flag = [](bool value) { return value ? "true" : "false"; };
    text << "; reJSRF settings, written by the settings menu (Tab opens it).\n"
         << "; Environment variables (REXBOX_RENDER_SCALE, REXBOX_VSYNC) override these for a run.\n"
         << "[Graphics]\n"
         << "; WindowScale: 1..3 (the window at start). RenderScale: 1..8 (1: 640x480).\n"
         << "; Aspect: 4:3, 16:9 or 21:9 (wider shows more at the sides; the HUD stays centered).\n"
         << "; VSync: true for adaptive vsync, false for none.\n"
         << "WindowScale = " << settings.window_scale << "\n"
         << "RenderScale = " << settings.render_scale << "\n"
         << "Fullscreen = " << flag(settings.fullscreen) << "\n"
         << "Aspect = " << (settings.aspect == 1 ? "16:9" : settings.aspect == 2 ? "21:9" : "4:3") << "\n"
         << "VSync = " << flag(settings.vsync) << "\n"
         << "; VBlankWait: false lets the game run as fast as it draws (game speed follows).\n"
         << "VBlankWait = " << flag(settings.vblank_wait) << "\n"
         << "; Msaa: 0, 2, 4, 8, 16. Smaa: off, 1x (not with Msaa). Anisotropy: 1, 2, 4, 8, 16.\n"
         << "Msaa = " << settings.msaa << "\n"
         << "Smaa = " << (settings.smaa == 1 ? "1x" : "off") << "\n"
         << "Anisotropy = " << settings.anisotropy << "\n"
         << "; The stats window: its sections, and its top-right corner as fractions of the window.\n"
         << "ShowFps = " << flag(settings.show_fps) << "\n"
         << "ShowMemory = " << flag(settings.show_memory) << "\n"
         << "ShowVideoMemory = " << flag(settings.show_video_memory) << "\n";
    if (settings.stats_x >= 0.0f && settings.stats_y >= 0.0f)
    {
        text << "StatsX = " << settings.stats_x << "\n"
             << "StatsY = " << settings.stats_y << "\n";
    }
    text << "[Input]\n"
         << "; Vibration: gamepad rumble. VibrationStrength: 10..100 (percent).\n"
         << "Vibration = " << flag(settings.vibration) << "\n"
         << "VibrationStrength = " << settings.vibration_strength << "\n";
    if (WriteFile(path_, text.str()))
    {
        return true;
    }
    if (!fallback_path_.empty() && fallback_path_ != path_ && WriteFile(fallback_path_, text.str()))
    {
        std::fprintf(stderr, "[settings] cannot write %s; using %s\n", path_.c_str(), fallback_path_.c_str());
        path_ = fallback_path_;
        return true;
    }
    std::fprintf(stderr, "[settings] cannot write %s\n", path_.c_str());
    return false;
}

}  // namespace rejsrf
