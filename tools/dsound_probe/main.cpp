// rexbox_dsound_probe: exercises the DSOUND HLE exports without a game. A
// fake executor holds the guest registers; each check pushes stdcall
// arguments on a guest stack, calls the export through the registry, and
// inspects eax and guest memory. The mixer runs only when a check renders
// (manual output), so cursor checks do not depend on time.
//
// Usage: rexbox_dsound_probe        (exit code 0 when every check passes)

#include "rexbox/hle/dsound/dsound_hle.h"
#include "rexbox/runtime/environment.h"
#include "rexbox/runtime/guest_cpu.h"
#include "rexbox/runtime/guest_heap.h"
#include "rexbox/runtime/guest_memory.h"
#include "rexbox/runtime/hle_registry.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{

using rexbox::runtime::GuestAddress;
using rexbox::runtime::Gpr;

class FakeCpu : public rexbox::runtime::GuestCpu
{
public:
    uint32_t Get(Gpr reg) const override { return regs_[static_cast<int>(reg)]; }
    void Set(Gpr reg, uint32_t value) override { regs_[static_cast<int>(reg)] = value; }
    GuestAddress FsBase() const override { return 0; }

private:
    std::array<uint32_t, 8> regs_{};
};

class FakeExecutor : public rexbox::runtime::GuestExecutor
{
public:
    rexbox::runtime::GuestCpu& Cpu() override { return cpu_; }
    bool StartThread(const rexbox::runtime::GuestThreadStart&, std::string* error) override
    {
        *error = "the probe runs no guest threads";
        return false;
    }
    [[noreturn]] void ExitThread(uint32_t) override { std::abort(); }
    uint32_t CallGuest(GuestAddress, const uint32_t*, int, bool* called) override
    {
        *called = false;
        return 0;
    }

private:
    FakeCpu cpu_;
};

constexpr GuestAddress kStackTop = 0x00080000;
constexpr GuestAddress kScratch = 0x00090000;  // argument blocks
constexpr GuestAddress kOut = kScratch + 0x000;     // out parameters
constexpr GuestAddress kDesc = kScratch + 0x100;    // DSBUFFERDESC
constexpr GuestAddress kFormat = kScratch + 0x140;  // WAVEFORMATEX
constexpr GuestAddress kBins = kScratch + 0x180;    // DSMIXBINS + pairs
constexpr GuestAddress kData = kScratch + 0x1000;   // caller-owned buffer data
constexpr GuestAddress kImage = kScratch + 0x8000;  // a synthetic effects image

constexpr uint32_t kOk = 0;
constexpr uint32_t kInvalidCall = 0x88780032;

rexbox::runtime::GuestMemory* g_memory = nullptr;
rexbox::runtime::HleRegistry* g_registry = nullptr;
FakeExecutor* g_executor = nullptr;
int g_failures = 0;
int g_checks = 0;

// Calls export `name` as stdcall with `args`; returns eax. Fails the check if
// the export did not pop exactly its arguments.
uint32_t Call(const char* name, std::initializer_list<uint32_t> args)
{
    const rexbox::runtime::HleExport* entry = g_registry->FindExport(name);
    if (entry == nullptr)
    {
        std::printf("FAIL %s is not exported\n", name);
        ++g_failures;
        return 0xFFFFFFFF;
    }
    GuestAddress esp = kStackTop;
    const uint32_t* values = args.begin();
    for (size_t i = args.size(); i > 0; --i)
    {
        esp -= 4;
        g_memory->Write32(esp, values[i - 1]);
    }
    esp -= 4;
    g_memory->Write32(esp, 0xDEAD0000);  // return address
    rexbox::runtime::GuestCpu& cpu = g_executor->Cpu();
    cpu.Set(Gpr::kEsp, esp);
    entry->thunk();
    if (cpu.Get(Gpr::kEsp) != kStackTop)
    {
        std::printf("FAIL %s left esp %08X, expected %08X\n", name, cpu.Get(Gpr::kEsp), kStackTop);
        ++g_failures;
    }
    return cpu.Get(Gpr::kEax);
}

void Check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
    }
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}

bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-4f;
}

uint32_t Read32(GuestAddress address)
{
    return g_memory->Read32(address);
}

std::vector<float> Render(uint32_t frames)
{
    std::vector<float> samples(static_cast<size_t>(frames) * 2);
    rexbox::hle::dsound::RenderDsoundFrames(samples.data(), frames);
    return samples;
}

