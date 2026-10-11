// Object manager services: handle close and reference counting.

#include "kernel_internal.h"

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;

// NTSTATUS NtClose(HANDLE Handle)
void NtClose()
{
    HleCall call;
    const uint32_t handle = call.Arg(0);
    std::shared_ptr<KernelObject> object;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        const auto it = State().handles.find(handle);
        if (it != State().handles.end())
        {
            object = it->second;
            State().handles.erase(it);
        }
    }
    if (object == nullptr)
    {
        call.Return(kStatusInvalidHandle, 1);
        return;
    }
    if (object.use_count() == 1)
    {
        object->stream.reset();
        if (object->delete_on_close)
        {
            std::error_code ec;
            std::filesystem::remove(object->host_path, ec);
        }
    }
    call.Return(kStatusSuccess, 1);
}

// NTSTATUS ObReferenceObjectByHandle(HANDLE Handle, POBJECT_TYPE ObjectType, PVOID* ReturnedObject)
//
// Returns the dispatcher object's guest address; object bodies of other kinds
// (files, devices) are not modelled in guest memory yet.
void ObReferenceObjectByHandle()
{
    HleCall call;
    const uint32_t handle = call.Arg(0);
    const GuestAddress out = call.Arg(2);
    GuestAddress body = 0;
    if (handle == 0xFFFFFFFE)
    {
        KernelThread* thread = CurrentThread();
        body = thread != nullptr ? thread->ethread : 0;
    }
    else if (const std::shared_ptr<KernelObject> object = LookupHandle(handle))
    {
        body = object->object;
    }
    if (body == 0)
    {
        call.Return(kStatusInvalidHandle, 3);
        return;
    }
    Memory().Write32(out, body);
    call.Return(kStatusSuccess, 3);
}

// VOID FASTCALL ObfDereferenceObject(PVOID Object) -- object in ecx.
void ObfDereferenceObject()
{
    HleCall().Return(0, 0);
}

// VOID FASTCALL ObfReferenceObject(PVOID Object) -- object in ecx.
void ObfReferenceObject()
{
    HleCall().Return(0, 0);
}

}  // namespace

void RegisterObjectExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "NtClose", &NtClose);
    registry.AddExport("kernel", "ObReferenceObjectByHandle", &ObReferenceObjectByHandle);
    registry.AddExport("kernel", "ObfDereferenceObject", &ObfDereferenceObject);
    registry.AddExport("kernel", "ObfReferenceObject", &ObfReferenceObject);
}

}  // namespace rexbox::hle::kernel
