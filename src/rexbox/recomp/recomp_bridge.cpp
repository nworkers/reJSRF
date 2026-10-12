// The functions recompiled code calls into (declared by recomp_types.h),
// implemented on the active rexbox runtime.

#include "recomp_bridge.h"

#include "rexbox/platform/guarded_call.h"
#include "rexbox/recomp/recomp_hle_entry.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <setjmp.h>
#include <unordered_map>
#include <unordered_set>

namespace rexbox::recomp
{

namespace
{

runtime::GuestMemory* g_memory = nullptr;
runtime::HleRegistry* g_registry = nullptr;
bool g_stop_on_unresolved = false;
// -1: not decided yet (read REXBOX_HLE_FALLBACK on first use), 0 stop, 1 run the lifted original.
int g_hle_fallback_lifted = -1;

thread_local runtime::GuestAddress t_stop_target = 0;
thread_local const RexboxHleSlot* t_stop_slot = nullptr;

std::mutex g_log_mutex;
std::unordered_set<uint32_t> g_logged_sites;

// Logs each distinct site once; repeats would bury the first occurrence.
bool FirstTimeAt(uint32_t site)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return g_logged_sites.insert(site).second;
}

void StopAtUnimplementedKernel()
{
    platform::RaiseStop(kStopUnimplementedKernel);
}

}  // namespace

void AttachBridge(runtime::GuestMemory* memory, runtime::HleRegistry* registry)
{
    g_memory = memory;
    g_registry = registry;
}

void SetStopOnUnresolvedCall(bool stop)
{
    g_stop_on_unresolved = stop;
}

void SetUnimplementedHleFallback(bool run_lifted)
{
    g_hle_fallback_lifted = run_lifted ? 1 : 0;
}

bool BridgeHleFallbackLifted()
{
    if (g_hle_fallback_lifted < 0)
    {
        const char* value = std::getenv("REXBOX_HLE_FALLBACK");
        g_hle_fallback_lifted = value != nullptr && std::strcmp(value, "lifted") == 0 ? 1 : 0;
    }
    return g_hle_fallback_lifted == 1;
}

void TakeStopSymbol(const char** library, const char** name)
{
    *library = t_stop_slot != nullptr ? t_stop_slot->library : nullptr;
    *name = t_stop_slot != nullptr ? t_stop_slot->name : nullptr;
    t_stop_slot = nullptr;
}

runtime::GuestAddress TakeStopTarget()
{
    const runtime::GuestAddress target = t_stop_target;
    t_stop_target = 0;
    return target;
}

// Entry points for the extern "C" callbacks below, which cannot reach the
// anonymous namespace by name.
runtime::GuestMemory* BridgeMemory()
{
    return g_memory;
}

runtime::HleRegistry* BridgeRegistry()
{
    return g_registry;
}

bool BridgeStopOnUnresolved()
{
    return g_stop_on_unresolved;
}

void BridgeSetStopTarget(runtime::GuestAddress target)
{
    t_stop_target = target;
}

bool BridgeFirstTimeAt(uint32_t site)
{
    return FirstTimeAt(site);
}

BridgeFunction RecompLookupManual(uint32_t va)
{
    return g_registry == nullptr ? nullptr : g_registry->Lookup(va);
}

void BridgeStopAtHle(const RexboxHleSlot* slot)
{
    t_stop_target = slot->va;
    t_stop_slot = slot;
    platform::RaiseStop(kStopUnimplementedHle);
}

using GuestThunk = void (*)();

GuestThunk BridgeUnimplementedKernelThunk()
{
    return &StopAtUnimplementedKernel;
}

}  // namespace rexbox::recomp

namespace bridge = rexbox::recomp;

extern "C"
{

typedef void (*recomp_func_t)(void);

// Generated code consults this before its own dispatch table, so a bound HLE
// export wins over a lifted body for indirect calls. Kernel thunk VAs bound
// in the registry resolve here too.
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    rexbox::runtime::HleRegistry* registry = bridge::BridgeRegistry();
    return registry == nullptr ? nullptr : registry->Lookup(xbox_va);
}

