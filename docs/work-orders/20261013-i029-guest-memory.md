# 작업 지시: 게스트 메모리 부족 (issue #29)

설계: [20261013-i029](../design/20261013-i029-guest-memory.md)

1. 커널: base 0인 64 KiB 미만 `NtAllocateVirtualMemory`를 페이지 정렬하고 `GuestHeap::AllocateVirtual`로 할당한다. 메모리 부족 trace에 남은 바이트와 최대 연속 구간을 넣는다. `MmClaimGpuInstanceMemory`의 끝 질의는 64 MiB를 돌려준다.
2. 런타임: `GuestHeap`에 slab(`AllocateSmall`), 물리/가상 영역(`physical_size`, `AllocateVirtual`), `largest_free_bytes`를 더한다. `GuestMemory::kAddressSpaceSize`를 두고 앱이 128 MiB를 매핑한다.
3. HLE: D3D 헤더·셰이더 객체, DSOUND `AllocateGuest`, XInput 핸들, 커널 `AllocateZeroed`(페이지 미만 정렬)를 `AllocateSmall`로 옮긴다. D3D 그리기·텍스처의 `kPhysicalMask`를 `d3d8_internal.h` 하나로 합친다.
4. kernel probe에 검사를 더하고 Debug/Release 빌드, 회귀 도구, 부팅(메모리 trace, frame dump)으로 확인한다.
5. 문서: i003 §4 편차, boot-frontier, `ARCHITECTURE.md`, Win32 가이드, 작업 로그.

---

# Work order: guest memory runs out (issue #29)

Design: [20261013-i029](../design/20261013-i029-guest-memory.md)

1. Kernel:
   * Page-align base-0 `NtAllocateVirtualMemory` requests under 64 KiB and allocate through `GuestHeap::AllocateVirtual`.
   * Add the free bytes and the longest free run to the out-of-memory trace.
   * Make the end query of `MmClaimGpuInstanceMemory` return 64 MiB.
2. Runtime:
   * Add to `GuestHeap` the slab (`AllocateSmall`), the physical and virtual regions (`physical_size`, `AllocateVirtual`) and `largest_free_bytes`.
   * Add `GuestMemory::kAddressSpaceSize`; the app maps 128 MiB.
3. HLE:
   * Move to `AllocateSmall`: D3D headers and shader objects, DSOUND `AllocateGuest`, XInput handles, and kernel `AllocateZeroed` with sub-page alignment.
   * Merge the D3D drawing and texture `kPhysicalMask` into one in `d3d8_internal.h`.
4. Add kernel probe checks. Verify with Debug and Release builds, the regression tools and a boot with the memory trace and frame dumps.
5. Documents: the i003 §4 deviation, boot-frontier, `ARCHITECTURE.md`, the Win32 guide, the work log.
