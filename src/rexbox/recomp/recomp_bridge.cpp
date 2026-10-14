// The functions recompiled code calls into (declared by recomp_types.h),
// implemented on the active rexbox runtime.

#include "recomp_bridge.h"

#include "rexbox/platform/guarded_call.h"
#include "rexbox/recomp/recomp_abi.h"
#include "rexbox/recomp/recomp_hle_entry.h"
#include "rexbox/recomp/recomp_runtime.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"
#include "rexbox/runtime/kernel_thunks.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <setjmp.h>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

// Sites this thread already asked about, direct mapped: a site met again and
// again (an untranslated instruction in a loop) returns without the lock.
// Slot value 0 is empty; site 0 always takes the locked path.
constexpr size_t kSeenSiteSlots = 64;
thread_local uint32_t t_seen_sites[kSeenSiteSlots] = {};

// Logs each distinct site once; repeats would bury the first occurrence.
bool FirstTimeAt(uint32_t site)
{
    uint32_t& slot = t_seen_sites[(site ^ (site >> 6)) % kSeenSiteSlots];
    if (site != 0 && slot == site)
    {
        return false;
    }
    slot = site;
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

// REXBOX_HLE_TRACE=<n>: true for the first n traced calls (default 4000).
bool BridgeTraceEnabled()
{
    static const long trace_limit = [] {
        const char* value = std::getenv("REXBOX_HLE_TRACE");
        return value == nullptr ? 0L : (std::atol(value) > 0 ? std::atol(value) : 4000L);
    }();
    static std::atomic<long> traced{0};
    return trace_limit != 0 && traced.fetch_add(1) < trace_limit;
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

// Call profile (generated code built with RECOMP_CALL_PROFILE): per guest
// thread, a ring of the most recent direct-call targets and a count per
// target. Read by DumpCallProfile from another thread without locking; it is
// a diagnostic for a thread that stopped making HLE calls.
struct CallRecord
{
    uint32_t va;
    uint32_t esp;   // at the call, after the return address was pushed
    uint32_t site;  // [esp]: the return address, i.e. the call site
    uint32_t arg0;  // [esp+4] and [esp+8] at the call: the first two stack arguments
    uint32_t arg1;
};

struct CallProfile
{
    CallRecord ring[256] = {};
    std::atomic<uint32_t> next{0};
    std::unordered_map<uint32_t, uint64_t> counts;
    uint32_t thread_id = 0;
};

std::mutex g_profile_mutex;
std::vector<CallProfile*> g_profiles;
thread_local CallProfile* t_profile = nullptr;

CallProfile* ThreadProfile()
{
    if (t_profile == nullptr)
    {
        t_profile = new CallProfile;
        t_profile->thread_id = static_cast<uint32_t>(g_profiles.size() + 1);
        std::lock_guard<std::mutex> lock(g_profile_mutex);
        g_profiles.push_back(t_profile);
    }
    return t_profile;
}

void DumpCallProfile(FILE* out)
{
    std::lock_guard<std::mutex> lock(g_profile_mutex);
    if (g_profiles.empty())
    {
        std::fprintf(out, "[profile] no direct-call profile (build the generated code with REJSRF_RECOMP_CALL_PROFILE)\n");
        return;
    }
    for (CallProfile* profile : g_profiles)
    {
        const uint32_t next = profile->next.load();
        std::fprintf(out, "[profile] host thread %u: %u direct calls; most recent targets (oldest first):\n",
                     profile->thread_id, next);
        const uint32_t count = std::min<uint32_t>(next, 256);
        for (uint32_t i = 0; i < count; ++i)
        {
            const CallRecord& record = profile->ring[(next - count + i) % 256];
            std::fprintf(out, "%s%08X(%08X,%08X)@%08X<%08X", i % 4 == 0 ? "\n    " : "  ", record.va, record.arg0,
                         record.arg1, record.esp, record.site);
        }
        std::fprintf(out, "\n[profile] top targets by count:\n");
        std::vector<std::pair<uint32_t, uint64_t>> top(profile->counts.begin(), profile->counts.end());
        std::sort(top.begin(), top.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (size_t i = 0; i < top.size() && i < 24; ++i)
        {
            std::fprintf(out, "    %08X x%llu\n", top[i].first, static_cast<unsigned long long>(top[i].second));
        }
    }
}

// Diagnostic: REXBOX_HEAP_CHECK=<guest address of an NT-style heap>. At every
// HLE entry the heap's variable-size free list (LIST_ENTRY head at +0x180,
// entries at block+8, 16-byte HEAP_ENTRY) is walked; the first inconsistency
// is reported with the direct-call profile and the run stops. Narrows a heap
// corruption to the guest code between two HLE calls.
void CheckWatchedHeap(const char* where)
{
    static const uint32_t heap = [] {
        const char* value = std::getenv("REXBOX_HEAP_CHECK");
        return value == nullptr ? 0u : static_cast<uint32_t>(std::strtoul(value, nullptr, 16));
    }();
    static std::atomic<bool> reported{false};
    runtime::GuestMemory* memory = g_memory;
    if (heap == 0 || memory == nullptr || reported.load())
    {
        return;
    }
    // Only once the heap exists (HEAP.Signature) and its list was initialized.
    if (!memory->IsValid(heap, 0x200) || memory->Read32(heap + 0x10) != 0xEEFFEEFFu || memory->Read32(heap + 0x180) == 0)
    {
        return;
    }
    const uint32_t head = heap + 0x180;
    uint32_t entry = memory->Read32(head);
    uint32_t previous = head;
    std::string problem;
    for (int steps = 0; steps < 100000; ++steps)
    {
        if (entry == head)
        {
            break;
        }
        if (!memory->IsValid(entry, 8) || entry < heap || entry > heap + 0x4000000)
        {
            problem = "Flink out of range";
            break;
        }
        if (memory->Read32(entry + 4) != previous)
        {
            problem = "Blink does not point back";
            break;
        }
        if ((memory->Read8(entry - 8 + 5) & 1) != 0)
        {
            problem = "linked block is busy";
            break;
        }
        previous = entry;
        entry = memory->Read32(entry);
    }
    if (problem.empty() && entry != head)
    {
        problem = "list does not return to its head";
    }
    if (!problem.empty() && !reported.exchange(true))
    {
        std::fprintf(stderr, "[heap-check] %s at %s: entry %08X (previous %08X), head %08X -> %08X\n", problem.c_str(),
                     where, entry, previous, head, memory->Read32(head));
        DumpCallProfile(stderr);
        std::fflush(stderr);
        std::_Exit(5);
    }
}

// Diagnostic: REXBOX_WATCH32=<hex>[,<hex>...]. At every HLE entry (XDK
// override or kernel import) the DWORDs at those guest addresses are compared
// with their last value; a change is logged with where the HLE was entered
// from. Narrows an unexpected write to the guest code between two HLE calls.
void CheckWatchedDwords(const char* where)
{
    struct Watch
    {
        uint32_t address;
        uint32_t value;
    };
    static std::vector<Watch> watches = [] {
        std::vector<Watch> list;
        const char* value = std::getenv("REXBOX_WATCH32");
        while (value != nullptr && *value != '\0')
        {
            char* end = nullptr;
            const uint32_t address = static_cast<uint32_t>(std::strtoul(value, &end, 16));
            list.push_back({address, 0});
            value = (end != nullptr && *end == ',') ? end + 1 : nullptr;
        }
        return list;
    }();
    runtime::GuestMemory* memory = g_memory;
    if (watches.empty() || memory == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(g_log_mutex);
    const uint32_t esp = rexbox_recomp_get_gpr(4);
    const uint32_t caller = memory->IsValid(esp, 4) ? memory->Read32(esp) : 0;
    for (Watch& watch : watches)
    {
        if (!memory->IsValid(watch.address, 4))
        {
            continue;
        }
        const uint32_t now = memory->Read32(watch.address);
        if (now != watch.value)
        {
            std::fprintf(stderr, "[watch] [%08X] %08X -> %08X before %s (from %08X)\n", watch.address, watch.value,
                         now, where, caller);
            watch.value = now;
        }
    }
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

// The guest return address on top of the stack: the lifter pushes it before an
// indirect call, so it names the call site.
uint32_t BridgeReturnAddress()
{
    const uint32_t esp = rexbox_recomp_get_gpr(4);
    return g_memory != nullptr && g_memory->IsValid(esp, 4) ? g_memory->Read32(esp) : 0;
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
    // Diagnostic: with REXBOX_HLE_TRACE, kernel imports are logged by ordinal
    // as they are looked up (the same limit as the XDK trace).
    if (rexbox::runtime::IsKernelThunkVa(xbox_va))
    {
        bridge::CheckWatchedHeap("kernel import");
        bridge::CheckWatchedDwords("kernel import");
    }
    if (rexbox::runtime::IsKernelThunkVa(xbox_va) && bridge::BridgeTraceEnabled())
    {
        const uint32_t esp = rexbox_recomp_get_gpr(4);
        rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
        std::fprintf(stderr, "[hle-trace] kernel ordinal %u(%08X, %08X, %08X) esp %08X\n",
                     rexbox::runtime::KernelThunkOrdinal(xbox_va), memory->Read32(esp), memory->Read32(esp + 4),
                     memory->Read32(esp + 8), esp);
    }
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
        bridge::CheckWatchedHeap(slot->name);
        bridge::CheckWatchedDwords(slot->name);
        // Diagnostic: REXBOX_HLE_TRACE=<n> logs the first n HLE entries (default 4000).
        if (bridge::BridgeTraceEnabled())
        {
            const uint32_t esp = rexbox_recomp_get_gpr(4);
            rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
            const uint32_t arg0 = memory->Read32(esp + 4);
            const uint32_t arg1 = memory->Read32(esp + 8);
            const uint32_t arg2 = memory->Read32(esp + 12);
            const uint32_t caller = memory->Read32(esp);
            const uint32_t ecx = rexbox_recomp_get_gpr(1);
            slot->resolved();
            std::fprintf(stderr, "[hle-trace] %s %s(%08X, %08X, %08X; ecx %08X) from %08X -> eax %08X\n",
                         slot->library, slot->name, arg0, arg1, arg2, ecx, caller, rexbox_recomp_get_gpr(0));
            return;
        }
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

// Generated code built with RECOMP_ABI_CHECK (CMake REJSRF_RECOMP_ABI_CHECK)
// reports a direct call whose callee changed ebx/esi/edi or came back without
// popping even its return address -- a stack imbalance that otherwise shows up
// far away. The first 16 are logged with the call site.
void recomp_abi_violation_log(uint32_t va, uint32_t ebx0, uint32_t esi0, uint32_t edi0, uint32_t esp0)
{
    // REXBOX_ABI_IGNORE=<hex>[,<hex>...] skips callees that move esp down by
    // design (_chkstk, SEH prologs); REXBOX_ABI_ESP_ONLY reports only the
    // missing-pop case, not register clobbers.
    static const std::vector<uint32_t> ignored = [] {
        std::vector<uint32_t> list;
        const char* value = std::getenv("REXBOX_ABI_IGNORE");
        while (value != nullptr && *value != '\0')
        {
            char* end = nullptr;
            list.push_back(static_cast<uint32_t>(std::strtoul(value, &end, 16)));
            value = (end != nullptr && *end == ',') ? end + 1 : nullptr;
        }
        return list;
    }();
    static const bool esp_only = std::getenv("REXBOX_ABI_ESP_ONLY") != nullptr;
    if (std::find(ignored.begin(), ignored.end(), va) != ignored.end())
    {
        return;
    }
    if (esp_only && rexbox_recomp_get_gpr(4) >= esp0 + 4)
    {
        return;
    }
    static std::atomic<int> logged{0};
    if (logged.fetch_add(1) >= 32)
    {
        return;
    }
    rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
    const uint32_t site = memory != nullptr && memory->IsValid(esp0 - 4, 4) ? memory->Read32(esp0 - 4) : 0;
    std::fprintf(stderr,
                 "[abi] call %08X (returns to %08X): ebx %08X->%08X esi %08X->%08X edi %08X->%08X "
                 "esp %08X->%08X\n",
                 va, site, ebx0, rexbox_recomp_get_gpr(3), esi0, rexbox_recomp_get_gpr(6), edi0,
                 rexbox_recomp_get_gpr(7), esp0, rexbox_recomp_get_gpr(4));
}

// Generated code built with RECOMP_CALL_PROFILE reports every direct call.
void recomp_call_profile_hit(uint32_t va)
{
    bridge::CallProfile* profile = bridge::ThreadProfile();
    const uint32_t index = profile->next.fetch_add(1);
    const uint32_t esp = rexbox_recomp_get_gpr(4);
    rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
    bridge::CallRecord& record = profile->ring[index % 256];
    record.va = va;
    record.esp = esp;
    record.site = memory != nullptr && memory->IsValid(esp, 4) ? memory->Read32(esp) : 0;
    record.arg0 = memory != nullptr && memory->IsValid(esp + 4, 8) ? memory->Read32(esp + 4) : 0;
    record.arg1 = memory != nullptr && memory->IsValid(esp + 4, 8) ? memory->Read32(esp + 8) : 0;
    ++profile->counts[va];
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
        std::fprintf(stderr, "[recomp] indirect call to %08X has no target; skipped (returns to %08X)\n", va,
                     bridge::BridgeReturnAddress());
    }
}

// An unresolved-target stub ran: the disassembler did not find a function at
// va, so the code there was skipped.
void recomp_unresolved_stub_log(uint32_t va)
{
    if (bridge::BridgeFirstTimeAt(va))
    {
        std::fprintf(stderr, "[recomp] unresolved stub %08X called; its code was skipped (returns to %08X)\n", va,
                     bridge::BridgeReturnAddress());
    }
}

void recomp_icall_not_code_log(uint32_t va)
{
    if (bridge::BridgeFirstTimeAt(va))
    {
        std::fprintf(stderr, "[recomp] indirect call to %08X is outside code; skipped (returns to %08X)\n", va,
                     bridge::BridgeReturnAddress());
        // Diagnostic: REXBOX_PROFILE_ON_SKIP dumps the direct-call profile at
        // the first skipped call (generated code built with REJSRF_RECOMP_CALL_PROFILE).
        static bool dumped = false;
        if (!dumped && std::getenv("REXBOX_PROFILE_ON_SKIP") != nullptr)
        {
            dumped = true;
            bridge::DumpCallProfile(stderr);
        }
        // Diagnostic: REXBOX_SKIP_DUMP=<file> writes the guest RAM and the
        // registers at the first skipped call.
        static bool memory_dumped = false;
        const char* dump_path = std::getenv("REXBOX_SKIP_DUMP");
        rexbox::runtime::GuestMemory* memory = bridge::BridgeMemory();
        if (!memory_dumped && dump_path != nullptr && memory != nullptr)
        {
            memory_dumped = true;
            if (FILE* dump = std::fopen(dump_path, "wb"))
            {
                std::fwrite(memory->base(), 1, memory->size(), dump);
                std::fclose(dump);
            }
            std::fprintf(stderr, "[recomp] skip dump %s: eax %08X ecx %08X edx %08X ebx %08X esp %08X esi %08X edi %08X\n",
                         dump_path, rexbox_recomp_get_gpr(0), rexbox_recomp_get_gpr(1), rexbox_recomp_get_gpr(2),
                         rexbox_recomp_get_gpr(3), rexbox_recomp_get_gpr(4), rexbox_recomp_get_gpr(6),
                         rexbox_recomp_get_gpr(7));
        }
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