// Generated overrides of XDK library functions enter here. The export is
// looked up by name once; concurrent first calls resolve to the same answer.
void rexbox_hle_enter(RexboxHleSlot* slot)
{
    if (slot->state == 0)
    {
        rexbox::runtime::HleRegistry* registry = bridge::BridgeRegistry();
        const rexbox::runtime::HleExport* found = registry != nullptr ? registry->FindExport(slot->name) : nullptr;
        slot->resolved = found != nullptr ? found->thunk : nullptr;
        slot->state = slot->resolved != nullptr ? 1 : 2;
    }
    if (slot->state == 1)
    {
        slot->resolved();
        return;
    }
    // Diagnostic: REXBOX_HLE_FALLBACK=lifted (or SetUnimplementedHleFallback)
    // runs the original body of an unimplemented function instead of
    // stopping, logging each one once, to reveal the order in which a title
    // calls a library.
    if (bridge::BridgeHleFallbackLifted() && slot->lifted != nullptr)
    {
        if (bridge::BridgeFirstTimeAt(slot->va))
        {
            std::fprintf(stderr, "[hle] %s %s (%08X) unimplemented; running the lifted original\n", slot->library,
                         slot->name, slot->va);
        }
        slot->lifted();
        return;
    }
    bridge::BridgeStopAtHle(slot);
}

// Kernel imports are called through synthetic VAs (0xFE000000 and up) that
// boot preparation wrote into the thunk slots. Bound ones were already found
// by recomp_lookup_manual; anything reaching here has no implementation.
recomp_func_t recomp_lookup_kernel(uint32_t xbox_va)
{
    if (!rexbox::runtime::IsKernelThunkVa(xbox_va))
    {
        return nullptr;
    }
    bridge::BridgeSetStopTarget(xbox_va);
    return bridge::BridgeUnimplementedKernelThunk();
}

void recomp_icall_fail_log(uint32_t va)
{
    if (bridge::BridgeStopOnUnresolved())
    {
        bridge::BridgeSetStopTarget(va);
        rexbox::platform::RaiseStop(bridge::kStopUnresolvedCall);
    }
    if (bridge::BridgeFirstTimeAt(va))
    {
        std::fprintf(stderr, "[recomp] indirect call to %08X has no target; skipped\n", va);
    }
}

void recomp_icall_not_code_log(uint32_t va)
{
    if (bridge::BridgeFirstTimeAt(va))
    {
        std::fprintf(stderr, "[recomp] indirect call to %08X is outside code; skipped\n", va);
    }
}

void recomp_unimpl(const char* text, uint32_t va)
{
    if (bridge::BridgeFirstTimeAt(va))
    {
        std::fprintf(stderr, "[recomp] untranslated instruction at %08X: %s\n", va, text);
    }
}

// INT 2Dh debug services. Service 1 prints the ANSI_STRING at arg_va
// ({USHORT Length; USHORT MaximumLength; PCHAR Buffer}).
void recomp_debug_service(uint32_t service, uint32_t arg_va)
{
    rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
    if (service != 1 || memory == nullptr || !memory->IsValid(arg_va, 8))
    {
        return;
    }
    const uint8_t* header = memory->Translate(arg_va);
    const uint32_t length = static_cast<uint32_t>(header[0] | (header[1] << 8));
    const uint32_t buffer = static_cast<uint32_t>(header[4]) | (static_cast<uint32_t>(header[5]) << 8) |
                            (static_cast<uint32_t>(header[6]) << 16) | (static_cast<uint32_t>(header[7]) << 24);
    if (memory->IsValid(buffer, length))
    {
        std::fprintf(stderr, "[guest] %.*s", static_cast<int>(length),
                     reinterpret_cast<const char*>(memory->Translate(buffer)));
    }
}

// The Xbox CPU runs at 733 MHz. A time-based counter keeps the rate the guest
// expects on every host architecture.
uint64_t xbox_ReadTimeStampCounter(void)
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
    return static_cast<uint64_t>(ns) * 733u / 1000u;
}

void recomp_trace_enter(const char* name, uint32_t va)
{
    std::fprintf(stderr, "[trace] enter %s %08X\n", name, va);
}

void recomp_trace_exit(const char* name, uint32_t va)
{
    std::fprintf(stderr, "[trace] exit  %s %08X\n", name, va);
}

void recomp_trace_esp(const char* name, const char* tag)
{
    std::fprintf(stderr, "[trace] esp   %s %s\n", name, tag);
}

// Native buffers paired with guest jmp_buf addresses. JSRF's generated code
// does not use them, so recomp_guest_longjmp reports "no native counterpart"
// and the guest's own longjmp runs.
jmp_buf* recomp_setjmp_slot(uint32_t buf_va)
{
    static std::mutex mutex;
    static std::unordered_map<uint32_t, jmp_buf*> slots;
    std::lock_guard<std::mutex> lock(mutex);
    jmp_buf*& slot = slots[buf_va];
    if (slot == nullptr)
    {
        slot = new jmp_buf[1];
    }
    return slot;
}

int recomp_guest_longjmp(uint32_t /*buf_va*/, uint32_t /*value*/)
{
    return 0;
}

}  // extern "C"
