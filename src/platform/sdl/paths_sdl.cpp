// Paths contract on SDL3.

#include "rexbox/platform/paths.h"

#include <SDL3/SDL.h>

namespace rexbox::platform
{

std::string ExecutableDirectory()
{
    const char* base = SDL_GetBasePath();  // owned by SDL
    return base != nullptr ? std::string(base) : std::string();
}

std::string UserSettingsDirectory()
{
    char* pref = SDL_GetPrefPath("reJSRF", "reJSRF");
    if (pref == nullptr)
    {
        return std::string();
    }
    std::string path(pref);
    SDL_free(pref);
    return path;
}

}  // namespace rexbox::platform
