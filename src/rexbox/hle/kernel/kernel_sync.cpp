// Dispatcher objects (events, semaphores, mutants, timers, threads) and
// waits on them.
//
// State lives where the Xbox keeps it -- DISPATCHER_HEADER.SignalState in
// guest memory -- and is changed only under the dispatcher lock. Waiters
// sleep on one condition variable and re-check on every signal.

#include "kernel_internal.h"

#include <vector>

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;
namespace L = layout;

constexpr uint32_t kStatusAbandoned = 0x00000080;
constexpr uint32_t kMaximumWaitObjects = 64;

ObjectType TypeOf(GuestAddress object)
{
    return static_cast<ObjectType>(Memory().Read8(object + L::kHeaderType) & 0x7F);
}

int32_t SignalState(GuestAddress object)
{
    return static_cast<int32_t>(Memory().Read32(object + L::kHeaderSignalState));
}

void SetSignalState(GuestAddress object, int32_t value)
{
    Memory().Write32(object + L::kHeaderSignalState, static_cast<uint32_t>(value));
}

// KMUTANT: header, MutantListEntry (+0x10), OwnerThread (+0x18), Abandoned (+0x1C).
constexpr uint32_t kMutantOwnerThread = 0x18;

GuestAddress CurrentEthread()
{
    KernelThread* thread = CurrentThread();
    return thread != nullptr ? thread->ethread : 0;
}

bool IsSignalledLocked(GuestAddress object)
{
    if (TypeOf(object) == ObjectType::kMutant)
    {
        return SignalState(object) > 0 || Memory().Read32(object + kMutantOwnerThread) == CurrentEthread();
    }
    return SignalState(object) > 0;
}

// Applies the side effect of a satisfied wait.
void ConsumeLocked(GuestAddress object)
{
    switch (TypeOf(object))
    {
    case ObjectType::kSynchronizationEvent:
    case ObjectType::kSynchronizationTimer:
        SetSignalState(object, 0);
        break;
    case ObjectType::kSemaphore:
        SetSignalState(object, SignalState(object) - 1);
        break;
    case ObjectType::kMutant:
        SetSignalState(object, SignalState(object) - 1);
        Memory().Write32(object + kMutantOwnerThread, CurrentEthread());
        break;
    default:
        break;
    }
}

GuestAddress ObjectFromHandle(uint32_t handle)
{
    // Pseudo-handle for the current thread.
    if (handle == 0xFFFFFFFE)
    {
        return CurrentEthread();
    }
    const std::shared_ptr<KernelObject> object = LookupHandle(handle);
    return object != nullptr ? object->object : 0;
}

// Waits for one (wait_all = false) or all of `objects`. Returns the NTSTATUS:
// the index of the satisfying object, STATUS_TIMEOUT, or an error.
uint32_t WaitForObjects(const std::vector<GuestAddress>& objects, bool wait_all, const Deadline& deadline)
{
    KernelState& state = State();
    std::unique_lock<std::mutex> lock(state.dispatcher);
    for (;;)
    {
        if (wait_all)
        {
            bool all = true;
            for (const GuestAddress object : objects)
            {
                all = all && IsSignalledLocked(object);
            }
            if (all)
            {
                for (const GuestAddress object : objects)
                {
                    ConsumeLocked(object);
                }
                return kStatusSuccess;
            }
        }
        else
        {
            for (size_t i = 0; i < objects.size(); ++i)
            {
                if (IsSignalledLocked(objects[i]))
                {
                    ConsumeLocked(objects[i]);
                    return static_cast<uint32_t>(i);
                }
            }
        }
        if (deadline)
        {
            if (state.signalled.wait_until(lock, *deadline) == std::cv_status::timeout)
            {
                // Re-check once more under the lock before reporting a timeout.
                bool any = false;
                for (const GuestAddress object : objects)
                {
                    any = any || IsSignalledLocked(object);
                }
                if (!any || wait_all)
                {
                    return kStatusTimeout;
                }
            }
        }
        else
        {
            state.signalled.wait(lock);
        }
    }
}

void NotifyWaitersLocked()
{
    State().signalled.notify_all();
}

// --- Event ---------------------------------------------------------------

// VOID KeInitializeEvent is a macro in the XDK; titles initialize KEVENT
// themselves and only call these.

// LONG KeSetEvent(PRKEVENT Event, KPRIORITY Increment, BOOLEAN Wait)
void KeSetEvent()
{
    HleCall call;
    const GuestAddress event = call.Arg(0);
    int32_t previous = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        previous = SignalState(event);
        SetSignalState(event, 1);
        NotifyWaitersLocked();
    }
    call.Return(static_cast<uint32_t>(previous), 3);
}

