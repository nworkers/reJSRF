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
    if (address < kFlatLimit)
    {
        const size_t word = address / 64;
        if (bound_bits_.size() <= word)
        {
            bound_bits_.resize(word + 1, 0);
        }
        bound_bits_[word] |= uint64_t{1} << (address % 64);
    }
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
    if (address < kFlatLimit)
    {
        const size_t word = address / 64;
        if (word >= bound_bits_.size() || ((bound_bits_[word] >> (address % 64)) & 1) == 0)
        {
            return nullptr;
        }
    }
    const auto it = bindings_.find(address);
    return it == bindings_.end() ? nullptr : exports_[it->second].thunk;
}

}  // namespace rexbox::runtime
