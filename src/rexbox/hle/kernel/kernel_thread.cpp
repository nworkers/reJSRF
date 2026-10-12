// Guest threads: KPCR/ETHREAD construction, creation, termination,
// suspension and priority.

#include "kernel_internal.h"

#include "rexbox/hle/kernel/kernel_hle.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>
#include <thread>

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;
namespace L = layout;

thread_local KernelThread* t_current = nullptr;

// Guest stacks smaller than this are raised to it: lifted code keeps its
// frames on the guest stack exactly as the original did, but titles may ask
// for the Xbox's small kernel default.
constexpr uint32_t kMinimumGuestStack = 64u * 1024u;

uint32_t RoundUpPage(uint32_t size)
{
    return (size + runtime::GuestHeap::kPageSize - 1) & ~(runtime::GuestHeap::kPageSize - 1);
}

struct ThreadLaunch
{
    std::shared_ptr<KernelThread> thread;
};

void OnThreadStart(void* context)
{
    KernelThread* thread = static_cast<ThreadLaunch*>(context)->thread.get();
    t_current = thread;
    std::unique_lock<std::mutex> lock(thread->gate);
    thread->resumed.wait(lock, [thread] { return thread->suspend_count <= 0; });
}

void OnThreadExit(void* context, uint32_t exit_code)
{
    auto* launch = static_cast<ThreadLaunch*>(context);
    SignalThreadExit(launch->thread->ethread, exit_code);
    t_current = nullptr;
    delete launch;
}

// Builds a guest thread's kernel structures and stack, pushes `arguments`
// (first argument at the lowest address) and a zero return address, and
// starts it at `function`.
std::shared_ptr<KernelThread> CreateGuestThread(GuestAddress function, const uint32_t* arguments, int argument_count,
                                                uint32_t stack_size, uint32_t tls_size, bool suspended,
                                                std::string* error)
{
    runtime::GuestMemory& memory = Memory();
    auto thread = std::make_shared<KernelThread>();

    const uint32_t stack_bytes = RoundUpPage(std::max(stack_size, kMinimumGuestStack));
    const uint32_t tls_bytes = (tls_size + 15u) & ~15u;
    const uint32_t block_bytes = RoundUpPage(stack_bytes + tls_bytes);
    thread->stack_block = AllocateZeroed(block_bytes, runtime::GuestHeap::kPageSize, true);
    thread->kpcr = AllocateZeroed(L::kPcrBlockSize + L::kEthreadSize, runtime::GuestHeap::kPageSize, true);
    if (thread->stack_block == 0 || thread->kpcr == 0)
    {
        *error = "guest memory exhausted creating a thread";
        return nullptr;
    }
    thread->ethread = thread->kpcr + L::kPcrBlockSize;

    // The TLS area sits at the very top; StackBase points just above it, which
    // is where the title's negative TLS index expects it (design i003 section 2).
    const GuestAddress top = thread->stack_block + block_bytes;
    const GuestAddress tls_data = top - tls_bytes;
    const GuestAddress stack_limit = thread->stack_block;

    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        thread->id = State().next_thread_id++;
        State().threads[thread->id] = thread;
    }

    const GuestAddress pcr = thread->kpcr;
    memory.Write32(pcr + L::kPcrExceptionList, 0xFFFFFFFF);
    memory.Write32(pcr + L::kPcrStackBase, top);
    memory.Write32(pcr + L::kPcrStackLimit, stack_limit);
    memory.Write32(pcr + L::kPcrSelf, pcr);
    memory.Write32(pcr + L::kPcrSelfPcr, pcr);
    memory.Write32(pcr + L::kPcrPrcb, pcr + L::kPcrPrcbData);
    memory.Write8(pcr + L::kPcrIrql, 0);
    memory.Write32(pcr + L::kPcrCurrentThread, thread->ethread);

    const GuestAddress ethread = thread->ethread;
    memory.Write8(ethread + L::kHeaderType, static_cast<uint8_t>(ObjectType::kThread));
    memory.Write8(ethread + L::kHeaderSize, static_cast<uint8_t>(L::kEthreadSize / 4));
    memory.Write32(ethread + L::kHeaderSignalState, 0);
    memory.Write32(ethread + L::kHeaderWaitListHead, ethread + L::kHeaderWaitListHead);
    memory.Write32(ethread + L::kHeaderWaitListHead + 4, ethread + L::kHeaderWaitListHead);
    memory.Write32(ethread + L::kThreadStackBase, top);
    memory.Write32(ethread + L::kThreadStackLimit, stack_limit);
    memory.Write32(ethread + L::kThreadTlsData, tls_size != 0 ? tls_data : 0);
    memory.Write32(ethread + L::kEthreadUniqueThread, thread->id);

    GuestAddress esp = (tls_data - 16) & ~15u;
    for (int i = argument_count - 1; i >= 0; --i)
    {
        esp -= 4;
        memory.Write32(esp, arguments[i]);
    }
    esp -= 4;
    memory.Write32(esp, 0);  // return address: a return ends the thread
    memory.Write32(ethread + L::kThreadKernelStack, esp);

    thread->suspend_count = suspended ? 1 : 0;
    runtime::GuestThreadStart start;
    start.function = function;
    start.stack_pointer = esp;
    start.fs_base = pcr;
    start.thread_id = thread->id;
    start.on_start = &OnThreadStart;
    start.on_exit = &OnThreadExit;
    start.context = new ThreadLaunch{thread};
    if (!runtime::Env().executor->StartThread(start, error))
    {
        delete static_cast<ThreadLaunch*>(start.context);
        return nullptr;
    }
    return thread;
}

