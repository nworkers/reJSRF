#include "rexbox/runtime/environment.h"

namespace rexbox::runtime
{

namespace
{

Environment g_environment;

}  // namespace

void SetEnvironment(const Environment& environment)
{
    g_environment = environment;
}

const Environment& Env()
{
    return g_environment;
}

}  // namespace rexbox::runtime
