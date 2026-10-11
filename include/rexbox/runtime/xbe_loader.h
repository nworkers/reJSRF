// Places an XBE image into guest memory the way the Xbox kernel's loader does:
// headers at the base address, each section at its virtual address with its
// zero-filled tail.

#ifndef REXBOX_RUNTIME_XBE_LOADER_H
#define REXBOX_RUNTIME_XBE_LOADER_H

#include <string>

namespace rexbox::xbe
{
class XbeImage;
}

namespace rexbox::runtime
{

class GuestMemory;

bool LoadXbeIntoGuestMemory(const xbe::XbeImage& image, GuestMemory& memory, std::string* error);

}  // namespace rexbox::runtime

#endif  // REXBOX_RUNTIME_XBE_LOADER_H
