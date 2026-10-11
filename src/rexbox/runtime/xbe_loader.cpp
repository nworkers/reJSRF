#include "rexbox/runtime/xbe_loader.h"

#include "rexbox/runtime/guest_memory.h"
#include "rexbox/xbe/xbe_image.h"

#include <algorithm>
#include <cstring>

namespace rexbox::runtime
{

bool LoadXbeIntoGuestMemory(const xbe::XbeImage& image, GuestMemory& memory, std::string* error)
{
    const auto fail = [error](std::string message)
    {
        if (error != nullptr)
        {
            *error = std::move(message);
        }
        return false;
    };

    if (!memory.IsValid(image.base_address(), image.size_of_headers()))
    {
        return fail("XBE headers do not fit in guest memory");
    }
    std::memcpy(memory.Translate(image.base_address()), image.bytes().data(), image.size_of_headers());

    for (const xbe::XbeSection& section : image.sections())
    {
        const uint32_t span = std::max(section.virtual_size, section.raw_size);
        if (!memory.IsValid(section.virtual_address, span))
        {
            return fail("section " + section.name + " does not fit in guest memory");
        }
        uint8_t* target = memory.Translate(section.virtual_address);
        const uint32_t copied = std::min(section.raw_size, section.virtual_size);
        std::memcpy(target, image.bytes().data() + section.raw_address, copied);
        std::memset(target + copied, 0, span - copied);
    }
    return true;
}

}  // namespace rexbox::runtime
