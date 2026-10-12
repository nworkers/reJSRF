#include "rexbox/runtime/hle_registry.h"

namespace rexbox::runtime
{

bool HleRegistry::AddExport(std::string_view module, std::string_view symbol, HleThunk thunk)
{
    std::string key(symbol);
    const auto existing = export_index_.find(key);
    if (existing != export_index_.end())
    {
        exports_[existing->second] = HleExport{std::string(module), key, thunk};
        return false;
    }
    export_index_.emplace(key, exports_.size());
    exports_.push_back(HleExport{std::string(module), std::move(key), thunk});
    return true;
}

bool HleRegistry::BindGuestAddress(GuestAddress address, std::string_view symbol)
{
    const auto it = export_index_.find(std::string(symbol));
    if (it == export_index_.end())
    {
        return false;
    }
    bindings_[address] = it->second;
    return true;
}

const HleExport* HleRegistry::FindExport(std::string_view symbol) const
{
    const auto it = export_index_.find(std::string(symbol));
    return it == export_index_.end() ? nullptr : &exports_[it->second];
}

void HleRegistry::BindVariable(std::string_view name, GuestAddress address)
{
    variables_[std::string(name)] = address;
}

GuestAddress HleRegistry::FindVariable(std::string_view name) const
{
    const auto it = variables_.find(std::string(name));
    return it == variables_.end() ? 0 : it->second;
}

HleThunk HleRegistry::Lookup(GuestAddress address) const
{
    const auto it = bindings_.find(address);
    return it == bindings_.end() ? nullptr : exports_[it->second].thunk;
}

}  // namespace rexbox::runtime
