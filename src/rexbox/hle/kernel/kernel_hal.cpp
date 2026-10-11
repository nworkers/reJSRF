// HAL, interrupt, DPC, IRQL, AV, time and settings services.
//
// Interrupts and DPCs are accepted but never delivered: the hardware paths
// that would raise them belong to D3D8/DSOUND, which are to be replaced at the
// API level (design i003 section 4). Each such call is traced once.

#include "kernel_internal.h"

#include <thread>

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;
namespace L = layout;

uint8_t CurrentIrql()
{
    KernelThread* thread = CurrentThread();
    return thread != nullptr ? Memory().Read8(thread->kpcr + L::kPcrIrql) : 0;
}

void SetIrql(uint8_t irql)
{
    KernelThread* thread = CurrentThread();
    if (thread != nullptr)
    {
        Memory().Write8(thread->kpcr + L::kPcrIrql, irql);
    }
}

// --- IRQL ----------------------------------------------------------------

// KIRQL FASTCALL KfRaiseIrql(KIRQL NewIrql) -- ecx
void KfRaiseIrql()
{
    HleCall call;
    const uint8_t previous = CurrentIrql();
    SetIrql(static_cast<uint8_t>(call.Ecx()));
    call.Return(previous, 0);
}

// VOID FASTCALL KfLowerIrql(KIRQL NewIrql) -- ecx
void KfLowerIrql()
{
    HleCall call;
    SetIrql(static_cast<uint8_t>(call.Ecx()));
    call.Return(0, 0);
}

// KIRQL KeRaiseIrqlToDpcLevel(VOID)
void KeRaiseIrqlToDpcLevel()
{
    HleCall call;
    const uint8_t previous = CurrentIrql();
    SetIrql(2);
    call.Return(previous, 0);
}

// --- Time ----------------------------------------------------------------

// VOID KeQuerySystemTime(PLARGE_INTEGER CurrentTime)
void KeQuerySystemTime()
{
    HleCall call;
    const uint64_t now = SystemTimeNt();
    Memory().Write32(call.Arg(0), static_cast<uint32_t>(now));
    Memory().Write32(call.Arg(0) + 4, static_cast<uint32_t>(now >> 32));
    call.Return(0, 1);
}

// ULONGLONG KeQueryInterruptTime(VOID)
void KeQueryInterruptTime()
{
    HleCall().Return64(InterruptTime100ns(), 0);
}

// VOID KeStallExecutionProcessor(ULONG MicroSeconds)
void KeStallExecutionProcessor()
{
    HleCall call;
    std::this_thread::sleep_for(std::chrono::microseconds(call.Arg(0)));
    call.Return(0, 1);
}

// --- Interrupts and DPCs -------------------------------------------------

// VOID KeInitializeInterrupt(PKINTERRUPT Interrupt, PKSERVICE_ROUTINE ServiceRoutine,
//     PVOID ServiceContext, ULONG Vector, KIRQL Irql, KINTERRUPT_MODE InterruptMode,
//     BOOLEAN ShareVector)
void KeInitializeInterrupt()
{
    HleCall call;
    // KINTERRUPT: ServiceRoutine (+0), ServiceContext (+4), BusInterruptLevel
    // (+8), Irql (+0xC), Connected (+0x10), ShareVector (+0x11), Mode (+0x12).
    runtime::GuestMemory& memory = Memory();
    const GuestAddress interrupt = call.Arg(0);
    memory.Write32(interrupt + 0x00, call.Arg(1));
    memory.Write32(interrupt + 0x04, call.Arg(2));
    memory.Write32(interrupt + 0x08, call.Arg(3));
    memory.Write32(interrupt + 0x0C, call.Arg(4));
    memory.Write8(interrupt + 0x10, 0);
    TraceOnce("KeInitializeInterrupt", "vector " + std::to_string(call.Arg(3)) + " (never delivered)");
    call.Return(0, 7);
}

// BOOLEAN KeConnectInterrupt(PKINTERRUPT Interrupt)
void KeConnectInterrupt()
{
    HleCall call;
    Memory().Write8(call.Arg(0) + 0x10, 1);
    call.Return(1, 1);
}

// BOOLEAN KeDisconnectInterrupt(PKINTERRUPT Interrupt)
void KeDisconnectInterrupt()
{
    HleCall call;
    Memory().Write8(call.Arg(0) + 0x10, 0);
    call.Return(1, 1);
}

// BOOLEAN KeSynchronizeExecution(PKINTERRUPT Interrupt, PKSYNCHRONIZE_ROUTINE
//     SynchronizeRoutine, PVOID SynchronizeContext)
//
// No interrupt ever runs concurrently, so the routine is simply called.
void KeSynchronizeExecution()
{
    HleCall call;
    const GuestAddress routine = call.Arg(1);
    const uint32_t context = call.Arg(2);
    bool called = false;
    const uint32_t result = runtime::Env().executor->CallGuest(routine, &context, 1, &called);
    if (!called)
    {
        TraceOnce("KeSynchronizeExecution", "routine has no code");
    }
    HleCall().Return(result & 0xFF, 3);
}

