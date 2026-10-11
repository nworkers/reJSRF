// Internal to rexbox_recomp: state shared between the C callbacks and the
// C++ runtime API.

#ifndef REXBOX_RECOMP_RECOMP_BRIDGE_H
#define REXBOX_RECOMP_RECOMP_BRIDGE_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>

namespace rexbox::runtime
{
class HleRegistry;
}

namespace rexbox::recomp
{

// Codes passed to platform::RaiseStop.
constexpr uint32_t kStopUnimplementedKernel = 1;
constexpr uint32_t kStopUnresolvedCall = 2;
constexpr uint32_t kStopThreadExit = 3;

void AttachBridge(runtime::GuestMemory* memory, runtime::HleRegistry* registry);

// The HLE binding at `va`, or nullptr (what recomp_lookup_manual returns).
using BridgeFunction = void (*)();
BridgeFunction RecompLookupManual(uint32_t va);

// The VA that caused the last stop on this thread; clears it.
runtime::GuestAddress TakeStopTarget();

}  // namespace rexbox::recomp

#endif  // REXBOX_RECOMP_RECOMP_BRIDGE_H
