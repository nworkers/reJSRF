// The active runtime objects HLE code works against. The app installs them
// once before guest code runs; they then live for the rest of the process.

#ifndef REXBOX_RUNTIME_ENVIRONMENT_H
#define REXBOX_RUNTIME_ENVIRONMENT_H

namespace rexbox::runtime
{

class GuestExecutor;
class GuestHeap;
class GuestMemory;
class HleRegistry;

struct Environment
{
    GuestMemory* memory = nullptr;
    GuestHeap* heap = nullptr;
    HleRegistry* registry = nullptr;
    GuestExecutor* executor = nullptr;
};

void SetEnvironment(const Environment& environment);
const Environment& Env();

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_ENVIRONMENT_H