// ULONG HalGetInterruptVector(ULONG BusInterruptLevel, PKIRQL Irql)
void HalGetInterruptVector()
{
    HleCall call;
    const uint32_t level = call.Arg(0);
    if (call.Arg(1) != 0)
    {
        Memory().Write8(call.Arg(1), static_cast<uint8_t>(26 - level));
    }
    call.Return(0x30 + level, 2);
}

// VOID KeInitializeDpc(PKDPC Dpc, PKDEFERRED_ROUTINE DeferredRoutine, PVOID DeferredContext)
void KeInitializeDpc()
{
    HleCall call;
    // KDPC: Type (+0), Inserted (+2), DpcListEntry (+4), DeferredRoutine (+0xC),
    // DeferredContext (+0x10), SystemArgument1/2 (+0x14/+0x18).
    runtime::GuestMemory& memory = Memory();
    const GuestAddress dpc = call.Arg(0);
    memory.Fill(dpc, 0, 0x1C);
    memory.Write16(dpc + 0x00, 0x13);  // DpcObject
    memory.Write32(dpc + 0x0C, call.Arg(1));
    memory.Write32(dpc + 0x10, call.Arg(2));
    call.Return(0, 3);
}

// BOOLEAN KeInsertQueueDpc(PRKDPC Dpc, PVOID SystemArgument1, PVOID SystemArgument2)
void KeInsertQueueDpc()
{
    HleCall call;
    TraceOnce("KeInsertQueueDpc", "queued but never run");
    call.Return(1, 3);
}

// BOOLEAN KeRemoveQueueDpc(PRKDPC Dpc)
void KeRemoveQueueDpc()
{
    HleCall().Return(0, 1);
}

// --- Bus, shutdown, sections ---------------------------------------------

// VOID HalReadWritePCISpace(ULONG BusNumber, ULONG SlotNumber, ULONG RegisterNumber,
//     PVOID Buffer, ULONG Length, BOOLEAN WritePCISpace)
void HalReadWritePCISpace()
{
    HleCall call;
    if ((call.Arg(5) & 0xFF) == 0)
    {
        Memory().Fill(call.Arg(3), 0, call.Arg(4));
    }
    TraceOnce("HalReadWritePCISpace", "bus " + std::to_string(call.Arg(0)) + " slot " + std::to_string(call.Arg(1)) +
                                          " reg " + std::to_string(call.Arg(2)));
    call.Return(0, 6);
}

// VOID HalRegisterShutdownNotification(PHAL_SHUTDOWN_REGISTRATION ShutdownRegistration, BOOLEAN Register)
void HalRegisterShutdownNotification()
{
    HleCall().Return(0, 2);
}

// BOOLEAN HalIsResetOrShutdownPending(VOID)
void HalIsResetOrShutdownPending()
{
    HleCall().Return(0, 0);
}

// NTSTATUS XeLoadSection(PXBE_SECTION_HEADER Section) -- every section is preloaded.
void XeLoadSection()
{
    HleCall().Return(kStatusSuccess, 1);
}

// NTSTATUS XeUnloadSection(PXBE_SECTION_HEADER Section)
void XeUnloadSection()
{
    HleCall().Return(kStatusSuccess, 1);
}

// --- AV ------------------------------------------------------------------

// PVOID AvGetSavedDataAddress(VOID)
void AvGetSavedDataAddress()
{
    HleCall().Return(0, 0);
}

// VOID AvSetSavedDataAddress(PVOID Address)
void AvSetSavedDataAddress()
{
    HleCall().Return(0, 1);
}

// VOID AvSendTVEncoderOption(PVOID RegisterBase, ULONG Option, ULONG Param, PULONG Result)
void AvSendTVEncoderOption()
{
    HleCall call;
    const uint32_t option = call.Arg(1);
    if (call.Arg(3) != 0)
    {
        // AV_QUERY_AV_CAPABILITIES (6): NTSC-M, 60 Hz, composite pack.
        // Provisional until D3D's use of the answer is analysed.
        Memory().Write32(call.Arg(3), option == 6 ? 0x00400101u : 0u);
    }
    TraceOnce("AvSendTVEncoderOption", "option " + std::to_string(option));
    call.Return(0, 4);
}

// ULONG AvSetDisplayMode(PVOID RegisterBase, ULONG Step, ULONG Mode, ULONG Format,
//     ULONG Pitch, ULONG FrameBuffer)
void AvSetDisplayMode()
{
    HleCall call;
    TraceOnce("AvSetDisplayMode", "mode " + std::to_string(call.Arg(2)));
    call.Return(0, 6);
}

