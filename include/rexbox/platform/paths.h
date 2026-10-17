// Platform contract: folders the application keeps files in.

#ifndef REXBOX_PLATFORM_PATHS_H
#define REXBOX_PLATFORM_PATHS_H

#include <string>

namespace rexbox::platform
{

// The folder of the running executable, ending in a separator; empty if
// unknown.
std::string ExecutableDirectory();

// A per-user folder for the application's settings (created if needed),
// ending in a separator; empty if unavailable.
std::string UserSettingsDirectory();

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_PATHS_H
