// rejsrf_decomp_check: runs each decompiled function and the lifted body it
// replaced (sub_X_gen) from identical guest states and compares what they
// leave behind -- general registers, XMM registers and all of guest memory.
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

extern "C"
{
void sub_000128C0(void);
void sub_000128C0_gen(void);

// MMATRIX (src/game/jsrf/decomp/mmatrix.c).
void sub_001BA8A0(void);
void sub_001BA8A0_gen(void);
void sub_001BA8F0(void);
void sub_001BA8F0_gen(void);
void sub_001BA950(void);
void sub_001BA950_gen(void);
void sub_001BA9B0(void);
void sub_001BA9B0_gen(void);
void sub_001BA9C0(void);
void sub_001BA9C0_gen(void);
void sub_001BAA00(void);
void sub_001BAA00_gen(void);
void sub_001BAA50(void);
void sub_001BAA50_gen(void);
void sub_001BAA60(void);
void sub_001BAA60_gen(void);
void sub_001BAAA0(void);
void sub_001BAAA0_gen(void);
void sub_001BAAE0(void);
void sub_001BAAE0_gen(void);
void sub_001BAB20(void);
void sub_001BAB20_gen(void);
void sub_001BABA0(void);
void sub_001BABA0_gen(void);
void sub_001BAC00(void);
void sub_001BAC00_gen(void);
void sub_001BAC50(void);
void sub_001BAC50_gen(void);
void sub_001BAC70(void);
void sub_001BAC70_gen(void);
void sub_001BADB0(void);
void sub_001BADB0_gen(void);
void sub_001BB270(void);
void sub_001BB270_gen(void);
void sub_001BB2B0(void);
void sub_001BB2B0_gen(void);
void sub_001BB2E0(void);
void sub_001BB2E0_gen(void);
void sub_001BB330(void);
void sub_001BB330_gen(void);
void sub_001BB380(void);
void sub_001BB380_gen(void);
}

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

void WriteF32(GuestMemory& memory, GuestAddress address, float value)
{
    std::memcpy(memory.Translate(address), &value, 4);
}

struct GuestState
{
    RexboxRecompGprs gprs = {};
    uint32_t xmm[8][4] = {};
};

struct Case
{
    std::string name;
    std::function<void(GuestMemory&, GuestState&)> setup;
};

struct Outcome
{
    GuestState state;
    std::vector<uint8_t> memory;
};

Outcome RunFrom(GuestMemory& memory, const std::vector<uint8_t>& initial_memory, const GuestState& initial,
                GuestFunction function)
{
    std::memcpy(memory.base(), initial_memory.data(), initial_memory.size());
    rexbox_recomp_reset_thread(initial.gprs.esp, 0);
    rexbox_recomp_set_gprs(&initial.gprs);
    for (int i = 0; i < 8; ++i)
    {
        rexbox_recomp_set_xmm(i, initial.xmm[i]);
    }
    function();
    Outcome outcome;
    rexbox_recomp_get_gprs(&outcome.state.gprs);
    for (int i = 0; i < 8; ++i)
    {
        rexbox_recomp_get_xmm(i, outcome.state.xmm[i]);
    }
    outcome.memory.assign(memory.base(), memory.base() + memory.size());
    return outcome;
}

