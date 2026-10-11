// rejsrf_decomp_check: runs each decompiled function and the lifted body it
// replaced (sub_X_gen) from identical guest states and compares what they
// leave behind -- general registers and all of guest memory.
//
// Usage: rejsrf_decomp_check
// Exit code 0 when every case matches.

#include "rexbox/recomp/recomp_abi.h"
#include "rexbox/runtime/guest_memory.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

extern "C" void sub_000128C0(void);
extern "C" void sub_000128C0_gen(void);

namespace
{

using rexbox::runtime::GuestAddress;
using rexbox::runtime::GuestMemory;
using GuestFunction = void (*)();

constexpr GuestAddress kStackTop = 0x00F00000;
constexpr GuestAddress kObject = 0x00400000;

void WriteU32(GuestMemory& memory, GuestAddress address, uint32_t value)
{
    std::memcpy(memory.Translate(address), &value, 4);
}

struct Case
{
    std::string name;
    std::function<void(GuestMemory&, RexboxRecompGprs&)> setup;
};

struct Outcome
{
    RexboxRecompGprs gprs;
    std::vector<uint8_t> memory;
};

Outcome RunFrom(GuestMemory& memory, const std::vector<uint8_t>& initial_memory, const RexboxRecompGprs& initial,
                GuestFunction function)
{
    std::memcpy(memory.base(), initial_memory.data(), initial_memory.size());
    rexbox_recomp_reset_thread(initial.esp, 0);
    rexbox_recomp_set_gprs(&initial);
    function();
    Outcome outcome;
    rexbox_recomp_get_gprs(&outcome.gprs);
    outcome.memory.assign(memory.base(), memory.base() + memory.size());
    return outcome;
}

bool Compare(const char* function, const Case& test, const Outcome& decompiled, const Outcome& lifted)
{
    bool same = true;
    const uint32_t* a = &decompiled.gprs.eax;
    const uint32_t* b = &lifted.gprs.eax;
    static const char* kNames[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    for (int i = 0; i < 8; ++i)
    {
        if (a[i] != b[i])
        {
            std::printf("  %s %s: %s decompiled %08X lifted %08X\n", function, test.name.c_str(), kNames[i], a[i], b[i]);
            same = false;
        }
    }
    if (decompiled.memory != lifted.memory)
    {
        size_t first = 0;
        while (decompiled.memory[first] == lifted.memory[first])
        {
            ++first;
        }
        std::printf("  %s %s: memory differs first at %08zX\n", function, test.name.c_str(), first);
        same = false;
    }
    return same;
}

}  // namespace

int main()
{
    // 16 MiB is enough for these cases and keeps the per-case copies cheap.
    GuestMemory memory;
    if (!memory.Initialize(16u * 1024u * 1024u))
    {
        std::fprintf(stderr, "cannot reserve guest memory\n");
        return 2;
    }
    rexbox_recomp_set_memory(reinterpret_cast<ptrdiff_t>(memory.base()), 0, 0);

    // sub_000128C0: thiscall (this, index) -> this->table_098[index] or 0.
    std::vector<Case> cases;
    for (const int32_t index : {0, 1, 7, 255, -1, INT32_MIN})
    {
        cases.push_back({"index " + std::to_string(index), [index](GuestMemory& m, RexboxRecompGprs& r)
                         {
                             for (uint32_t i = 0; i < 256; ++i)
                             {
                                 WriteU32(m, kObject + 0x98 + i * 4, 0xA5000000u | i);
                             }
                             r.ecx = kObject;
                             r.esp = kStackTop - 8;
                             WriteU32(m, r.esp, 0x00012345);  // return address
                             WriteU32(m, r.esp + 4, static_cast<uint32_t>(index));
                             r.eax = 0xDEADBEEF;
                             r.edx = 0x11111111;
                             r.ebx = 0x22222222;
                             r.esi = 0x33333333;
                             r.edi = 0x44444444;
                             r.ebp = 0x55555555;
                         }});
    }

    int failures = 0;
    for (const Case& test : cases)
    {
        std::memset(memory.base(), 0, memory.size());
        RexboxRecompGprs initial = {};
        test.setup(memory, initial);
        const std::vector<uint8_t> initial_memory(memory.base(), memory.base() + memory.size());
        const Outcome decompiled = RunFrom(memory, initial_memory, initial, &sub_000128C0);
        const Outcome lifted = RunFrom(memory, initial_memory, initial, &sub_000128C0_gen);
        const bool same = Compare("sub_000128C0", test, decompiled, lifted);
        std::printf("%s sub_000128C0 %s\n", same ? "match" : "DIFF ", test.name.c_str());
        failures += same ? 0 : 1;
    }
    std::printf("%zu case(s), %d difference(s)\n", cases.size(), failures);
    return failures == 0 ? 0 : 1;
}
