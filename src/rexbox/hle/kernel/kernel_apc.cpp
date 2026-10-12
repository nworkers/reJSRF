// User-mode APCs: queued on a thread when I/O with a completion routine
// finishes, run by the thread's next alertable user-mode wait
// (docs/design/20261012-i019-user-apc.md).

#include "kernel_internal.h"

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;

// Host threads that are not guest threads (tools) queue and deliver here.
KernelThread& HostThreadRecord()
{
    thread_local KernelThread record;
    return record;
}

// VOID NtUserIoApcDispatcher(PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG Reserved)
// XAPI's ReadFileEx/WriteFileEx pass this as the APC routine and their
// LPOVERLAPPED_COMPLETION_ROUTINE as the context. The OVERLAPPED begins with
// Internal (Status) and InternalHigh (Information), so it is the status block.
void NtUserIoApcDispatcher()
{
    HleCall call;
    const GuestAddress completion = call.Arg(0);
    const GuestAddress io_status = call.Arg(1);
    const uint32_t status = Memory().Read32(io_status);
    const bool failed = IsNtError(status);
    const uint32_t arguments[3] = {failed ? NtStatusToDosError(status) : 0, failed ? 0 : Memory().Read32(io_status + 4),
                                   io_status};
    bool called = false;
    runtime::Env().executor->CallGuest(completion, arguments, 3, &called);
    if (!called)
    {
        TraceOnce("NtUserIoApcDispatcher", "completion routine has no code");
    }
    call.Return(0, 3);
}

}  // namespace

KernelThread& ApcThread()
{
    KernelThread* thread = CurrentThread();
    return thread != nullptr ? *thread : HostThreadRecord();
}

void QueueUserApc(KernelThread& thread, const UserApc& apc)
{
    KernelState& state = State();
    std::lock_guard<std::mutex> lock(state.dispatcher);
    thread.user_apcs.push_back(apc);
    state.signalled.notify_all();
}

uint32_t DeliverUserApcs(KernelThread& thread)
{
    // APCs a routine queues run in the same delivery, as NT's return path
    // (NtContinue with TestAlert) does.
    uint32_t delivered = 0;
    for (;;)
    {
        UserApc apc;
        {
            std::lock_guard<std::mutex> lock(State().dispatcher);
            if (thread.user_apcs.empty())
            {
                return delivered;
            }
            apc = thread.user_apcs.front();
            thread.user_apcs.pop_front();
        }
        const uint32_t arguments[3] = {apc.argument1, apc.argument2, apc.argument3};
        bool called = false;
        runtime::Env().executor->CallGuest(apc.routine, arguments, 3, &called);
        if (!called)
        {
            TraceOnce("user APC", "routine has no code");
        }
        ++delivered;
    }
}

void RegisterApcExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "NtUserIoApcDispatcher", &NtUserIoApcDispatcher);
}

}  // namespace rexbox::hle::kernel