// NTSTATUS NtCreateEvent(PHANDLE EventHandle, POBJECT_ATTRIBUTES ObjectAttributes,
//     EVENT_TYPE EventType, BOOLEAN InitialState)
void NtCreateEvent()
{
    HleCall call;
    const GuestAddress handle_out = call.Arg(0);
    const uint32_t type = call.Arg(2);
    const bool initial = (call.Arg(3) & 0xFF) != 0;
    const GuestAddress event = AllocateZeroed(0x10, 16);
    if (event == 0)
    {
        call.Return(kStatusNoMemory, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write8(event + L::kHeaderType, static_cast<uint8_t>(type == 0 ? ObjectType::kNotificationEvent
                                                                         : ObjectType::kSynchronizationEvent));
    memory.Write8(event + L::kHeaderSize, 4);
    memory.Write32(event + L::kHeaderSignalState, initial ? 1 : 0);
    memory.Write32(event + L::kHeaderWaitListHead, event + L::kHeaderWaitListHead);
    memory.Write32(event + L::kHeaderWaitListHead + 4, event + L::kHeaderWaitListHead);

    auto object = std::make_shared<KernelObject>();
    object->kind = KernelObject::Kind::kEvent;
    object->object = event;
    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    memory.Write32(handle_out, handle);
    call.Return(kStatusSuccess, 4);
}

void SetEventByHandle(int stack_args, bool set, bool pulse)
{
    HleCall call;
    const GuestAddress event = ObjectFromHandle(call.Arg(0));
    const GuestAddress previous_out = call.Arg(1);
    if (event == 0)
    {
        call.Return(kStatusInvalidHandle, stack_args);
        return;
    }
    int32_t previous = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        previous = SignalState(event);
        SetSignalState(event, set || pulse ? 1 : 0);
        NotifyWaitersLocked();
    }
    if (pulse)
    {
        // Waiters woken above see the signal; then the event resets.
        std::lock_guard<std::mutex> lock(State().dispatcher);
        SetSignalState(event, 0);
    }
    if (stack_args > 1 && previous_out != 0)
    {
        Memory().Write32(previous_out, static_cast<uint32_t>(previous));
    }
    call.Return(kStatusSuccess, stack_args);
}

// NTSTATUS NtSetEvent(HANDLE EventHandle, PLONG PreviousState)
void NtSetEvent()
{
    SetEventByHandle(2, true, false);
}

// NTSTATUS NtClearEvent(HANDLE EventHandle)
void NtClearEvent()
{
    SetEventByHandle(1, false, false);
}

// NTSTATUS NtPulseEvent(HANDLE EventHandle, PLONG PreviousState)
void NtPulseEvent()
{
    SetEventByHandle(2, false, true);
}

// --- Semaphore -----------------------------------------------------------

// NTSTATUS NtCreateSemaphore(PHANDLE SemaphoreHandle, POBJECT_ATTRIBUTES ObjectAttributes,
//     LONG InitialCount, LONG MaximumCount)
void NtCreateSemaphore()
{
    HleCall call;
    const GuestAddress handle_out = call.Arg(0);
    const GuestAddress semaphore = AllocateZeroed(0x14, 16);
    if (semaphore == 0)
    {
        call.Return(kStatusNoMemory, 4);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write8(semaphore + L::kHeaderType, static_cast<uint8_t>(ObjectType::kSemaphore));
    memory.Write8(semaphore + L::kHeaderSize, 5);
    memory.Write32(semaphore + L::kHeaderSignalState, call.Arg(2));
    memory.Write32(semaphore + 0x10, call.Arg(3));  // Limit
    auto object = std::make_shared<KernelObject>();
    object->kind = KernelObject::Kind::kSemaphore;
    object->object = semaphore;
    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    memory.Write32(handle_out, handle);
    call.Return(kStatusSuccess, 4);
}

// NTSTATUS NtReleaseSemaphore(HANDLE SemaphoreHandle, LONG ReleaseCount, PLONG PreviousCount)
void NtReleaseSemaphore()
{
    HleCall call;
    const GuestAddress semaphore = ObjectFromHandle(call.Arg(0));
    const int32_t count = static_cast<int32_t>(call.Arg(1));
    if (semaphore == 0)
    {
        call.Return(kStatusInvalidHandle, 3);
        return;
    }
    int32_t previous = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        previous = SignalState(semaphore);
        const int32_t limit = static_cast<int32_t>(Memory().Read32(semaphore + 0x10));
        if (previous + count > limit)
        {
            call.Return(kStatusSemaphoreLimitExceeded, 3);
            return;
        }
        SetSignalState(semaphore, previous + count);
        NotifyWaitersLocked();
    }
    if (call.Arg(2) != 0)
    {
        Memory().Write32(call.Arg(2), static_cast<uint32_t>(previous));
    }
    call.Return(kStatusSuccess, 3);
}

// --- Mutant --------------------------------------------------------------

// NTSTATUS NtCreateMutant(PHANDLE MutantHandle, POBJECT_ATTRIBUTES ObjectAttributes, BOOLEAN InitialOwner)
void NtCreateMutant()
{
    HleCall call;
    const GuestAddress handle_out = call.Arg(0);
    const bool owned = (call.Arg(2) & 0xFF) != 0;
    const GuestAddress mutant = AllocateZeroed(0x20, 16);
    if (mutant == 0)
    {
        call.Return(kStatusNoMemory, 3);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    memory.Write8(mutant + L::kHeaderType, static_cast<uint8_t>(ObjectType::kMutant));
    memory.Write8(mutant + L::kHeaderSize, 8);
    memory.Write32(mutant + L::kHeaderSignalState, owned ? 0 : 1);
    memory.Write32(mutant + kMutantOwnerThread, owned ? CurrentEthread() : 0);
    auto object = std::make_shared<KernelObject>();
    object->kind = KernelObject::Kind::kMutant;
    object->object = mutant;
    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    memory.Write32(handle_out, handle);
    call.Return(kStatusSuccess, 3);
}

// NTSTATUS NtReleaseMutant(HANDLE MutantHandle, PLONG PreviousCount)
void NtReleaseMutant()
{
    HleCall call;
    const GuestAddress mutant = ObjectFromHandle(call.Arg(0));
    if (mutant == 0)
    {
        call.Return(kStatusInvalidHandle, 2);
        return;
    }
    int32_t previous = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        if (Memory().Read32(mutant + kMutantOwnerThread) != CurrentEthread())
        {
            call.Return(kStatusMutantNotOwned, 2);
            return;
        }
        previous = SignalState(mutant);
        SetSignalState(mutant, previous + 1);
        if (previous + 1 > 0)
        {
            Memory().Write32(mutant + kMutantOwnerThread, 0);
        }
        NotifyWaitersLocked();
    }
    if (call.Arg(1) != 0)
    {
        Memory().Write32(call.Arg(1), static_cast<uint32_t>(previous));
    }
    call.Return(kStatusSuccess, 2);
}

// --- Timer ---------------------------------------------------------------

// KTIMER: header (0x10), DueTime (+0x10, 8 bytes), TimerListEntry (+0x18),
// Dpc (+0x20), Period (+0x24).
constexpr uint32_t kTimerDueTime = 0x10;
constexpr uint32_t kTimerPeriod = 0x24;

// VOID KeInitializeTimerEx(PKTIMER Timer, TIMER_TYPE Type)
void KeInitializeTimerEx()
{
    HleCall call;
    const GuestAddress timer = call.Arg(0);
    runtime::GuestMemory& memory = Memory();
    memory.Fill(timer, 0, 0x28);
    memory.Write8(timer + L::kHeaderType, static_cast<uint8_t>(call.Arg(1) == 0 ? ObjectType::kNotificationTimer
                                                                                  : ObjectType::kSynchronizationTimer));
    memory.Write8(timer + L::kHeaderSize, 10);
    memory.Write32(timer + L::kHeaderWaitListHead, timer + L::kHeaderWaitListHead);
    memory.Write32(timer + L::kHeaderWaitListHead + 4, timer + L::kHeaderWaitListHead);
    call.Return(0, 2);
}

// BOOLEAN KeSetTimer(PKTIMER Timer, LARGE_INTEGER DueTime, PKDPC Dpc)
//
// The timer is armed but never fires: no DPC delivery exists yet. Logged so a
// title that depends on it is visible.
void KeSetTimer()
{
    HleCall call;
    const GuestAddress timer = call.Arg(0);
    Memory().Write32(timer + kTimerDueTime, call.Arg(1));
    Memory().Write32(timer + kTimerDueTime + 4, call.Arg(2));
    Memory().Write32(timer + kTimerPeriod, 0);
    TraceOnce("KeSetTimer", "armed; timers do not fire yet");
    call.Return(0, 4);
}

// BOOLEAN KeCancelTimer(PKTIMER Timer)
void KeCancelTimer()
{
    HleCall call;
    call.Return(0, 1);
}

// --- Waits ---------------------------------------------------------------

// NTSTATUS KeWaitForSingleObject(PVOID Object, KWAIT_REASON WaitReason,
//     KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
void KeWaitForSingleObject()
{
    HleCall call;
    const std::vector<GuestAddress> objects{call.Arg(0)};
    const Deadline deadline = DeadlineFromTimeout(call.Arg(4));
    const uint32_t status = WaitForObjects(objects, false, deadline);
    call.Return(status, 5);
}

void WaitForHandle(int stack_args, uint32_t handle, GuestAddress timeout)
{
    const GuestAddress object = ObjectFromHandle(handle);
    if (object == 0)
    {
        HleCall().Return(kStatusInvalidHandle, stack_args);
        return;
    }
    const std::vector<GuestAddress> objects{object};
    const uint32_t status = WaitForObjects(objects, false, DeadlineFromTimeout(timeout));
    HleCall().Return(status, stack_args);
}

// NTSTATUS NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
void NtWaitForSingleObject()
{
    HleCall call;
    WaitForHandle(3, call.Arg(0), call.Arg(2));
}

// NTSTATUS NtWaitForSingleObjectEx(HANDLE Handle, KPROCESSOR_MODE WaitMode,
//     BOOLEAN Alertable, PLARGE_INTEGER Timeout)
void NtWaitForSingleObjectEx()
{
    HleCall call;
    WaitForHandle(4, call.Arg(0), call.Arg(3));
}

// NTSTATUS NtWaitForMultipleObjectsEx(ULONG Count, HANDLE Handles[], WAIT_TYPE WaitType,
//     KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER Timeout)
void NtWaitForMultipleObjectsEx()
{
    HleCall call;
    const uint32_t count = call.Arg(0);
    const GuestAddress handles = call.Arg(1);
    const bool wait_all = call.Arg(2) == 0;  // WaitAll = 0, WaitAny = 1
    if (count == 0 || count > kMaximumWaitObjects)
    {
        call.Return(kStatusInvalidParameter, 6);
        return;
    }
    std::vector<GuestAddress> objects;
    for (uint32_t i = 0; i < count; ++i)
    {
        const GuestAddress object = ObjectFromHandle(Memory().Read32(handles + i * 4));
        if (object == 0)
        {
            call.Return(kStatusInvalidHandle, 6);
            return;
        }
        objects.push_back(object);
    }
    const uint32_t status = WaitForObjects(objects, wait_all, DeadlineFromTimeout(call.Arg(5)));
    call.Return(status, 6);
}

}  // namespace

