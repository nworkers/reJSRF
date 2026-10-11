// rexbox_xdk_symbols: identifies the XDK library functions and variables
// statically linked into an XBE, with XbSymbolDatabase, and prints them as
// JSON sorted by address.
//
// Usage: rexbox_xdk_symbols <default.xbe> [--json <output>]
//
// Title-agnostic: scripts/recomp/regenerate.py uses the output to generate
// HLE overrides (docs/design/20261012-i006-xdk-symbol-binding.md).

#include <libXbSymbolDatabase.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

struct Symbol
{
    std::string library;
    std::string name;
    uint32_t address = 0;
    bool function = false;
    std::string call;
    uint32_t params = 0;
    uint32_t build = 0;
};

std::vector<Symbol> g_symbols;

void OnMessage(xb_output_message /*flag*/, const char* message)
{
    std::fprintf(stderr, "XbSymbolDatabase: %s\n", message);
}

void OnSymbol(const char* library_str, uint32_t /*library_flag*/, uint32_t /*xref_index*/, const char* symbol_str,
              xbaddr address, uint32_t build_version, uint32_t symbol_type, uint32_t call_type, uint32_t param_count,
              const XbSDBSymbolParam* param_list)
{
    Symbol symbol;
    symbol.library.assign(library_str, strnlen(library_str, 8));
    symbol.name = symbol_str;
    symbol.address = static_cast<uint32_t>(address);
    symbol.function = symbol_type == symbol_function;
    symbol.call = XbSDB_CallingConventionToString(call_type);
    for (uint32_t i = 0; i < param_count && param_list != nullptr; ++i)
    {
        if (param_list[i].type == param_void)
        {
            break;
        }
        ++symbol.params;
    }
    symbol.build = build_version;
    g_symbols.push_back(std::move(symbol));
}

std::string Escape(const std::string& text)
{
    std::string out;
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <default.xbe> [--json <output>]\n", argv[0]);
        return 2;
    }
    const char* output_path = nullptr;
    for (int i = 2; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--json") == 0)
        {
            output_path = argv[i + 1];
        }
    }

    std::ifstream file(argv[1], std::ios::binary);
    std::vector<unsigned char> xbe((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (xbe.size() < 0x200 || std::memcmp(xbe.data(), "XBEH", 4) != 0)
    {
        std::fprintf(stderr, "error: %s is not an XBE\n", argv[1]);
        return 1;
    }

    XbSDB_SetOutputMessage(&OnMessage);
    XbSDBLibraryHeader libraries{};
    libraries.count = XbSDB_GenerateLibraryFilter(xbe.data(), nullptr);
    std::vector<XbSDBLibrary> library_filters(libraries.count);
    libraries.filters = library_filters.data();
    XbSDB_GenerateLibraryFilter(xbe.data(), &libraries);

    XbSDBSectionHeader sections{};
    sections.count = XbSDB_GenerateSectionFilter(xbe.data(), nullptr, true);
    std::vector<XbSDBSection> section_filters(sections.count);
    sections.filters = section_filters.data();
    XbSDB_GenerateSectionFilter(xbe.data(), &sections, true);

    XbSDBContextHandle context = nullptr;
    if (!XbSDB_CreateContext(&context, &OnSymbol, libraries, sections, XbSDB_GetKernelThunkAddress(xbe.data())))
    {
        std::fprintf(stderr, "error: XbSymbolDatabase could not create a scan context\n");
        return 1;
    }
    XbSDBContext_ScanManual(context);
    XbSDBContext_ScanAllLibraryFilter(context);
    XbSDBContext_RegisterXRefs(context);
    XbSDBContext_Release(context);

    std::sort(g_symbols.begin(), g_symbols.end(), [](const Symbol& a, const Symbol& b)
              { return a.address != b.address ? a.address < b.address : a.name < b.name; });
    g_symbols.erase(std::unique(g_symbols.begin(), g_symbols.end(), [](const Symbol& a, const Symbol& b)
                                { return a.address == b.address && a.name == b.name; }),
                    g_symbols.end());

    std::string json = "[\n";
    for (size_t i = 0; i < g_symbols.size(); ++i)
    {
        const Symbol& s = g_symbols[i];
        char address[16];
        std::snprintf(address, sizeof(address), "0x%08X", s.address);
        json += "  {\"library\": \"" + Escape(s.library) + "\", \"name\": \"" + Escape(s.name) + "\", \"address\": \"" +
                address + "\", \"kind\": \"" + (s.function ? "function" : "variable") + "\", \"call\": \"" +
                Escape(s.call) + "\", \"params\": " + std::to_string(s.params) +
                ", \"build\": " + std::to_string(s.build) + "}" + (i + 1 < g_symbols.size() ? ",\n" : "\n");
    }
    json += "]\n";

    if (output_path != nullptr)
    {
        std::ofstream out(output_path, std::ios::binary);
        out << json;
    }
    else
    {
        std::fwrite(json.data(), 1, json.size(), stdout);
    }
    std::fprintf(stderr, "%zu symbol(s)\n", g_symbols.size());
    return 0;
}
