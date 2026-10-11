// rexbox_xbe_info: prints what an XBE declares about itself -- certificate,
// sections, linked XDK libraries, TLS and kernel imports.
//
// Usage: rexbox_xbe_info <default.xbe>

#include "rexbox/hle/kernel/kernel_exports.h"
#include "rexbox/xbe/xbe_image.h"

#include <cstdio>
#include <string>

namespace
{

std::string Narrow(const std::u16string& text)
{
    std::string out;
    for (const char16_t ch : text)
    {
        out.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
    }
    return out;
}

std::string SectionFlags(const rexbox::xbe::XbeSection& section)
{
    std::string flags;
    flags += section.IsWritable() ? 'W' : '-';
    flags += section.IsExecutable() ? 'X' : '-';
    flags += section.IsPreload() ? 'P' : '-';
    return flags;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <default.xbe>\n", argv[0]);
        return 2;
    }

    std::string error;
    const auto image = rexbox::xbe::XbeImage::Load(argv[1], &error);
    if (!image)
    {
        std::fprintf(stderr, "error: %s: %s\n", argv[1], error.c_str());
        return 1;
    }

    const auto& cert = image->certificate();
    std::printf("file           %s (%zu bytes)\n", argv[1], image->bytes().size());
    std::printf("title          %s\n", Narrow(cert.title_name).c_str());
    std::printf("title id       %08X\n", cert.title_id);
    std::printf("cert version   %08X\n", cert.version);
    std::printf("cert time      %08X\n", cert.time_date);
    std::printf("region         %08X\n", cert.game_region);
    std::printf("media          %08X\n", cert.allowed_media);
    std::printf("disk           %u\n", cert.disk_number);
    std::printf("keys           %s\n", image->key_kind() == rexbox::xbe::XbeKeyKind::kRetail ? "retail" : "debug");
    std::printf("base           %08X\n", image->base_address());
    std::printf("image size     %08X\n", image->size_of_image());
    std::printf("headers size   %08X\n", image->size_of_headers());
    std::printf("time           %08X\n", image->time_date());
    std::printf("init flags     %08X\n", image->init_flags());
    std::printf("entry point    %08X\n", image->entry_point());
    std::printf("kernel thunks  %08X\n", image->kernel_thunk_address());
    std::printf("stack commit   %08X\n", image->stack_commit());
    std::printf("heap           reserve %08X commit %08X\n", image->heap_reserve(), image->heap_commit());
    if (image->tls())
    {
        const auto& tls = *image->tls();
        std::printf("tls            data %08X-%08X index %08X callbacks %08X zero-fill %08X\n",
                    tls.raw_data_start, tls.raw_data_end, tls.index_address, tls.callbacks_address,
                    tls.zero_fill_size);
    }

    std::printf("\nsections (%zu)\n", image->sections().size());
    std::printf("  %-10s %-4s %-8s %-8s %-8s %-8s\n", "name", "flag", "vaddr", "vsize", "raw", "rawsize");
    for (const auto& section : image->sections())
    {
        std::printf("  %-10s %-4s %08X %08X %08X %08X\n", section.name.c_str(), SectionFlags(section).c_str(),
                    section.virtual_address, section.virtual_size, section.raw_address, section.raw_size);
    }

    std::printf("\nlibraries (%zu)\n", image->library_versions().size());
    for (const auto& library : image->library_versions())
    {
        std::printf("  %-8s %u.%u.%u flags %04X\n", library.name.c_str(), library.major, library.minor,
                    library.build, library.flags);
    }

    std::printf("\nkernel imports (%zu)\n", image->kernel_imports().size());
    for (const uint32_t ordinal : image->kernel_imports())
    {
        const char* name = rexbox::hle::kernel::KernelExportName(ordinal);
        std::printf("  %3u %s\n", ordinal, name != nullptr ? name : "<unknown>");
    }
    return 0;
}