std::shared_ptr<KernelThread> FindThreadByEthread(GuestAddress ethread)
{
    std::lock_guard<std::mutex> lock(State().dispatcher);
    for (const auto& [id, thread] : State().threads)
    {
        if (thread->ethread == ethread)
        {
            return thread;
        }
    }
    return nullptr;
}

std::shared_ptr<KernelThread> ThreadFromHandle(uint32_t handle)
{
    const std::shared_ptr<KernelObject> object = LookupHandle(handle);
    if (object == nullptr || object->kind != KernelObject::Kind::kThread)
    {
        return nullptr;
    }
    return FindThreadByEthread(object->object);
}

// NTSTATUS PsCreateSystemThreadEx(PHANDLE ThreadHandle, ULONG ThreadExtensionSize,
//     ULONG KernelStackSize, ULONG TlsDataSize, PHANDLE ThreadId, PKSTART_ROUTINE StartRoutine,
//     PVOID StartContext, BOOLEAN CreateSuspended, BOOLEAN DebuggerThread,
//     PKSYSTEM_ROUTINE SystemRoutine)
void PsCreateSystemThreadEx()
{
    HleCall call;
    const GuestAddress handle_out = call.Arg(0);
    const uint32_t stack_size = call.Arg(2);
    const uint32_t tls_size = call.Arg(3);
    const GuestAddress id_out = call.Arg(4);
    const GuestAddress start_routine = call.Arg(5);
    const GuestAddress start_context = call.Arg(6);
    const bool suspended = (call.Arg(7) & 0xFF) != 0;
    const GuestAddress system_routine = call.Arg(9);

    // With a system routine the kernel calls SystemRoutine(StartRoutine,
    // StartContext); otherwise StartRoutine(StartContext).
    const uint32_t with_system[] = {start_routine, start_context};
    const uint32_t without_system[] = {start_context};
    std::string error;
    const std::shared_ptr<KernelThread> thread =
        system_routine != 0
            ? CreateGuestThread(system_routine, with_system, 2, stack_size, tls_size, suspended, &error)
            : CreateGuestThread(start_routine, without_system, 1, stack_size, tls_size, suspended, &error);
    if (thread == nullptr)
    {
        TraceOnce("PsCreateSystemThreadEx", error);
        call.Return(kStatusNoMemory, 10);
        return;
    }
    char routine[16];
    std::snprintf(routine, sizeof(routine), "%08X", system_routine != 0 ? system_routine : start_routine);
    TraceOnce("PsCreateSystemThreadEx", "thread " + std::to_string(thread->id) + " routine " + routine);

    auto object = std::make_shared<KernelObject>();
    object->kind = KernelObject::Kind::kThread;
    object->object = thread->ethread;
    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    if (handle_out != 0)
    {
        Memory().Write32(handle_out, handle);
    }
    if (id_out != 0)
    {
        Memory().Write32(id_out, thread->id);
    }
    call.Return(kStatusSuccess, 10);
}

// VOID PsTerminateSystemThread(NTSTATUS ExitStatus)
void PsTerminateSystemThread()
{
    const uint32_t status = HleCall().Arg(0);
    runtime::Env().executor->ExitThread(status);
}

// NTSTATUS KeDelayExecutionThread(KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER Interval)
// An alertable user-mode delay ends early for a user APC, runs it and
// returns STATUS_USER_APC.
void KeDelayExecutionThread()
{
    HleCall call;
    const Deadline deadline = DeadlineFromTimeout(call.Arg(2));
    const bool alertable = (call.Arg(1) & 0xFF) != 0 && (call.Arg(0) & 0xFF) == kUserMode;
    if (alertable)
    {
        KernelThread& thread = ApcThread();
        bool pending = false;
        {
            KernelState& state = State();
            std::unique_lock<std::mutex> lock(state.dispatcher);
            const auto has_apc = [&thread] { return !thread.user_apcs.empty(); };
            pending = deadline ? state.signalled.wait_until(lock, *deadline, has_apc) : has_apc();
        }
        if (pending)
        {
            DeliverUserApcs(thread);
            HleCall().Return(kStatusUserApc, 3);
            return;
        }
        if (!deadline)
        {
            std::this_thread::yield();
        }
        call.Return(kStatusSuccess, 3);
        return;
    }
    if (deadline)
    {
        std::this_thread::sleep_until(*deadline);
    }
    else
    {
        std::this_thread::yield();
    }
    call.Return(kStatusSuccess, 3);
}

