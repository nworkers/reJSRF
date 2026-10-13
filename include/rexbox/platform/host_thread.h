// Platform contract: host threads with a chosen stack size.
//
// Lifted guest calls nest as native C calls, so guest threads need larger
// native stacks than a default thread provides.

#ifndef REXBOX_PLATFORM_HOST_THREAD_H
#define REXBOX_PLATFORM_HOST_THREAD_H

#include <cstddef>
#include <cstdint>

namespace rexbox::platform
{

using HostThread = uintptr_t;  // 0 = none

// Starts fn(context) on a new thread. Returns 0 on failure.
HostThread CreateHostThread(void (*fn)(void*), void* context, size_t stack_size);

// Waits for the thread to finish and releases it.
void JoinHostThread(HostThread thread);

// Sleeps the calling thread.
void SleepMilliseconds(uint32_t milliseconds);

// The processor time the calling thread has used (user and kernel), in
// nanoseconds; the granularity is the OS clock tick. 0 when unknown.
uint64_t ThreadCpuTimeNanoseconds();

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_HOST_THREAD_H