bool Compare(const char* function, const Case& test, const Outcome& decompiled, const Outcome& lifted)
{
    bool same = true;
    const uint32_t* a = &decompiled.state.gprs.eax;
    const uint32_t* b = &lifted.state.gprs.eax;
    static const char* kNames[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    for (int i = 0; i < 8; ++i)
    {
        if (a[i] != b[i])
        {
            std::printf("  %s %s: %s decompiled %08X lifted %08X\n", function, test.name.c_str(), kNames[i], a[i], b[i]);
            same = false;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (std::memcmp(decompiled.state.xmm[i], lifted.state.xmm[i], 16) != 0)
        {
            const uint32_t* x = decompiled.state.xmm[i];
            const uint32_t* y = lifted.state.xmm[i];
            std::printf("  %s %s: xmm%d decompiled %08X %08X %08X %08X lifted %08X %08X %08X %08X\n", function,
                        test.name.c_str(), i, x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3]);
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

struct Suite
{
    const char* name;
    GuestFunction decompiled;
    GuestFunction lifted;
    std::vector<Case> cases;
};

// ---- sub_000128C0: thiscall (this, index) -> this->table_098[index] or 0.

Suite IndexedTableSuite()
{
    Suite suite{"sub_000128C0", &sub_000128C0, &sub_000128C0_gen, {}};
    for (const int32_t index : {0, 1, 7, 255, -1, INT32_MIN})
    {
        suite.cases.push_back({"index " + std::to_string(index), [index](GuestMemory& m, GuestState& s)
                               {
                                   for (uint32_t i = 0; i < 256; ++i)
                                   {
                                       WriteU32(m, kObject + 0x98 + i * 4, 0xA5000000u | i);
                                   }
                                   RexboxRecompGprs& r = s.gprs;
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
    return suite;
}

// ---- MMATRIX: the library's globals point into these areas.

constexpr GuestAddress kStackCell = 0x00264C04;
constexpr GuestAddress kSinCosTable = 0x00264BA4;
constexpr GuestAddress kWorkMatrixCell = 0x00264C08;
constexpr GuestAddress kWorkScalar = 0x00264BD8;
constexpr GuestAddress kWorkVectorCell = 0x0022E5B0;

constexpr GuestAddress kCell = 0x00500000;        // holds the current matrix address
constexpr GuestAddress kMatrixStack = 0x00600000;  // 8 slots
constexpr GuestAddress kCurrent = kMatrixStack + 0x80;
constexpr GuestAddress kTable = 0x00700000;      // 4096 (a, b) pairs
constexpr GuestAddress kWork = 0x00510000;       // scratch matrix
constexpr GuestAddress kWorkVector = 0x00520000;
constexpr GuestAddress kArgs = 0x00530000;       // argument buffers
constexpr GuestAddress kOut = 0x00540000;

// Deterministic floats in [-4, 4), with exact small values mixed in.
struct Random
{
    uint32_t state;
    uint32_t Next()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    float Float()
    {
        const uint32_t v = Next();
        if ((v & 15) == 0)
        {
            return static_cast<float>(static_cast<int>(v >> 28) - 8) * 0.5f;  // exact values, zeros
        }
        return (static_cast<float>(v >> 8) / 16777216.0f) * 8.0f - 4.0f;
    }
};

void FillFloats(GuestMemory& m, Random& random, GuestAddress address, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        WriteF32(m, address + i * 4, random.Float());
    }
}

// The library's state plus registers and XMM garbage; the caller adds the
// arguments. esp points at a return address with free stack below.
void MatrixSetup(GuestMemory& m, GuestState& s, uint32_t seed)
{
    Random random{seed};
    WriteU32(m, kStackCell, kCell);
    WriteU32(m, kCell, kCurrent);
    FillFloats(m, random, kMatrixStack, 8 * 16);
    WriteU32(m, kSinCosTable, kTable);
    FillFloats(m, random, kTable, 4096 * 2);
    WriteU32(m, kWorkMatrixCell, kWork);
    FillFloats(m, random, kWork, 16);
    WriteU32(m, kWorkVectorCell, kWorkVector);
    FillFloats(m, random, kWorkVector, 4);
    WriteF32(m, kWorkScalar, random.Float());
    FillFloats(m, random, kArgs, 64);
    FillFloats(m, random, kOut, 64);
    RexboxRecompGprs& r = s.gprs;
    r.esp = kStackTop - 0x40;
    WriteU32(m, r.esp, 0x00012345);
    FillFloats(m, random, r.esp + 4, 4);  // stack arguments
    r.eax = 0xDEADBEEF;
    r.ecx = 0x10101010;
    r.edx = 0x11111111;
    r.ebx = 0x22222222;
    r.esi = 0x33333333;
    r.edi = 0x44444444;
    r.ebp = 0x55555555;
    for (int i = 0; i < 8; ++i)
    {
        for (int lane = 0; lane < 4; ++lane)
        {
            s.xmm[i][lane] = random.Next();
        }
    }
}

using MatrixArguments = std::function<void(GuestMemory&, GuestState&)>;

Suite MatrixSuite(const char* name, GuestFunction decompiled, GuestFunction lifted,
                  std::vector<std::pair<std::string, MatrixArguments>> variants)
{
    Suite suite{name, decompiled, lifted, {}};
    for (const auto& variant : variants)
    {
        for (const uint32_t seed : {1u, 2u, 3u})
        {
            const MatrixArguments arguments = variant.second;
            suite.cases.push_back({variant.first + " seed " + std::to_string(seed),
                                   [seed, arguments](GuestMemory& m, GuestState& s)
                                   {
                                       MatrixSetup(m, s, seed);
                                       arguments(m, s);
                                   }});
        }
    }
    return suite;
}

MatrixArguments EcxIs(GuestAddress address)
{
    return [address](GuestMemory&, GuestState& s) { s.gprs.ecx = address; };
}

MatrixArguments None()
{
    return [](GuestMemory&, GuestState&) {};
}

MatrixArguments Angle(uint32_t angle)
{
    return [angle](GuestMemory&, GuestState& s) { s.gprs.ecx = angle; };
}

MatrixArguments Angles(uint32_t x, uint32_t y, uint32_t z)
{
    return [x, y, z](GuestMemory& m, GuestState& s)
    {
        WriteU32(m, kArgs + 0x100, x);
        WriteU32(m, kArgs + 0x104, y);
        WriteU32(m, kArgs + 0x108, z);
        s.gprs.ecx = kArgs + 0x100;
    };
}

std::vector<Suite> MatrixSuites()
{
    std::vector<Suite> suites;
    suites.push_back(MatrixSuite("sub_001BA8A0", &sub_001BA8A0, &sub_001BA8A0_gen, {{"identity", None()}}));
    suites.push_back(MatrixSuite("sub_001BA8F0", &sub_001BA8F0, &sub_001BA8F0_gen,
                                 {{"aligned", EcxIs(kArgs)}, {"unaligned", EcxIs(kArgs + 4)}}));
    suites.push_back(MatrixSuite("sub_001BA950", &sub_001BA950, &sub_001BA950_gen,
                                 {{"aligned", EcxIs(kOut)}, {"unaligned", EcxIs(kOut + 8)}}));
    suites.push_back(MatrixSuite("sub_001BA9B0", &sub_001BA9B0, &sub_001BA9B0_gen, {{"get", None()}}));
    suites.push_back(MatrixSuite("sub_001BA9C0", &sub_001BA9C0, &sub_001BA9C0_gen, {{"push", None()}}));
    suites.push_back(MatrixSuite("sub_001BAA00", &sub_001BAA00, &sub_001BAA00_gen, {{"push identity", None()}}));
    suites.push_back(MatrixSuite("sub_001BAA50", &sub_001BAA50, &sub_001BAA50_gen, {{"pop", None()}}));
    suites.push_back(MatrixSuite("sub_001BAA60", &sub_001BAA60, &sub_001BAA60_gen, {{"stack xyz", None()}}));
    suites.push_back(MatrixSuite("sub_001BAAA0", &sub_001BAAA0, &sub_001BAAA0_gen, {{"vector", EcxIs(kArgs)}}));
    suites.push_back(MatrixSuite("sub_001BAAE0", &sub_001BAAE0, &sub_001BAAE0_gen, {{"vector", EcxIs(kArgs)}}));
    suites.push_back(MatrixSuite("sub_001BAB20", &sub_001BAB20, &sub_001BAB20_gen,
                                 {{"point", [](GuestMemory&, GuestState& s)
                                   {
                                       s.gprs.ecx = kOut;
                                       s.gprs.edx = kArgs;
                                   }},
                                  {"null input", [](GuestMemory&, GuestState& s)
                                   {
                                       s.gprs.ecx = kOut;
                                       s.gprs.edx = 0;
                                   }},
                                  {"in place", [](GuestMemory&, GuestState& s)
                                   {
                                       s.gprs.ecx = kArgs;
                                       s.gprs.edx = kArgs;
                                   }}}));
    const auto points = [](uint32_t count, uint32_t dst_stride, uint32_t src_stride)
    {
        return [count, dst_stride, src_stride](GuestMemory& m, GuestState& s)
        {
            WriteU32(m, kArgs + 0x100, kOut);
            WriteU32(m, kArgs + 0x104, kArgs);
            WriteU32(m, kArgs + 0x108, dst_stride);
            WriteU32(m, kArgs + 0x10C, src_stride);
            s.gprs.ecx = kArgs + 0x100;
            s.gprs.edx = count;
        };
    };
    suites.push_back(MatrixSuite("sub_001BABA0", &sub_001BABA0, &sub_001BABA0_gen,
                                 {{"one", points(1, 16, 12)}, {"three", points(3, 16, 12)},
                                  {"five strided", points(5, 32, 16)}}));
    suites.push_back(MatrixSuite("sub_001BAC00", &sub_001BAC00, &sub_001BAC00_gen,
                                 {{"vector", [](GuestMemory&, GuestState& s)
                                   {
                                       s.gprs.ecx = kOut;
                                       s.gprs.edx = kArgs;
                                   }}}));
    suites.push_back(MatrixSuite("sub_001BAC50", &sub_001BAC50, &sub_001BAC50_gen,
                                 {{"xyz", Angles(0x1234, 0x8000, 0xFFFF0010)},
                                  {"zero x", Angles(0, 0x4000, 0x0100)},
                                  {"zero all", Angles(0, 0, 0)},
                                  {"only z", Angles(0, 0, 0x7FFF)}}));
    suites.push_back(MatrixSuite("sub_001BAC70", &sub_001BAC70, &sub_001BAC70_gen,
                                 {{"angle", [](GuestMemory& m, GuestState& s)
                                   {
                                       WriteU32(m, kArgs + 0xC, 0x2345);
                                       s.gprs.ecx = kArgs;
                                   }},
                                  {"negative angle", [](GuestMemory& m, GuestState& s)
                                   {
                                       WriteU32(m, kArgs + 0x4C, 0xFFFFC000u);
                                       s.gprs.ecx = kArgs + 0x40;
                                   }}}));
    suites.push_back(MatrixSuite("sub_001BADB0", &sub_001BADB0, &sub_001BADB0_gen,
                                 {{"product", EcxIs(kArgs)}, {"self", EcxIs(kCurrent)}}));
    suites.push_back(MatrixSuite("sub_001BB270", &sub_001BB270, &sub_001BB270_gen, {{"scale", EcxIs(kArgs)}}));
    suites.push_back(MatrixSuite("sub_001BB2B0", &sub_001BB2B0, &sub_001BB2B0_gen, {{"uniform", None()}}));
    for (const auto& rotation : {std::make_pair("sub_001BB2E0", std::make_pair(&sub_001BB2E0, &sub_001BB2E0_gen)),
                                 std::make_pair("sub_001BB330", std::make_pair(&sub_001BB330, &sub_001BB330_gen)),
                                 std::make_pair("sub_001BB380", std::make_pair(&sub_001BB380, &sub_001BB380_gen))})
    {
        suites.push_back(MatrixSuite(rotation.first, rotation.second.first, rotation.second.second,
                                     {{"angle", Angle(0x1234)},
                                      {"zero", Angle(0)},
                                      {"negative", Angle(0xFFFF8001u)},
                                      {"high bits", Angle(0x7FFFFFFFu)}}));
    }
    return suites;
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

    std::vector<Suite> suites = MatrixSuites();
    suites.insert(suites.begin(), IndexedTableSuite());

    size_t total = 0;
    int failures = 0;
    for (const Suite& suite : suites)
    {
        for (const Case& test : suite.cases)
        {
            std::memset(memory.base(), 0, memory.size());
            GuestState initial;
            test.setup(memory, initial);
            const std::vector<uint8_t> initial_memory(memory.base(), memory.base() + memory.size());
            const Outcome decompiled = RunFrom(memory, initial_memory, initial, suite.decompiled);
            const Outcome lifted = RunFrom(memory, initial_memory, initial, suite.lifted);
            const bool same = Compare(suite.name, test, decompiled, lifted);
            std::printf("%s %s %s\n", same ? "match" : "DIFF ", suite.name, test.name.c_str());
            failures += same ? 0 : 1;
            ++total;
        }
    }
    std::printf("%zu case(s), %d difference(s)\n", total, failures);
    return failures == 0 ? 0 : 1;
}
