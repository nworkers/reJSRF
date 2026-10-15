#include "rexbox/runtime/guest_lock.h"

#include "rexbox/runtime/thread_activity.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace
{

using rexbox::runtime::CurrentThreadName;
using rexbox::runtime::ScopedWait;

struct LockState
{
    std::timed_mutex mutex;
    std::atomic<std::thread::id> owner{};
    int depth = 0;  // touched by the owner only
    std::atomic<bool> timeout_reported{false};
    std::string wait_note;
};

std::mutex g_create_mutex;

LockState& State(RexboxGuestLock* lock)
{
    std::atomic_ref<void*> impl(lock->impl);
    if (void* existing = impl.load(std::memory_order_acquire))
    {
        return *static_cast<LockState*>(existing);
    }
    std::lock_guard<std::mutex> guard(g_create_mutex);
    if (void* existing = impl.load(std::memory_order_acquire))
    {
        return *static_cast<LockState*>(existing);
    }
    auto* state = new LockState;  // lives for the process, like the lock
    state->wait_note = std::string("guest lock ") + (lock->name != nullptr ? lock->name : "?");
    impl.store(state, std::memory_order_release);
    return *state;
}

// REXBOX_GUEST_LOCK_TIMEOUT_MS: how long a waiter waits before it enters
// without exclusion; 0 waits for ever.
int64_t TimeoutMs()
{
    static const int64_t timeout = [] {
        const char* setting = std::getenv("REXBOX_GUEST_LOCK_TIMEOUT_MS");
        return setting != nullptr ? std::strtoll(setting, nullptr, 10) : int64_t{2000};
    }();
    return timeout;
}

void Own(LockState& state)
{
    state.owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
    state.depth = 1;
}

}  // namespace

extern "C" void rexbox_guest_lock_enter(RexboxGuestLock* lock)
{
    LockState& state = State(lock);
    const std::thread::id self = std::this_thread::get_id();
    if (state.owner.load(std::memory_order_relaxed) == self)
    {
        ++state.depth;
        return;
    }
    if (state.mutex.try_lock())
    {
        Own(state);
        return;
    }
    const int64_t timeout = TimeoutMs();
    const ScopedWait note(state.wait_note.c_str(), nullptr, 0, false, timeout > 0 ? timeout : -1);
    if (timeout <= 0)
    {
        state.mutex.lock();
        Own(state);
        return;
    }
    if (state.mutex.try_lock_for(std::chrono::milliseconds(timeout)))
    {
        Own(state);
        return;
    }
    // Held too long: enter as the Xbox would have let a lower thread in.
    if (!state.timeout_reported.exchange(true))
    {
        std::fprintf(stderr,
                     "[guest lock] %s waited %lld ms for \"%s\" and enters without exclusion "
                     "(reported once per lock)\n",
                     CurrentThreadName().c_str(), static_cast<long long>(timeout),
                     lock->name != nullptr ? lock->name : "?");
    }
}

extern "C" void rexbox_guest_lock_leave(RexboxGuestLock* lock)
{
    LockState& state = State(lock);
    if (state.owner.load(std::memory_order_relaxed) != std::this_thread::get_id())
    {
        return;  // entered without exclusion, or an unpaired leave
    }
    if (--state.depth == 0)
    {
        state.owner.store(std::thread::id(), std::memory_order_relaxed);
        state.mutex.unlock();
    }
}