// LONG KeQueryBasePriorityThread(PKTHREAD Thread)
void KeQueryBasePriorityThread()
{
    HleCall call;
    const GuestAddress thread = call.Arg(0);
    call.Return(static_cast<uint32_t>(static_cast<int8_t>(Memory().Read8(thread + L::kThreadBasePriority))), 1);
}

// LONG KeSetBasePriorityThread(PKTHREAD Thread, LONG Increment)
void KeSetBasePriorityThread()
{
    HleCall call;
    const GuestAddress thread = call.Arg(0);
    const uint8_t previous = Memory().Read8(thread + L::kThreadBasePriority);
    Memory().Write8(thread + L::kThreadBasePriority, static_cast<uint8_t>(call.Arg(1)));
    call.Return(static_cast<uint32_t>(static_cast<int8_t>(previous)), 2);
}

// LOGICAL KeSetDisableBoostThread(PKTHREAD Thread, LOGICAL Disable)
void KeSetDisableBoostThread()
{
    HleCall call;
    const GuestAddress thread = call.Arg(0);
    const uint8_t previous = Memory().Read8(thread + L::kThreadDisableBoost);
    Memory().Write8(thread + L::kThreadDisableBoost, static_cast<uint8_t>(call.Arg(1) != 0));
    call.Return(previous, 2);
}

// NTSTATUS NtResumeThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount)
void NtResumeThread()
{
    HleCall call;
    const std::shared_ptr<KernelThread> thread = ThreadFromHandle(call.Arg(0));
    if (thread == nullptr)
    {
        call.Return(kStatusInvalidHandle, 2);
        return;
    }
    int previous = 0;
    {
        std::lock_guard<std::mutex> lock(thread->gate);
        previous = thread->suspend_count;
        thread->suspend_count = std::max(0, previous - 1);
    }
    thread->resumed.notify_all();
    if (call.Arg(1) != 0)
    {
        Memory().Write32(call.Arg(1), static_cast<uint32_t>(previous));
    }
    call.Return(kStatusSuccess, 2);
}

// NTSTATUS NtSuspendThread(HANDLE ThreadHandle, PULONG PreviousSuspendCount)
//
// Only a thread that has not started running guest code can actually be held;
// suspending a running thread would need a safe point the lifted code does not
// offer. The count is kept and reported either way.
void NtSuspendThread()
{
    HleCall call;
    const std::shared_ptr<KernelThread> thread = ThreadFromHandle(call.Arg(0));
    if (thread == nullptr)
    {
        call.Return(kStatusInvalidHandle, 2);
        return;
    }
    int previous = 0;
    {
        std::lock_guard<std::mutex> lock(thread->gate);
        previous = thread->suspend_count++;
    }
    TraceOnce("NtSuspendThread", "running threads are not stopped");
    if (call.Arg(1) != 0)
    {
        Memory().Write32(call.Arg(1), static_cast<uint32_t>(previous));
    }
    call.Return(kStatusSuccess, 2);
}

}  // namespace

KernelThread* CurrentThread()
{
    return t_current;
}

void RegisterThreadExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "PsCreateSystemThreadEx", &PsCreateSystemThreadEx);
    registry.AddExport("kernel", "PsTerminateSystemThread", &PsTerminateSystemThread);
    registry.AddExport("kernel", "KeDelayExecutionThread", &KeDelayExecutionThread);
    registry.AddExport("kernel", "KeQueryBasePriorityThread", &KeQueryBasePriorityThread);
    registry.AddExport("kernel", "KeSetBasePriorityThread", &KeSetBasePriorityThread);
    registry.AddExport("kernel", "KeSetDisableBoostThread", &KeSetDisableBoostThread);
    registry.AddExport("kernel", "NtResumeThread", &NtResumeThread);
    registry.AddExport("kernel", "NtSuspendThread", &NtSuspendThread);
}

bool StartInitialThread(const xbe::XbeImage& image, std::string* error)
{
    // The XBE loader gives the first thread the image's TLS block, laid out the
    // way XAPI lays it out for later threads: a self pointer, then the data.
    uint32_t tls_size = 0;
    uint32_t template_size = 0;
    if (image.tls())
    {
        template_size = image.tls()->raw_data_end - image.tls()->raw_data_start;
        tls_size = ((template_size + image.tls()->zero_fill_size + 15u) & ~15u) + 4u;
    }
    const std::shared_ptr<KernelThread> thread =
        CreateGuestThread(image.entry_point(), nullptr, 0, image.stack_commit(), tls_size, true, error);
    if (thread == nullptr)
    {
        return false;
    }
    if (tls_size != 0)
    {
        runtime::GuestMemory& memory = Memory();
        const GuestAddress tls_data = memory.Read32(thread->ethread + layout::kThreadTlsData);
        memory.Write32(tls_data, tls_data + 4);
        if (template_size != 0)
        {
            std::memcpy(memory.Translate(tls_data + 4), memory.Translate(image.tls()->raw_data_start), template_size);
        }
    }
    {
        std::lock_guard<std::mutex> lock(thread->gate);
        thread->suspend_count = 0;
    }
    thread->resumed.notify_all();
    return true;
}

}  // namespace rexbox::hle::kernel