GuestAddress g_device = 0;

// Creates a buffer: format tag, channels, rate, bits, block align, size, flags,
// optional mix bins (count, pairs at kBins + 8).
GuestAddress CreateBuffer(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t bits, uint16_t align,
                          uint32_t bytes, uint32_t flags = 0, uint32_t bin_count = 0)
{
    g_memory->Fill(kDesc, 0, 0x18);
    g_memory->Fill(kFormat, 0, 0x14);
    g_memory->Write16(kFormat + 0x0, tag);
    g_memory->Write16(kFormat + 0x2, channels);
    g_memory->Write32(kFormat + 0x4, rate);
    g_memory->Write32(kFormat + 0x8, rate * align);
    g_memory->Write16(kFormat + 0xC, align);
    g_memory->Write16(kFormat + 0xE, bits);
    g_memory->Write32(kDesc + 0x0, 0x18);
    g_memory->Write32(kDesc + 0x4, flags);
    g_memory->Write32(kDesc + 0x8, bytes);
    g_memory->Write32(kDesc + 0xC, tag == 0 ? 0 : kFormat);
    if (bin_count != 0)
    {
        g_memory->Write32(kBins, bin_count);
        g_memory->Write32(kBins + 4, kBins + 8);
        g_memory->Write32(kDesc + 0x10, kBins);
    }
    g_memory->Write32(kOut, 0);
    const uint32_t hr = Call("IDirectSound_CreateSoundBuffer", {g_device, kDesc, kOut, 0});
    return hr == kOk ? Read32(kOut) : 0;
}

// Fills a buffer's data with one 16-bit value per sample.
void Fill16(GuestAddress data, uint32_t bytes, int16_t value)
{
    for (uint32_t i = 0; i + 1 < bytes; i += 2)
    {
        g_memory->Write16(data + i, static_cast<uint16_t>(value));
    }
}

GuestAddress DataOf(GuestAddress buffer)
{
    Call("IDirectSoundBuffer_Lock", {buffer, 0, 0, kOut, kOut + 4, 0, 0, 2});  // DSBLOCK_ENTIREBUFFER
    return Read32(kOut);
}

void Cursors(GuestAddress buffer, uint32_t* play, uint32_t* write)
{
    Call("IDirectSoundBuffer_GetCurrentPosition", {buffer, kOut + 0x20, kOut + 0x24});
    *play = Read32(kOut + 0x20);
    *write = Read32(kOut + 0x24);
}

uint32_t Status(GuestAddress buffer)
{
    Call("IDirectSoundBuffer_GetStatus", {buffer, kOut + 0x28});
    return Read32(kOut + 0x28);
}

void ProbeDevice()
{
    Check(Call("DirectSoundCreate", {0, kOut, 0}) == kOk && Read32(kOut) != 0, "DirectSoundCreate returns an object");
    g_device = Read32(kOut);
    Call("DirectSoundCreate", {0, kOut, 0});
    Check(Read32(kOut) == g_device, "a second DirectSoundCreate returns the same object");
    Check(Call("IDirectSound_AddRef", {g_device}) == 3, "AddRef counts both creations");
    Check(Call("IDirectSound_Release", {g_device}) == 2, "Release drops one reference");
    Call("IDirectSound_GetSpeakerConfig", {g_device, kOut});
    Check(Read32(kOut) == 0, "speaker configuration is stereo");
}

void ProbeLock()
{
    const GuestAddress buffer = CreateBuffer(1, 1, 48000, 16, 2, 4800);
    Check(buffer != 0, "CreateSoundBuffer: PCM 16-bit mono");
    const GuestAddress data = DataOf(buffer);
    Check(data != 0 && Read32(kOut + 4) == 4800, "Lock(ENTIREBUFFER) covers the buffer");
    Call("IDirectSoundBuffer_Lock", {buffer, 4000, 1600, kOut, kOut + 4, kOut + 8, kOut + 12, 0});
    Check(Read32(kOut) == data + 4000 && Read32(kOut + 4) == 800 && Read32(kOut + 8) == data &&
              Read32(kOut + 12) == 800,
          "Lock past the end wraps into the second part");
    Call("IDirectSoundBuffer_Lock", {buffer, 100, 200, kOut, kOut + 4, kOut + 8, kOut + 12, 0});
    Check(Read32(kOut + 4) == 200 && Read32(kOut + 8) == 0 && Read32(kOut + 12) == 0,
          "Lock inside the buffer leaves the second part empty");
    Check(Call("IDirectSoundBuffer_Unlock", {buffer, data, 200, 0, 0}) == kOk, "Unlock succeeds");
    Call("IDirectSoundBuffer_Release", {buffer});
}

