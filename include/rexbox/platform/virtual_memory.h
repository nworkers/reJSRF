// Platform contract: host virtual memory.
//
// The platform layer knows none of the runtime's types. Callers pass plain
// sizes and addresses; each OS implements this in src/platform/<os>/.

#ifndef REXBOX_PLATFORM_VIRTUAL_MEMORY_H
#define REXBOX_PLATFORM_VIRTUAL_MEMORY_H

#include <cstddef>

namespace rexbox::platform
{

enum class PageAccess
{
    kNone,
    kRead,
    kReadWrite,
    kReadExecute,
    kReadWriteExecute,
};

// Reserves and commits `size` bytes of zeroed read/write memory. When
// `preferred_address` is non-null the OS is asked for that exact address
// first; a null return means the reservation failed (at the preferred address
// if one was given and `require_preferred` is true, otherwise anywhere).
// With `track_writes` the OS records which pages are written, for
// TakeWrittenPages; where it cannot, the block is allocated untracked.
void* AllocateVirtualMemory(size_t size, void* preferred_address, bool require_preferred,
                            bool track_writes = false);

// Releases a block returned by AllocateVirtualMemory. `size` must be the size
// passed to it.
void FreeVirtualMemory(void* address, size_t size);

// The pages of [address, address + size) written since the block was
// allocated or since the last call that covered them, and clears that record.
// `address` and `size` are page aligned and lie in one block allocated with
// `track_writes`. Up to `capacity` page addresses go to `pages` and their
// number to `*count`. Returns false when the written pages cannot be told
// (no tracking on this OS or block, more than `capacity` pages, a failure);
// the caller must then treat every page in the range as written.
bool TakeWrittenPages(void* address, size_t size, void** pages, size_t capacity, size_t* count);

// Changes the access of committed pages. Returns false on failure.
bool ProtectVirtualMemory(void* address, size_t size, PageAccess access);

// The host page size in bytes.
size_t PageSize();

}  // namespace rexbox::platform

#endif  // REXBOX_PLATFORM_VIRTUAL_MEMORY_H
