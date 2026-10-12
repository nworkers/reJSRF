// The effects image: DownloadEffectsImage builds the image descriptor from
// the image's plaintext effect table, and GetEffectData/SetEffectData copy
// to and from the effects' state segments, as the original does. The DSP code
// itself (the encrypted part of the image) never runs (design section 6).

#include "dsound_internal.h"

#include <algorithm>
#include <cstring>

namespace rexbox::hle::dsound
{

namespace
{

// The original relocates scratch offsets by subtracting this base.
constexpr uint32_t kScratchOffsetBase = 0xC000;
constexpr uint32_t kMaxEffects = 1024;

void FreeImageLocked(EffectsImage& image)
{
    for (const GuestAddress block : {image.copy, image.descriptor, image.y_memory, image.scratch})
    {
        if (block != 0)
        {
            Heap().Free(block);
        }
    }
    image = EffectsImage{};
}

// The guest address of effect `index`'s state segment, or 0.
GuestAddress EffectState(const EffectsImage& image, uint32_t index, uint32_t* state_size)
{
    if (image.descriptor == 0 || index >= image.effect_count)
    {
        return 0;
    }
    const GuestAddress map = image.descriptor + layout::kImageDescMaps + index * layout::kEffectMapSize;
    *state_size = Memory().Read32(map + layout::kMapStateSize);
    return Memory().Read32(map + layout::kMapState);
}

// HRESULT IDirectSound_DownloadEffectsImage(LPDIRECTSOUND pThis, LPCVOID pvImageBuffer,
//     DWORD dwImageSize, LPCDSEFFECTIMAGELOC pImageLoc, LPDSEFFECTIMAGEDESC* ppImageDesc)
void IDirectSound_DownloadEffectsImage()
{
    HleCall call;
    const GuestAddress image = call.Arg(1);
    const uint32_t size = call.Arg(2);
    const GuestAddress out = call.Arg(4);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    runtime::GuestMemory& memory = Memory();
    if (image == 0 || size < layout::kImageBody || !memory.IsValid(image, size))
    {
        call.Return(kErrInvalidCall, 5);
        return;
    }
    const uint64_t words = static_cast<uint64_t>(memory.Read32(image + layout::kImageCodeWords)) +
                           memory.Read32(image + layout::kImageStateWords);
    const uint64_t table = layout::kImageBody + words * 4;
    if (table + layout::kImageDescMaps > size)
    {
        std::fprintf(stderr, "[dsound] DownloadEffectsImage: the effect table lies outside the %u-byte image\n", size);
        call.Return(kErrInvalidCall, 5);
        return;
    }
    const GuestAddress source = image + static_cast<uint32_t>(table);
    const uint32_t count = memory.Read32(source + layout::kImageDescCount);
    const uint32_t scratch_size = memory.Read32(source + layout::kImageDescScratchSize);
    const uint64_t descriptor_size = layout::kImageDescMaps + static_cast<uint64_t>(count) * layout::kEffectMapSize;
    if (count > kMaxEffects || table + descriptor_size > size)
    {
        std::fprintf(stderr, "[dsound] DownloadEffectsImage: %u effects do not fit the image\n", count);
        call.Return(kErrInvalidCall, 5);
        return;
    }

    FreeImageLocked(state.effects);
    EffectsImage& effects = state.effects;
    uint32_t y_size = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const GuestAddress map = source + layout::kImageDescMaps + i * layout::kEffectMapSize;
        y_size = std::max(y_size, memory.Read32(map + layout::kMapYMemory) + memory.Read32(map + layout::kMapYMemorySize));
    }
    effects.copy = AllocateGuest(size);
    effects.descriptor = AllocateGuest(static_cast<uint32_t>(descriptor_size));
    effects.y_memory = AllocateGuest(std::max<uint32_t>(y_size, 4));
    effects.scratch = AllocateGuest(std::max<uint32_t>(scratch_size, 4));
    if (effects.copy == 0 || effects.descriptor == 0 || effects.y_memory == 0 || effects.scratch == 0)
    {
        FreeImageLocked(effects);
        call.Return(kErrOutOfMemory, 5);
        return;
    }
    std::memcpy(memory.Translate(effects.copy), memory.Translate(image), size);
    effects.effect_count = count;
    memory.Write32(effects.descriptor + layout::kImageDescCount, count);
    memory.Write32(effects.descriptor + layout::kImageDescScratchSize, scratch_size);
    for (uint32_t i = 0; i < count; ++i)
    {
        const GuestAddress from = source + layout::kImageDescMaps + i * layout::kEffectMapSize;
        const GuestAddress to = effects.descriptor + layout::kImageDescMaps + i * layout::kEffectMapSize;
        const uint32_t code = memory.Read32(from + layout::kMapCode);
        const uint32_t code_size = memory.Read32(from + layout::kMapCodeSize);
        const uint32_t state_offset = memory.Read32(from + layout::kMapState);
        const uint32_t state_size = memory.Read32(from + layout::kMapStateSize);
        const uint32_t scratch = memory.Read32(from + layout::kMapScratch);
        const uint32_t scratch_bytes = memory.Read32(from + layout::kMapScratchSize);
        const bool code_ok = static_cast<uint64_t>(code) + code_size <= size;
        const bool state_ok = static_cast<uint64_t>(state_offset) + state_size <= size;
        const bool scratch_ok = scratch >= kScratchOffsetBase &&
                                static_cast<uint64_t>(scratch - kScratchOffsetBase) + scratch_bytes <= scratch_size;
        if (!code_ok || !state_ok)
        {
            std::fprintf(stderr, "[dsound] DownloadEffectsImage: effect %u's segments lie outside the image\n", i);
        }
        memory.Write32(to + layout::kMapCode, code_ok ? effects.copy + code : 0);
        memory.Write32(to + layout::kMapCodeSize, code_size);
        memory.Write32(to + layout::kMapState, state_ok ? effects.copy + state_offset : 0);
        memory.Write32(to + layout::kMapStateSize, state_ok ? state_size : 0);
        memory.Write32(to + layout::kMapYMemory, effects.y_memory + memory.Read32(from + layout::kMapYMemory));
        memory.Write32(to + layout::kMapYMemorySize, memory.Read32(from + layout::kMapYMemorySize));
        memory.Write32(to + layout::kMapScratch, scratch_ok ? effects.scratch + (scratch - kScratchOffsetBase) : 0);
        memory.Write32(to + layout::kMapScratchSize, scratch_bytes);
    }
    if (out != 0)
    {
        memory.Write32(out, effects.descriptor);
    }
    std::fprintf(stderr, "[dsound] DownloadEffectsImage: %u effects, %u bytes of scratch -> descriptor %08X\n", count,
                 scratch_size, effects.descriptor);
    call.Return(kDsOk, 5);
}

// HRESULT IDirectSound_GetEffectData(LPDIRECTSOUND pThis, DWORD dwEffectIndex,
//     DWORD dwOffset, LPVOID pvData, DWORD dwDataSize)
void IDirectSound_GetEffectData()
{
    HleCall call;
    const uint32_t index = call.Arg(1);
    const uint32_t offset = call.Arg(2);
    const GuestAddress data = call.Arg(3);
    const uint32_t size = call.Arg(4);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint32_t state_size = 0;
    const GuestAddress segment = EffectState(state.effects, index, &state_size);
    runtime::GuestMemory& memory = Memory();
    if (segment == 0 || !memory.IsValid(segment + offset, size) || !memory.IsValid(data, size))
    {
        call.Return(kErrInvalidCall, 5);
        return;
    }
    std::memmove(memory.Translate(data), memory.Translate(segment + offset), size);
    call.Return(kDsOk, 5);
}

// HRESULT IDirectSound_SetEffectData(LPDIRECTSOUND pThis, DWORD dwEffectIndex,
//     DWORD dwOffset, LPCVOID pvData, DWORD dwDataSize, DWORD dwApply)
void IDirectSound_SetEffectData()
{
    HleCall call;
    const uint32_t index = call.Arg(1);
    const uint32_t offset = call.Arg(2);
    const GuestAddress data = call.Arg(3);
    const uint32_t size = call.Arg(4);
    DsoundState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint32_t state_size = 0;
    const GuestAddress segment = EffectState(state.effects, index, &state_size);
    runtime::GuestMemory& memory = Memory();
    if (segment == 0 || !memory.IsValid(segment + offset, size) || !memory.IsValid(data, size))
    {
        call.Return(kErrInvalidCall, 6);
        return;
    }
    std::memmove(memory.Translate(segment + offset), memory.Translate(data), size);
    call.Return(kDsOk, 6);
}

}  // namespace

void RegisterEffectsExports(runtime::HleRegistry& registry)
{
    registry.AddExport("dsound", "IDirectSound_DownloadEffectsImage", &IDirectSound_DownloadEffectsImage);
    registry.AddExport("dsound", "IDirectSound_GetEffectData", &IDirectSound_GetEffectData);
    registry.AddExport("dsound", "IDirectSound_SetEffectData", &IDirectSound_SetEffectData);
}

}  // namespace rexbox::hle::dsound