Deadline DeadlineFromTimeout(GuestAddress timeout_pointer)
{
    if (timeout_pointer == 0)
    {
        return std::nullopt;
    }
    const uint64_t raw = static_cast<uint64_t>(Memory().Read32(timeout_pointer)) |
                         (static_cast<uint64_t>(Memory().Read32(timeout_pointer + 4)) << 32);
    const int64_t value = static_cast<int64_t>(raw);
    const auto now = std::chrono::steady_clock::now();
    if (value <= 0)
    {
        return now + std::chrono::nanoseconds(-value * 100);
    }
    const int64_t until = value - static_cast<int64_t>(SystemTimeNt());
    return now + std::chrono::nanoseconds(until > 0 ? until * 100 : 0);
}

void SignalThreadExit(GuestAddress ethread, uint32_t exit_status)
{
    std::lock_guard<std::mutex> lock(State().dispatcher);
    Memory().Write32(ethread + L::kEthreadExitStatus, exit_status);
    Memory().Write8(ethread + L::kThreadHasTerminated, 1);
    SetSignalState(ethread, 1);
    NotifyWaitersLocked();
}

void RegisterSyncExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "KeSetEvent", &KeSetEvent);
    registry.AddExport("kernel", "NtCreateEvent", &NtCreateEvent);
    registry.AddExport("kernel", "NtSetEvent", &NtSetEvent);
    registry.AddExport("kernel", "NtClearEvent", &NtClearEvent);
    registry.AddExport("kernel", "NtPulseEvent", &NtPulseEvent);
    registry.AddExport("kernel", "NtCreateSemaphore", &NtCreateSemaphore);
    registry.AddExport("kernel", "NtReleaseSemaphore", &NtReleaseSemaphore);
    registry.AddExport("kernel", "NtCreateMutant", &NtCreateMutant);
    registry.AddExport("kernel", "NtReleaseMutant", &NtReleaseMutant);
    registry.AddExport("kernel", "KeInitializeTimerEx", &KeInitializeTimerEx);
    registry.AddExport("kernel", "KeSetTimer", &KeSetTimer);
    registry.AddExport("kernel", "KeCancelTimer", &KeCancelTimer);
    registry.AddExport("kernel", "KeWaitForSingleObject", &KeWaitForSingleObject);
    registry.AddExport("kernel", "NtWaitForSingleObject", &NtWaitForSingleObject);
    registry.AddExport("kernel", "NtWaitForSingleObjectEx", &NtWaitForSingleObjectEx);
    registry.AddExport("kernel", "NtWaitForMultipleObjectsEx", &NtWaitForMultipleObjectsEx);
}

}  // namespace rexbox::hle::kernel
