// Internal to the kernel HLE: the Xbox object namespace for files, mapped
// onto host directories.
//
//   \Device\CdRom0                     -> the game directory (read-only)
//   \Device\Harddisk0\Partition0       -> the raw disk (a device, no files)
//   \Device\Harddisk0\Partition1..5    -> <hdd root>/Partition1..5
//   \??\X:                             -> symbolic links (IoCreateSymbolicLink);
//                                         \??\D: is created at boot for the disc
//
// Names compare case-insensitively, as on FATX and the Xbox DVD file system,
// on every host.

#ifndef REXBOX_HLE_KERNEL_KERNEL_VFS_H
#define REXBOX_HLE_KERNEL_KERNEL_VFS_H

#include <filesystem>
#include <optional>
#include <string>

namespace rexbox::hle::kernel
{

struct VfsPath
{
    std::string xbox_path;           // normalized, links resolved
    std::filesystem::path host_path; // the host file or directory
    bool read_only = false;
    bool raw_device = false;         // a whole-disk device (no file system)
    bool volume = false;             // a partition opened by its bare device name
};

void VfsInitialize(const std::filesystem::path& game_directory, const std::filesystem::path& hdd_root);

// Resolves an Xbox object name to a host path. Symbolic links apply to the
// longest matching prefix. Missing trailing components are kept as written so
// a file can be created. std::nullopt for names outside any mounted device.
std::optional<VfsPath> VfsResolve(const std::string& xbox_path);

bool VfsCreateSymbolicLink(const std::string& link, const std::string& target);
bool VfsDeleteSymbolicLink(const std::string& link);
std::optional<std::string> VfsSymbolicLinkTarget(const std::string& link);

}  // namespace rexbox::hle::kernel

#endif  // REXBOX_HLE_KERNEL_KERNEL_VFS_H
