// XcSHA*: SHA-1 (FIPS 180-4) with its context kept in the guest's buffer, as
// the kernel keeps it, so a title may copy or reuse contexts freely.

#include "kernel_internal.h"

#include <array>

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;

// Context layout inside the guest buffer (the kernel's is 0x74 bytes):
// state[5] (+0), bit count low/high (+0x14), buffer[64] (+0x1C).
constexpr uint32_t kCtxState = 0x00;
constexpr uint32_t kCtxCountLow = 0x14;
constexpr uint32_t kCtxCountHigh = 0x18;
constexpr uint32_t kCtxBuffer = 0x1C;

uint32_t Rotl(uint32_t value, int bits)
{
    return (value << bits) | (value >> (32 - bits));
}

void Transform(std::array<uint32_t, 5>& state, const uint8_t* block)
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | block[i * 4 + 3];
    }
    for (int i = 16; i < 80; ++i)
    {
        w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i)
    {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20)
        {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        }
        else if (i < 40)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        }
        else if (i < 60)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const uint32_t temp = Rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rotl(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

std::array<uint32_t, 5> LoadState(GuestAddress context)
{
    std::array<uint32_t, 5> state;
    for (int i = 0; i < 5; ++i)
    {
        state[i] = Memory().Read32(context + kCtxState + i * 4);
    }
    return state;
}

void StoreState(GuestAddress context, const std::array<uint32_t, 5>& state)
{
    for (int i = 0; i < 5; ++i)
    {
        Memory().Write32(context + kCtxState + i * 4, state[i]);
    }
}

void Update(GuestAddress context, const uint8_t* data, uint32_t length)
{
    runtime::GuestMemory& memory = Memory();
    std::array<uint32_t, 5> state = LoadState(context);
    uint64_t bits = static_cast<uint64_t>(memory.Read32(context + kCtxCountLow)) |
                    (static_cast<uint64_t>(memory.Read32(context + kCtxCountHigh)) << 32);
    uint32_t used = static_cast<uint32_t>((bits / 8) % 64);
    uint8_t* buffer = memory.Translate(context + kCtxBuffer);
    for (uint32_t i = 0; i < length; ++i)
    {
        buffer[used++] = data[i];
        if (used == 64)
        {
            Transform(state, buffer);
            used = 0;
        }
    }
    bits += static_cast<uint64_t>(length) * 8;
    memory.Write32(context + kCtxCountLow, static_cast<uint32_t>(bits));
    memory.Write32(context + kCtxCountHigh, static_cast<uint32_t>(bits >> 32));
    StoreState(context, state);
}

// VOID XcSHAInit(PUCHAR pbSHAContext)
void XcSHAInit()
{
    HleCall call;
    const GuestAddress context = call.Arg(0);
    Memory().Fill(context, 0, kCtxBuffer + 64);
    StoreState(context, {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0});
    call.Return(0, 1);
}

// VOID XcSHAUpdate(PUCHAR pbSHAContext, PUCHAR pbInput, ULONG dwInputLength)
void XcSHAUpdate()
{
    HleCall call;
    const uint32_t length = call.Arg(2);
    if (length != 0)
    {
        Update(call.Arg(0), Memory().Translate(call.Arg(1)), length);
    }
    call.Return(0, 3);
}

// VOID XcSHAFinal(PUCHAR pbSHAContext, PUCHAR pbDigest)
void XcSHAFinal()
{
    HleCall call;
    const GuestAddress context = call.Arg(0);
    runtime::GuestMemory& memory = Memory();
    const uint64_t bits = static_cast<uint64_t>(memory.Read32(context + kCtxCountLow)) |
                          (static_cast<uint64_t>(memory.Read32(context + kCtxCountHigh)) << 32);
    const uint8_t pad = 0x80;
    Update(context, &pad, 1);
    const uint8_t zero = 0;
    while (((memory.Read32(context + kCtxCountLow) / 8) % 64) != 56)
    {
        Update(context, &zero, 1);
    }
    uint8_t length_be[8];
    for (int i = 0; i < 8; ++i)
    {
        length_be[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
    }
    Update(context, length_be, 8);
    const std::array<uint32_t, 5> state = LoadState(context);
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            memory.Write8(call.Arg(1) + i * 4 + j, static_cast<uint8_t>(state[i] >> (24 - 8 * j)));
        }
    }
    call.Return(0, 2);
}

}  // namespace

void RegisterCryptoExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "XcSHAInit", &XcSHAInit);
    registry.AddExport("kernel", "XcSHAUpdate", &XcSHAUpdate);
    registry.AddExport("kernel", "XcSHAFinal", &XcSHAFinal);
}

}  // namespace rexbox::hle::kernel
