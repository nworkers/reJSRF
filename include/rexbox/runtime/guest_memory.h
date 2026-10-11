// Guest address space.
//
// The guest is a 32-bit Xbox program; every guest pointer is a 32-bit virtual
// address. The runtime backs the guest's RAM with one host block and
// translates guest addresses as `host = base + guest` (the "based" address
// model, used on every host so 32- and 64-bit builds share one code path; see
// docs/design/20261012-i001-project-bootstrap.md).

#ifndef REXBOX_RUNTIME_GUEST_MEMORY_H
#define REXBOX_RUNTIME_GUEST_MEMORY_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rexbox::runtime
{

using GuestAddress = uint32_t;

class GuestMemory
{
public:
    // Retail Xbox RAM.
    static constexpr size_t kRetailRamSize = 64u * 1024u * 1024u;

    GuestMemory() = default;
    ~GuestMemory();
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;

    // Reserves and commits `size` bytes covering guest addresses [0, size).
    bool Initialize(size_t size);
    void Shutdown();

    bool IsValid(GuestAddress address, size_t length) const;

    // Host pointer for a guest address. The caller must have checked range.
    uint8_t* Translate(GuestAddress address) const { return base_ + address; }

    // Little-endian scalar access; the caller must have checked the range.
    uint32_t Read32(GuestAddress address) const
    {
        uint32_t value;
        std::memcpy(&value, Translate(address), 4);
        return value;
    }
    uint16_t Read16(GuestAddress address) const
    {
        uint16_t value;
        std::memcpy(&value, Translate(address), 2);
        return value;
    }
    uint8_t Read8(GuestAddress address) const { return *Translate(address); }
    void Write32(GuestAddress address, uint32_t value) { std::memcpy(Translate(address), &value, 4); }
    void Write16(GuestAddress address, uint16_t value) { std::memcpy(Translate(address), &value, 2); }
    void Write8(GuestAddress address, uint8_t value) { *Translate(address) = value; }
    void Fill(GuestAddress address, uint8_t value, size_t length) { std::memset(Translate(address), value, length); }

    uint8_t* base() const { return base_; }
    size_t size() const { return size_; }

private:
    uint8_t* base_ = nullptr;
    size_t size_ = 0;
};

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_GUEST_MEMORY_H
