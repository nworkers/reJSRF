// HLE registry: the one place where title code meets the HLE layers.
//
// An Xbox title links the XDK libraries (XAPI, D3D8, DSOUND, ...) statically,
// so the only true import table is the kernel's. Everything else is a guest
// function at a title-specific address. The boundary therefore has two sides:
//
//   * HLE modules (src/rexbox/hle/<module>/) export host implementations by a
//     stable symbol name, e.g. {"d3d8", "D3DDevice_Clear"}. They know nothing
//     about any title.
//   * A title profile binds those names to the guest addresses where its copy
//     of the library function lives (found by function identification), and
//     the kernel module binds the thunk table's ordinals
//     (rexbox/hle/kernel/kernel_hle.h).
//
// Recompiled and decompiled title code never calls an HLE module directly; the
// dispatcher asks the registry whether a guest address is bound.

#ifndef REXBOX_RUNTIME_HLE_REGISTRY_H
#define REXBOX_RUNTIME_HLE_REGISTRY_H

#include "rexbox/runtime/guest_memory.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rexbox::runtime
{

// Same shape as a lifted guest function: arguments and results travel through
// the guest register and stack state, not the host signature.
using HleThunk = void (*)();

struct HleExport
{
    std::string module;
    std::string symbol;
    HleThunk thunk = nullptr;
};

class HleRegistry
{
public:
    // Called by each HLE module's Register*Hle(). A duplicate symbol replaces
    // the earlier entry and returns false.
    bool AddExport(std::string_view module, std::string_view symbol, HleThunk thunk);

    // Called by a title profile. Returns false if no module exports `symbol`.
    bool BindGuestAddress(GuestAddress address, std::string_view symbol);

    const HleExport* FindExport(std::string_view symbol) const;

    // The HLE implementation bound at `address`, or nullptr.
    HleThunk Lookup(GuestAddress address) const;

    size_t export_count() const { return exports_.size(); }
    size_t binding_count() const { return bindings_.size(); }

private:
    std::vector<HleExport> exports_;
    std::unordered_map<std::string, size_t> export_index_;
    std::unordered_map<GuestAddress, size_t> bindings_;
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_HLE_REGISTRY_H
