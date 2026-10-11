// XBE (Xbox executable) image parser.
//
// Title-agnostic: reads any retail or debug XBE. Layout references are in
// docs/kb/xbe-format.md.

#ifndef REXBOX_XBE_XBE_IMAGE_H
#define REXBOX_XBE_XBE_IMAGE_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rexbox::xbe
{

// Which XOR keys decoded the entry point and kernel thunk address.
enum class XbeKeyKind
{
    kRetail,
    kDebug,
};

struct XbeSection
{
    std::string name;
    uint32_t flags = 0;
    uint32_t virtual_address = 0;
    uint32_t virtual_size = 0;
    uint32_t raw_address = 0;
    uint32_t raw_size = 0;

    bool IsWritable() const { return (flags & 0x01) != 0; }
    bool IsPreload() const { return (flags & 0x02) != 0; }
    bool IsExecutable() const { return (flags & 0x04) != 0; }
};

struct XbeLibraryVersion
{
    std::string name;
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t build = 0;
    uint16_t flags = 0;
};

struct XbeCertificate
{
    uint32_t time_date = 0;
    uint32_t title_id = 0;
    std::u16string title_name;
    uint32_t allowed_media = 0;
    uint32_t game_region = 0;
    uint32_t game_ratings = 0;
    uint32_t disk_number = 0;
    uint32_t version = 0;
};

struct XbeTls
{
    uint32_t raw_data_start = 0;
    uint32_t raw_data_end = 0;
    uint32_t index_address = 0;
    uint32_t callbacks_address = 0;
    uint32_t zero_fill_size = 0;
    uint32_t characteristics = 0;
};

class XbeImage
{
public:
    // Parses `bytes`. On failure returns std::nullopt and, if `error` is
    // non-null, stores a one-line reason.
    static std::optional<XbeImage> Parse(std::vector<uint8_t> bytes, std::string* error);
    static std::optional<XbeImage> Load(const std::filesystem::path& path, std::string* error);

    uint32_t base_address() const { return base_address_; }
    uint32_t size_of_headers() const { return size_of_headers_; }
    uint32_t size_of_image() const { return size_of_image_; }
    uint32_t time_date() const { return time_date_; }
    uint32_t init_flags() const { return init_flags_; }
    uint32_t entry_point() const { return entry_point_; }
    uint32_t kernel_thunk_address() const { return kernel_thunk_address_; }
    uint32_t stack_commit() const { return stack_commit_; }
    uint32_t heap_reserve() const { return heap_reserve_; }
    uint32_t heap_commit() const { return heap_commit_; }
    XbeKeyKind key_kind() const { return key_kind_; }

    const XbeCertificate& certificate() const { return certificate_; }
    const std::vector<XbeSection>& sections() const { return sections_; }
    const std::vector<XbeLibraryVersion>& library_versions() const { return library_versions_; }
    const std::optional<XbeTls>& tls() const { return tls_; }

    // Kernel ordinals imported through the thunk table, in table order.
    const std::vector<uint32_t>& kernel_imports() const { return kernel_imports_; }

    const std::vector<uint8_t>& bytes() const { return bytes_; }

    // Maps a guest virtual address to a file offset. Addresses inside the
    // headers map 1:1 from the base; addresses inside a section map through
    // its raw data. Returns std::nullopt for addresses with no file backing
    // (including the zero-filled tail of a section).
    std::optional<uint32_t> FileOffsetOf(uint32_t virtual_address) const;

    // Reads `count` bytes at a guest virtual address, or an empty span if any
    // byte of the range has no file backing.
    std::span<const uint8_t> BytesAt(uint32_t virtual_address, uint32_t count) const;

private:
    XbeImage() = default;

    bool ParseAll(std::string* error);
    std::optional<uint32_t> ReadU32At(uint32_t virtual_address) const;
    std::string ReadCStringAt(uint32_t virtual_address) const;

    std::vector<uint8_t> bytes_;
    uint32_t base_address_ = 0;
    uint32_t size_of_headers_ = 0;
    uint32_t size_of_image_ = 0;
    uint32_t time_date_ = 0;
    uint32_t init_flags_ = 0;
    uint32_t entry_point_ = 0;
    uint32_t kernel_thunk_address_ = 0;
    uint32_t stack_commit_ = 0;
    uint32_t heap_reserve_ = 0;
    uint32_t heap_commit_ = 0;
    XbeKeyKind key_kind_ = XbeKeyKind::kRetail;
    XbeCertificate certificate_;
    std::vector<XbeSection> sections_;
    std::vector<XbeLibraryVersion> library_versions_;
    std::optional<XbeTls> tls_;
    std::vector<uint32_t> kernel_imports_;
};

}  // namespace rexbox::xbe

#endif  // REXBOX_XBE_XBE_IMAGE_H
