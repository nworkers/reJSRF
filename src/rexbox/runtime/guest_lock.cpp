#include "rexbox/runtime/guest_lock.h"

#include "rexbox/runtime/thread_activity.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using rexbox::runtime::CurrentThreadName;
using rexbox::runtime::ScopedWait;

struct LockState
{
    const char* name = "?";
    std::timed_mutex mutex;
    std::atomic<std::thread::id> owner{};
    int depth = 0;  // touched by the owner only
    std::atomic<bool> timeout_reported{false};
    std::string wait_note;
    std::mutex owner_name_mutex;
    std::string owner_name;  // for the timeout report
};

std::mutex g_create_mutex;

// The locks the calling thread owns, and those it put down for a wait with
// the depth to restore.
thread_local std::vector<LockState*> t_held;
thread_local std::vector<std::pair<LockState*, int>> t_put_down;
thread_local int t_wait_depth = 0;
thread_local bool t_in_lock_wait = false;  // waiting for a guest lock itself

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
    state->name = lock->name != nullptr ? lock->name : "?";
    state->wait_note = std::string("guest lock ") + state->name;
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

void Own(LockState& state, int depth)
{
    state.owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
    state.depth = depth;
    t_held.push_back(&state);
    std::lock_guard<std::mutex> guard(state.owner_name_mutex);
    state.owner_name = CurrentThreadName();
}

void Disown(LockState& state)
{
    state.owner.store(std::thread::id(), std::memory_order_relaxed);
    state.depth = 0;
    t_held.erase(std::remove(t_held.begin(), t_held.end(), &state), t_held.end());
    state.mutex.unlock();
}

// Takes the mutex for the calling thread at `depth`, waiting up to the
// limit; past it, reports once and returns without owning it.
void Acquire(LockState& state, int depth)
{
    if (state.mutex.try_lock())
    {
        Own(state, depth);
        return;
    }
    const int64_t timeout = TimeoutMs();
    t_in_lock_wait = true;
    bool owned = false;
    {
        const ScopedWait note(state.wait_note.c_str(), nullptr, 0, false, timeout > 0 ? timeout : -1);
        if (timeout <= 0)
        {
            state.mutex.lock();
            owned = true;
        }
        else
        {
            owned = state.mutex.try_lock_for(std::chrono::milliseconds(timeout));
        }
    }
    t_in_lock_wait = false;
    if (owned)
    {
        Own(state, depth);
        return;
    }
    // Held too long: enter as the Xbox would have let a lower thread in.
    if (!state.timeout_reported.exchange(true))
    {
        std::string holder;
        {
            std::lock_guard<std::mutex> guard(state.owner_name_mutex);
            holder = state.owner_name;
        }
        std::fprintf(stderr,
                     "[guest lock] %s waited %lld ms for \"%s\" held by %s and enters without exclusion "
                     "(reported once per lock)\n",
                     CurrentThreadName().c_str(), static_cast<long long>(timeout), state.name, holder.c_str());
    }
}

}  // namespace

extern "C" void rexbox_guest_lock_enter(RexboxGuestLock* lock)
{
    LockState& state = State(lock);
    if (state.owner.load(std::memory_order_relaxed) == std::this_thread::get_id())
    {
        ++state.depth;
        return;
    }
    Acquire(state, 1);
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
        Disown(state);
    }
}

namespace rexbox::runtime
{

void PutDownGuestLocksForWait()
{
    if (t_in_lock_wait || t_wait_depth++ > 0)
    {
        return;
    }
    for (LockState* state : std::vector<LockState*>(t_held))
    {
        t_put_down.emplace_back(state, state->depth);
        Disown(*state);
    }
}

void PickUpGuestLocksAfterWait()
{
    if (t_in_lock_wait || t_wait_depth == 0 || --t_wait_depth > 0)
    {
        return;
    }
    std::vector<std::pair<LockState*, int>> put_down;
    put_down.swap(t_put_down);
    for (const auto& [state, depth] : put_down)
    {
        Acquire(*state, depth);
    }
}

}  // namespace rexbox::runtime