void ProbePlayback()
{
    const GuestAddress buffer = CreateBuffer(1, 1, 48000, 16, 2, 4800);  // 2400 frames
    Fill16(DataOf(buffer), 4800, 16384);
    Check(Status(buffer) == 0, "a new buffer is stopped");
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 0});
    Check(Status(buffer) == 1, "Play: status PLAYING");
    std::vector<float> out = Render(1000);
    Check(Near(out[0], 0.5f) && Near(out[1], 0.5f), "a mono buffer without mix bins plays on both sides");
    uint32_t play = 0;
    uint32_t write = 0;
    Cursors(buffer, &play, &write);
    Check(play == 2000 && write == 2064, "cursors after 1000 frames: play 2000, write 32 samples ahead");
    out = Render(2000);
    Check(Near(out[2 * 1399], 0.5f) && Near(out[2 * 1400], 0.0f), "a one-shot buffer stops at its end");
    Cursors(buffer, &play, &write);
    Check(Status(buffer) == 0 && play == 0 && write == 0, "after the end: stopped, position 0");

    // Stop keeps the position; Play resumes there; FROMSTART restarts.
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 0});
    Render(500);
    Call("IDirectSoundBuffer_Stop", {buffer});
    Cursors(buffer, &play, &write);
    Check(Status(buffer) == 0 && play == 1000 && write == 1000, "Stop saves the position");
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 0});
    Render(100);
    Cursors(buffer, &play, &write);
    Check(play == 1200, "Play resumes at the saved position");
    Call("IDirectSoundBuffer_Stop", {buffer});
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 2});  // DSBPLAY_FROMSTART
    Cursors(buffer, &play, &write);
    Check(play == 0, "Play(FROMSTART) starts at 0");
    Call("IDirectSoundBuffer_Stop", {buffer});
    Call("IDirectSoundBuffer_SetCurrentPosition", {buffer, 3000});
    Cursors(buffer, &play, &write);
    Check(play == 3000, "SetCurrentPosition while stopped sets the saved position");

    // Looping inside a loop region: frames 500..1000.
    Call("IDirectSoundBuffer_SetLoopRegion", {buffer, 1000, 1000});
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 3});  // LOOPING | FROMSTART
    Check(Status(buffer) == 5, "Play(LOOPING): status PLAYING | LOOPING");
    Render(1200);
    Cursors(buffer, &play, &write);
    Check(play == 1400 && write == 1464, "a looping buffer wraps into its loop region");
    Call("IDirectSoundBuffer_Stop", {buffer});

    // Frequency and volume.
    Call("IDirectSoundBuffer_SetFrequency", {buffer, 24000});
    Call("IDirectSoundBuffer_SetVolume", {buffer, static_cast<uint32_t>(-600)});
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 2});
    out = Render(1000);
    Cursors(buffer, &play, &write);
    Check(play == 1000, "SetFrequency(24000) plays at half speed");
    Check(Near(out[0], 0.5f * std::pow(10.0f, -0.3f)), "SetVolume(-600) attenuates by 6 dB");
    Call("IDirectSoundBuffer_SetVolume", {buffer, static_cast<uint32_t>(-10000)});
    out = Render(10);
    Check(out[0] == 0.0f, "SetVolume(-10000) is silent");

    // Moving a playing one-shot to its end stops it without reading past it.
    Call("IDirectSoundBuffer_SetVolume", {buffer, 0});
    Call("IDirectSoundBuffer_SetCurrentPosition", {buffer, 4800});
    out = Render(4);
    Cursors(buffer, &play, &write);
    Check(out[0] == 0.0f && Status(buffer) == 0 && play == 0, "SetCurrentPosition(end) on a playing one-shot ends it");
    // A loop start past the end still gives cursors inside the buffer.
    Call("IDirectSoundBuffer_SetLoopRegion", {buffer, 9000, 0});
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 3});
    Render(100);
    Cursors(buffer, &play, &write);
    Check(play < 4800 && write < 4800, "a loop start past the end keeps the cursors in the buffer");
    Call("IDirectSoundBuffer_Release", {buffer});
}

