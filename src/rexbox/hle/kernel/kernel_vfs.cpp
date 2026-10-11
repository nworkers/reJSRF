#include "kernel_vfs.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <system_error>
#include <vector>

namespace rexbox::hle::kernel
{

namespace
{

struct Mount
{
    std::string device;  // upper-case, e.g. \DEVICE\CDROM0
    std::filesystem::path host_root;
    bool read_only;
};

std::mutex g_mutex;
std::vector<Mount> g_mounts;
std::map<std::string, std::string> g_links;  // upper-case link -> target as written

std::string Upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

// Collapses separators to single backslashes and drops a trailing one.
std::string Normalize(const std::string& path)
{
    std::string out;
    for (const char c : path)
    {
        const char ch = c == '/' ? '\\' : c;
        if (ch == '\\' && !out.empty() && out.back() == '\\')
        {
            continue;
        }
        out.push_back(ch);
    }
    while (out.size() > 1 && out.back() == '\\')
    {
        out.pop_back();
    }
    return out;
}

bool HasPrefix(const std::string& upper_path, const std::string& upper_prefix)
{
    return upper_path.compare(0, upper_prefix.size(), upper_prefix) == 0 &&
           (upper_path.size() == upper_prefix.size() || upper_path[upper_prefix.size()] == '\\');
}

// Finds `name` in `directory` ignoring case; returns the name as stored on the
// host, or `name` itself when there is no such entry.
std::string MatchEntry(const std::filesystem::path& directory, const std::string& name)
{
    std::error_code ec;
    if (std::filesystem::exists(directory / name, ec))
    {
        return name;
    }
    const std::string wanted = Upper(name);
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        const std::string candidate = entry.path().filename().string();
        if (Upper(candidate) == wanted)
        {
            return candidate;
        }
    }
    return name;
}

}  // namespace

void VfsInitialize(const std::filesystem::path& game_directory, const std::filesystem::path& hdd_root)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_mounts.clear();
    g_links.clear();
    g_mounts.push_back({"\\DEVICE\\CDROM0", game_directory, true});
    for (int partition = 1; partition <= 5; ++partition)
    {
        const std::string name = "Partition" + std::to_string(partition);
        std::error_code ec;
        std::filesystem::create_directories(hdd_root / name, ec);
        g_mounts.push_back({Upper("\\Device\\Harddisk0\\" + name), hdd_root / name, false});
    }
    // The kernel's launch of an XBE from disc maps D: to the disc.
    g_links["\\??\\D:"] = "\\Device\\CdRom0";
}

std::optional<VfsPath> VfsResolve(const std::string& xbox_path)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string path = Normalize(xbox_path);

    // Apply symbolic links until none matches (bounded against cycles).
    for (int depth = 0; depth < 8; ++depth)
    {
        const std::string upper = Upper(path);
        const std::string* best_link = nullptr;
        const std::string* best_target = nullptr;
        for (const auto& [link, target] : g_links)
        {
            if (HasPrefix(upper, link) && (best_link == nullptr || link.size() > best_link->size()))
            {
                best_link = &link;
                best_target = &target;
            }
        }
        if (best_link == nullptr)
        {
            break;
        }
        path = Normalize(*best_target + path.substr(best_link->size()));
    }

    const std::string upper = Upper(path);
    if (upper == "\\DEVICE\\HARDDISK0\\PARTITION0")
    {
        VfsPath result;
        result.xbox_path = path;
        result.raw_device = true;
        return result;
    }
    for (const Mount& mount : g_mounts)
    {
        if (!HasPrefix(upper, mount.device))
        {
            continue;
        }
        VfsPath result;
        result.xbox_path = path;
        result.read_only = mount.read_only;
        result.volume = upper.size() == mount.device.size();
        std::filesystem::path host = mount.host_root;
        size_t position = mount.device.size();
        while (position < path.size())
        {
            const size_t start = path[position] == '\\' ? position + 1 : position;
            const size_t end = path.find('\\', start);
            const std::string component = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!component.empty() && component != ".")
            {
                host /= MatchEntry(host, component);
            }
            if (end == std::string::npos)
            {
                break;
            }
            position = end;
        }
        result.host_path = host;
        return result;
    }
    return std::nullopt;
}

bool VfsCreateSymbolicLink(const std::string& link, const std::string& target)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_links.insert_or_assign(Upper(Normalize(link)), Normalize(target)).second;
}

bool VfsDeleteSymbolicLink(const std::string& link)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_links.erase(Upper(Normalize(link))) != 0;
}

std::optional<std::string> VfsSymbolicLinkTarget(const std::string& link)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto it = g_links.find(Upper(Normalize(link)));
    if (it == g_links.end())
    {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace rexbox::hle::kernel
