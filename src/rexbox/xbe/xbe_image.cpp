#include "rexbox/xbe/xbe_image.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace rexbox::xbe
{

namespace
{

constexpr uint32_t kXbeMagic = 0x48454258;  // "XBEH"

// Image header field offsets.
constexpr uint32_t kOffBaseAddress = 0x104;
constexpr uint32_t kOffSizeOfHeaders = 0x108;
constexpr uint32_t kOffSizeOfImage = 0x10C;
constexpr uint32_t kOffTimeDate = 0x114;
constexpr uint32_t kOffCertificateAddress = 0x118;
constexpr uint32_t kOffSectionCount = 0x11C;
constexpr uint32_t kOffSectionHeadersAddress = 0x120;
constexpr uint32_t kOffInitFlags = 0x124;
constexpr uint32_t kOffEntryPoint = 0x128;
constexpr uint32_t kOffTlsAddress = 0x12C;
constexpr uint32_t kOffStackCommit = 0x130;
constexpr uint32_t kOffHeapReserve = 0x134;
constexpr uint32_t kOffHeapCommit = 0x138;
constexpr uint32_t kOffKernelThunkAddress = 0x158;
constexpr uint32_t kOffLibraryVersionCount = 0x160;
constexpr uint32_t kOffLibraryVersionsAddress = 0x164;
constexpr uint32_t kImageHeaderMinSize = 0x178;

// The entry point and kernel thunk address are stored XORed with a key that
// differs between retail and debug kernels.
constexpr uint32_t kEntryKeyRetail = 0xA8FC57AB;
constexpr uint32_t kEntryKeyDebug = 0x94859D4B;
constexpr uint32_t kThunkKeyRetail = 0x5B6D40B6;
constexpr uint32_t kThunkKeyDebug = 0xEFB1F152;

constexpr uint32_t kSectionHeaderSize = 56;
constexpr uint32_t kLibraryVersionSize = 16;
constexpr uint32_t kCertificateTitleNameChars = 40;
constexpr uint32_t kThunkImportByOrdinal = 0x80000000;
constexpr size_t kMaxKernelImports = 512;

uint32_t LoadU32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t LoadU16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

void SetError(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

}  // namespace

std::optional<XbeImage> XbeImage::Parse(std::vector<uint8_t> bytes, std::string* error)
{
    XbeImage image;
    image.bytes_ = std::move(bytes);
    if (!image.ParseAll(error))
    {
        return std::nullopt;
    }
    return image;
}

std::optional<XbeImage> XbeImage::Load(const std::filesystem::path& path, std::string* error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        SetError(error, "cannot open file");
        return std::nullopt;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Parse(std::move(bytes), error);
}

std::optional<uint32_t> XbeImage::FileOffsetOf(uint32_t virtual_address) const
{
    if (virtual_address >= base_address_ && virtual_address - base_address_ < size_of_headers_)
    {
        return virtual_address - base_address_;
    }
    for (const XbeSection& section : sections_)
    {
        if (virtual_address >= section.virtual_address &&
            virtual_address - section.virtual_address < section.raw_size)
        {
            return section.raw_address + (virtual_address - section.virtual_address);
        }
    }
    return std::nullopt;
}

std::span<const uint8_t> XbeImage::BytesAt(uint32_t virtual_address, uint32_t count) const
{
    if (count == 0)
    {
        return {};
    }
    const std::optional<uint32_t> first = FileOffsetOf(virtual_address);
    const std::optional<uint32_t> last = FileOffsetOf(virtual_address + count - 1);
    // Both ends must land in the same contiguous run of the file.
    if (!first || !last || *last - *first != count - 1 || *last >= bytes_.size())
    {
        return {};
    }
    return std::span<const uint8_t>(bytes_.data() + *first, count);
}

std::optional<uint32_t> XbeImage::ReadU32At(uint32_t virtual_address) const
{
    const std::span<const uint8_t> raw = BytesAt(virtual_address, 4);
    if (raw.empty())
    {
        return std::nullopt;
    }
    return LoadU32(raw.data());
}

std::string XbeImage::ReadCStringAt(uint32_t virtual_address) const
{
    std::string text;
    const std::optional<uint32_t> offset = FileOffsetOf(virtual_address);
    if (!offset)
    {
        return text;
    }
    for (size_t i = *offset; i < bytes_.size() && bytes_[i] != 0 && text.size() < 256; ++i)
    {
        text.push_back(static_cast<char>(bytes_[i]));
    }
    return text;
}

bool XbeImage::ParseAll(std::string* error)
{
    if (bytes_.size() < kImageHeaderMinSize || LoadU32(bytes_.data()) != kXbeMagic)
    {
        SetError(error, "not an XBE image (missing XBEH magic)");
        return false;
    }

    const uint8_t* header = bytes_.data();
    base_address_ = LoadU32(header + kOffBaseAddress);
    size_of_headers_ = LoadU32(header + kOffSizeOfHeaders);
    size_of_image_ = LoadU32(header + kOffSizeOfImage);
    time_date_ = LoadU32(header + kOffTimeDate);
    init_flags_ = LoadU32(header + kOffInitFlags);
    stack_commit_ = LoadU32(header + kOffStackCommit);
    heap_reserve_ = LoadU32(header + kOffHeapReserve);
    heap_commit_ = LoadU32(header + kOffHeapCommit);
    if (size_of_headers_ > bytes_.size())
    {
        SetError(error, "header size exceeds file size");
        return false;
    }

    // Sections first: every later address lookup may land inside one.
    const uint32_t section_count = LoadU32(header + kOffSectionCount);
    const uint32_t section_headers = LoadU32(header + kOffSectionHeadersAddress);
    const std::span<const uint8_t> section_table = BytesAt(section_headers, section_count * kSectionHeaderSize);
    if (section_count == 0 || section_table.empty())
    {
        SetError(error, "section table out of range");
        return false;
    }
    sections_.resize(section_count);
    for (uint32_t i = 0; i < section_count; ++i)
    {
        const uint8_t* entry = section_table.data() + i * kSectionHeaderSize;
        XbeSection& section = sections_[i];
        section.flags = LoadU32(entry + 0);
        section.virtual_address = LoadU32(entry + 4);
        section.virtual_size = LoadU32(entry + 8);
        section.raw_address = LoadU32(entry + 12);
        section.raw_size = LoadU32(entry + 16);
        if (static_cast<uint64_t>(section.raw_address) + section.raw_size > bytes_.size())
        {
            SetError(error, "section raw data exceeds file size");
            return false;
        }
    }
    for (uint32_t i = 0; i < section_count; ++i)
    {
        const uint32_t name_address = LoadU32(section_table.data() + i * kSectionHeaderSize + 20);
        sections_[i].name = ReadCStringAt(name_address);
    }

    // Pick the key set whose decoded entry point lands inside the image.
    const uint32_t encoded_entry = LoadU32(header + kOffEntryPoint);
    const uint32_t encoded_thunk = LoadU32(header + kOffKernelThunkAddress);
    const uint32_t image_end = base_address_ + size_of_image_;
    const uint32_t retail_entry = encoded_entry ^ kEntryKeyRetail;
    if (retail_entry >= base_address_ && retail_entry < image_end)
    {
        key_kind_ = XbeKeyKind::kRetail;
        entry_point_ = retail_entry;
        kernel_thunk_address_ = encoded_thunk ^ kThunkKeyRetail;
    }
    else
    {
        key_kind_ = XbeKeyKind::kDebug;
        entry_point_ = encoded_entry ^ kEntryKeyDebug;
        kernel_thunk_address_ = encoded_thunk ^ kThunkKeyDebug;
        if (entry_point_ < base_address_ || entry_point_ >= image_end)
        {
            SetError(error, "entry point decodes outside the image with both key sets");
            return false;
        }
    }

    // Certificate.
    const uint32_t certificate_address = LoadU32(header + kOffCertificateAddress);
    const std::span<const uint8_t> certificate = BytesAt(certificate_address, 0xB0);
    if (certificate.empty())
    {
        SetError(error, "certificate out of range");
        return false;
    }
    certificate_.time_date = LoadU32(certificate.data() + 0x04);
    certificate_.title_id = LoadU32(certificate.data() + 0x08);
    for (uint32_t i = 0; i < kCertificateTitleNameChars; ++i)
    {
        const char16_t ch = static_cast<char16_t>(LoadU16(certificate.data() + 0x0C + i * 2));
        if (ch == 0)
        {
            break;
        }
        certificate_.title_name.push_back(ch);
    }
    certificate_.allowed_media = LoadU32(certificate.data() + 0x9C);
    certificate_.game_region = LoadU32(certificate.data() + 0xA0);
    certificate_.game_ratings = LoadU32(certificate.data() + 0xA4);
    certificate_.disk_number = LoadU32(certificate.data() + 0xA8);
    certificate_.version = LoadU32(certificate.data() + 0xAC);

    // Library versions: the statically linked XDK libraries.
    const uint32_t library_count = LoadU32(header + kOffLibraryVersionCount);
    const uint32_t library_address = LoadU32(header + kOffLibraryVersionsAddress);
    const std::span<const uint8_t> libraries = BytesAt(library_address, library_count * kLibraryVersionSize);
    if (library_count != 0 && libraries.empty())
    {
        SetError(error, "library version table out of range");
        return false;
    }
    for (uint32_t i = 0; i < library_count; ++i)
    {
        const uint8_t* entry = libraries.data() + i * kLibraryVersionSize;
        XbeLibraryVersion library;
        library.name.assign(reinterpret_cast<const char*>(entry), strnlen(reinterpret_cast<const char*>(entry), 8));
        library.major = LoadU16(entry + 8);
        library.minor = LoadU16(entry + 10);
        library.build = LoadU16(entry + 12);
        library.flags = LoadU16(entry + 14);
        library_versions_.push_back(std::move(library));
    }

    // TLS directory (optional).
    const uint32_t tls_address = LoadU32(header + kOffTlsAddress);
    if (tls_address != 0)
    {
        const std::span<const uint8_t> tls = BytesAt(tls_address, 24);
        if (!tls.empty())
        {
            XbeTls value;
            value.raw_data_start = LoadU32(tls.data() + 0);
            value.raw_data_end = LoadU32(tls.data() + 4);
            value.index_address = LoadU32(tls.data() + 8);
            value.callbacks_address = LoadU32(tls.data() + 12);
            value.zero_fill_size = LoadU32(tls.data() + 16);
            value.characteristics = LoadU32(tls.data() + 20);
            tls_ = value;
        }
    }

    // Kernel thunk table: ordinal imports terminated by zero.
    for (uint32_t address = kernel_thunk_address_; kernel_imports_.size() < kMaxKernelImports; address += 4)
    {
        const std::optional<uint32_t> thunk = ReadU32At(address);
        if (!thunk)
        {
            SetError(error, "kernel thunk table out of range");
            return false;
        }
        if (*thunk == 0)
        {
            break;
        }
        if ((*thunk & kThunkImportByOrdinal) == 0)
        {
            SetError(error, "kernel thunk entry is not an ordinal import");
            return false;
        }
        kernel_imports_.push_back(*thunk & ~kThunkImportByOrdinal);
    }
    return true;
}

}  // namespace rexbox::xbe