void ProbeFormats()
{
    // Stereo with explicit mix bins: left channel to FRONT_RIGHT only.
    g_memory->Write32(kBins + 8, 1);  // pair 0: channel 0 -> FRONT_RIGHT
    g_memory->Write32(kBins + 12, 0);
    g_memory->Write32(kBins + 16, 0);  // pair 1: channel 1 -> FRONT_LEFT
    g_memory->Write32(kBins + 20, 0);
    const GuestAddress stereo = CreateBuffer(1, 2, 48000, 16, 4, 400, 0, 2);
    const GuestAddress data = DataOf(stereo);
    for (uint32_t i = 0; i < 400; i += 4)
    {
        g_memory->Write16(data + i, 8192);       // channel 0: 0.25
        g_memory->Write16(data + i + 2, 24576);  // channel 1: 0.75
    }
    Call("IDirectSoundBuffer_Play", {stereo, 0, 0, 0});
    std::vector<float> out = Render(4);
    Check(Near(out[0], 0.75f) && Near(out[1], 0.25f), "mix bins route each channel to its bin");
    Call("IDirectSoundBuffer_Release", {stereo});

    // 8-bit PCM is unsigned.
    const GuestAddress pcm8 = CreateBuffer(1, 1, 48000, 8, 1, 100);
    g_memory->Fill(DataOf(pcm8), 192, 100);
    Call("IDirectSoundBuffer_Play", {pcm8, 0, 0, 0});
    out = Render(4);
    Check(Near(out[0], 0.5f), "8-bit PCM: 192 plays as 0.5");
    Call("IDirectSoundBuffer_Release", {pcm8});

    // Xbox ADPCM: header {1000, index 0}, nibbles 4, 4, then zeros.
    const GuestAddress adpcm = CreateBuffer(0x69, 1, 48000, 4, 36, 36);
    const GuestAddress block = DataOf(adpcm);
    Check(adpcm != 0, "CreateSoundBuffer: Xbox ADPCM mono");
    g_memory->Fill(block, 0, 36);
    g_memory->Write16(block, 1000);
    g_memory->Write8(block + 4, 0x44);
    Call("IDirectSoundBuffer_Play", {adpcm, 0, 0, 0});
    out = Render(70);
    Check(Near(out[0], 1000.0f / 32768) && Near(out[2], 1007.0f / 32768) && Near(out[4], 1017.0f / 32768),
          "ADPCM decodes header sample then IMA nibbles");
    // Zero nibbles still add step / 8 until the step index falls to 0: 1018..1021.
    Check(Near(out[2 * 6], 1021.0f / 32768) && Near(out[2 * 63], 1021.0f / 32768) && out[2 * 64] == 0.0f,
          "an ADPCM block plays 64 samples");
    uint32_t play = 0;
    uint32_t write = 0;
    Cursors(adpcm, &play, &write);
    Check(Status(adpcm) == 0 && play == 0, "the one-block ADPCM buffer has ended");
    Call("IDirectSoundBuffer_Release", {adpcm});
}

void ProbeBufferData()
{
    const GuestAddress buffer = CreateBuffer(1, 1, 48000, 16, 2, 0);
    Check(buffer != 0, "CreateSoundBuffer with no data");
    Fill16(kData, 200, -16384);
    Check(Call("IDirectSoundBuffer_SetBufferData", {buffer, kData, 200}) == kOk, "SetBufferData accepts caller memory");
    Call("IDirectSoundBuffer_Play", {buffer, 0, 0, 0});
    std::vector<float> out = Render(4);
    Check(Near(out[0], -0.5f), "the buffer plays the caller's memory");
    g_memory->Write16(kData + 8, 16384);  // written while playing, no Unlock
    out = Render(2);
    Check(Near(out[0], 0.5f), "writes into a playing buffer are heard");
    Call("IDirectSoundBuffer_Release", {buffer});

    const GuestAddress submix = CreateBuffer(0, 0, 0, 0, 0, 0, 0x2000);  // DSBCAPS_MIXIN, no format
    Check(submix != 0, "a MIXIN buffer needs no format");
    Call("IDirectSoundBuffer_Play", {submix, 0, 0, 1});
    Check(Status(submix) == 5, "a MIXIN buffer reports playing");
    Call("IDirectSoundBuffer_Release", {submix});

    Call("IDirectSound_GetCaps", {g_device, kOut});
    Check(Read32(kOut) == 256 && Read32(kOut + 12) == 0, "GetCaps after every buffer is released");
}

