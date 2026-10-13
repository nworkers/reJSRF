#include "rexbox/runtime/environment.h"

#include <atomic>
#include <cstdio>

namespace rexbox::runtime
{

namespace
{

Environment g_environment;
std::atomic<ExitHandler> g_exit_handler{nullptr};

}  // namespace

void SetEnvironment(const Environment& environment)
{
    g_environment = environment;
}

const Environment& Env()
{
    return g_environment;
}

void SetExitHandler(ExitHandler handler)
{
    g_exit_handler.store(handler);
}

void RequestExit(int exit_code, const char* reason)
{
    const ExitHandler handler = g_exit_handler.load();
    if (handler == nullptr)
    {
        std::fprintf(stderr, "[runtime] exit requested (%s), but nothing handles it\n", reason);
        return;
    }
    handler(exit_code, reason);
}

}  // namespace rexbox::runtime