// --- Settings and debug --------------------------------------------------

// NTSTATUS ExQueryNonVolatileSetting(ULONG ValueIndex, PULONG Type, PVOID Value,
//     ULONG ValueLength, PULONG ResultLength)
void ExQueryNonVolatileSetting()
{
    HleCall call;
    runtime::GuestMemory& memory = Memory();
    const uint32_t index = call.Arg(0);
    const GuestAddress value = call.Arg(2);
    const uint32_t length = call.Arg(3);
    uint32_t setting = 0;
    switch (index)
    {
    case 0x07:  // XC_LANGUAGE: English
        setting = 1;
        break;
    case 0x08:  // XC_VIDEO: no widescreen, no HD modes enabled
        setting = 0;
        break;
    case 0x09:  // XC_AUDIO: stereo
        setting = 0;
        break;
    case 0x103:  // XC_FACTORY_AV_REGION: NTSC-M, 60 Hz
        setting = 0x00400100;
        break;
    case 0x104:  // XC_FACTORY_GAME_REGION: North America
        setting = 1;
        break;
    default:
        TraceOnce("ExQueryNonVolatileSetting", "index " + std::to_string(index) + " answered with zeros");
        break;
    }
    if (call.Arg(1) != 0)
    {
        memory.Write32(call.Arg(1), 4);  // REG_DWORD
    }
    if (value != 0 && length != 0)
    {
        memory.Fill(value, 0, length);
        if (length >= 4)
        {
            memory.Write32(value, setting);
        }
    }
    if (call.Arg(4) != 0)
    {
        memory.Write32(call.Arg(4), length < 4 ? length : 4);
    }
    call.Return(kStatusSuccess, 5);
}

// ULONG DbgPrint(PCH Format, ...) -- cdecl; prints the format string as is.
void DbgPrint()
{
    HleCall call;
    std::string text;
    for (GuestAddress p = call.Arg(0); p != 0 && text.size() < 512; ++p)
    {
        const char ch = static_cast<char>(Memory().Read8(p));
        if (ch == 0)
        {
            break;
        }
        text.push_back(ch);
    }
    std::fprintf(stderr, "[guest] %s", text.c_str());
    call.Return(0, 0);
}

}  // namespace

void RegisterHalExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "KfRaiseIrql", &KfRaiseIrql);
    registry.AddExport("kernel", "KfLowerIrql", &KfLowerIrql);
    registry.AddExport("kernel", "KeRaiseIrqlToDpcLevel", &KeRaiseIrqlToDpcLevel);
    registry.AddExport("kernel", "KeQuerySystemTime", &KeQuerySystemTime);
    registry.AddExport("kernel", "KeQueryInterruptTime", &KeQueryInterruptTime);
    registry.AddExport("kernel", "KeStallExecutionProcessor", &KeStallExecutionProcessor);
    registry.AddExport("kernel", "KeInitializeInterrupt", &KeInitializeInterrupt);
    registry.AddExport("kernel", "KeConnectInterrupt", &KeConnectInterrupt);
    registry.AddExport("kernel", "KeDisconnectInterrupt", &KeDisconnectInterrupt);
    registry.AddExport("kernel", "KeSynchronizeExecution", &KeSynchronizeExecution);
    registry.AddExport("kernel", "HalGetInterruptVector", &HalGetInterruptVector);
    registry.AddExport("kernel", "KeInitializeDpc", &KeInitializeDpc);
    registry.AddExport("kernel", "KeInsertQueueDpc", &KeInsertQueueDpc);
    registry.AddExport("kernel", "KeRemoveQueueDpc", &KeRemoveQueueDpc);
    registry.AddExport("kernel", "HalReadWritePCISpace", &HalReadWritePCISpace);
    registry.AddExport("kernel", "HalRegisterShutdownNotification", &HalRegisterShutdownNotification);
    registry.AddExport("kernel", "HalIsResetOrShutdownPending", &HalIsResetOrShutdownPending);
    registry.AddExport("kernel", "XeLoadSection", &XeLoadSection);
    registry.AddExport("kernel", "XeUnloadSection", &XeUnloadSection);
    registry.AddExport("kernel", "AvGetSavedDataAddress", &AvGetSavedDataAddress);
    registry.AddExport("kernel", "AvSetSavedDataAddress", &AvSetSavedDataAddress);
    registry.AddExport("kernel", "AvSendTVEncoderOption", &AvSendTVEncoderOption);
    registry.AddExport("kernel", "AvSetDisplayMode", &AvSetDisplayMode);
    registry.AddExport("kernel", "ExQueryNonVolatileSetting", &ExQueryNonVolatileSetting);
    registry.AddExport("kernel", "DbgPrint", &DbgPrint);
}

}  // namespace rexbox::hle::kernel