void ProbeEffects()
{
    // A two-effect image: code 2 words, state 4 words, table after them.
    g_memory->Fill(kImage, 0, 0x1000);
    g_memory->Write32(kImage + 0x804, 2);
    g_memory->Write32(kImage + 0x80C, 4);
    const GuestAddress table = kImage + 0x818 + 6 * 4;
    g_memory->Write32(kImage + 0x820, 0x11111111);  // state of effect 0
    g_memory->Write32(kImage + 0x824, 0x22222222);
    g_memory->Write32(kImage + 0x828, 0x33333333);  // state of effect 1
    g_memory->Write32(table + 0, 2);
    g_memory->Write32(table + 4, 0x100);
    const uint32_t maps[2][8] = {{0x818, 4, 0x820, 8, 0, 0, 0xC000, 0x10}, {0x81C, 4, 0x828, 8, 0, 0x10, 0xC010, 0}};
    for (uint32_t e = 0; e < 2; ++e)
    {
        for (uint32_t f = 0; f < 8; ++f)
        {
            g_memory->Write32(table + 8 + e * 0x20 + f * 4, maps[e][f]);
        }
    }
    const uint32_t size = 0x818 + 6 * 4 + 8 + 2 * 0x20;
    Check(Call("IDirectSound_DownloadEffectsImage", {g_device, kImage, size, 0, kOut}) == kOk,
          "DownloadEffectsImage accepts the image");
    const GuestAddress desc = Read32(kOut);
    Check(desc != 0 && Read32(desc) == 2 && Read32(desc + 4) == 0x100, "the descriptor has the table's counts");
    Check(Read32(Read32(desc + 8 + 8)) == 0x11111111, "effect 0's state pointer reaches its initial state");
    Check(Read32(desc + 8 + 0x18) + 0x10 == Read32(desc + 0x28 + 0x18), "scratch pointers keep their offsets");
    Call("IDirectSound_GetEffectData", {g_device, 0, 4, kOut + 0x40, 4});
    Check(Read32(kOut + 0x40) == 0x22222222, "GetEffectData reads state at an offset");
    g_memory->Write32(kOut + 0x44, 0xABCDEF01);
    Call("IDirectSound_SetEffectData", {g_device, 1, 4, kOut + 0x44, 4, 0});
    Call("IDirectSound_GetEffectData", {g_device, 1, 4, kOut + 0x48, 4});
    Check(Read32(kOut + 0x48) == 0xABCDEF01, "SetEffectData writes the state GetEffectData reads");
    Check(Call("IDirectSound_GetEffectData", {g_device, 2, 0, kOut + 0x40, 4}) == kInvalidCall,
          "an effect index past the table is DSERR_INVALIDCALL");
}

void ProbeAccepted()
{
    const GuestAddress buffer = CreateBuffer(1, 1, 48000, 16, 2, 64, 0x10);
    // Each accepted setting pops its own argument count (checked by Call).
    Call("IDirectSoundBuffer_SetPosition", {buffer, 0, 0, 0, 0});
    Call("IDirectSoundBuffer_SetConeAngles", {buffer, 0, 0, 0});
    Call("IDirectSoundBuffer_SetEG", {buffer, 0});
    Call("IDirectSound_SetOrientation", {g_device, 0, 0, 0, 0, 0, 0, 0});
    Call("IDirectSound_CommitDeferredSettings", {g_device});
    Call("DirectSoundDoWork", {});
    Check(Call("IDirectSoundBuffer_Release", {buffer}) == 0, "accepted settings leave the buffer intact");
}

}  // namespace

int main()
{
    rexbox::runtime::GuestMemory memory;
    if (!memory.Initialize(16u * 1024u * 1024u))
    {
        std::fprintf(stderr, "cannot reserve guest memory\n");
        return 2;
    }
    rexbox::runtime::GuestHeap heap(memory.size());
    heap.Reserve(0, 0x00100000);  // the probe's stack and scratch area
    rexbox::runtime::HleRegistry registry;
    FakeExecutor executor;
    rexbox::runtime::SetEnvironment({&memory, &heap, &registry, &executor});
    g_memory = &memory;
    g_registry = &registry;
    g_executor = &executor;

    rexbox::hle::dsound::SetDsoundOutput(rexbox::hle::dsound::DsoundOutput::kManual);
    rexbox::hle::dsound::RegisterDsoundHle(registry);

    ProbeDevice();
    ProbeLock();
    ProbePlayback();
    ProbeFormats();
    ProbeBufferData();
    ProbeEffects();
    ProbeAccepted();

    std::printf("%d check(s), %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
