# Issue #29 작업 로그 — 스테이지 로드 중 게스트 메모리 부족

작업 지시: [20261013-i029](../work-orders/20261013-i029-guest-memory.md) · 설계: [20261013-i029](../design/20261013-i029-guest-memory.md)

## 요약

로고 시퀀스 뒤 스테이지를 읽다가 `NtAllocateVirtualMemory out of guest memory`로 멈추던 문제를 고쳤다. 원인은 게스트 주소를 64 MiB 하나에 평면으로 매핑해 예약한 페이지가 모두 메모리를 차지한 것이다. 작은 예약을 페이지 정렬하고, 작은 HLE 객체와 풀 블록에 slab을 쓰고, 게스트 주소 공간을 128 MiB로 늘려 위 64 MiB를 가상 메모리 전용으로 두었다. 이제 부팅이 스테이지 로드를 지나 3D 어트랙트 데모까지 간다.

## 과정

1. `REXBOX_TRACE_MEMORY` 로그를 스크립트로 집계했다. base 0인 작은 요청 315건(합 0x5CF50바이트)이 64 KiB 단위로 올림되어 약 19 MiB를 차지했다.
2. 64 KiB 미만 요청을 페이지 정렬하자 부팅이 어트랙트 데모까지 갔다. 그래도 힙 확장의 8 MiB 요청은 실패했다(게임은 4 → 2 → 1 MiB로 다시 시도한다). 이때 남은 메모리는 8.2 MiB, 최대 연속 구간은 6.4 MiB였다.
3. 예약만 한 블록을 커밋 기록과 맞춰 보았다. 12.1 MiB 예약 가운데 3.8 MiB가 끝까지 커밋되지 않았다(예: `0x03910000`의 4 MiB 중 312 KiB). Xbox에서는 주소 공간만 쓰는 몫이다.
4. HLE의 작은 객체(D3D 헤더 0x18~0x40바이트, 정점 셰이더 객체, DSOUND 0x40바이트 블록, XInput 핸들, 커널 풀·이벤트)가 한 페이지씩 차지했다. 이것은 `GuestHeap::AllocateSmall`(크기 등급 slab)로 옮겼다. 이것만으로 남는 메모리는 약 260 KiB 늘었고 8 MiB 요청은 여전히 실패했다.
5. 게스트 주소 공간을 128 MiB로 늘렸다. 아래 64 MiB는 물리 RAM으로 연속 메모리, 풀, HLE 객체가 쓰고, 위 64 MiB는 `NtAllocateVirtualMemory`만 쓴다. `D3DResource_Register`는 물리 주소를 `0x03FFFFFF`로 자르므로 연속 메모리는 아래에 남아야 한다. 그리기와 텍스처 캐시의 마스크는 index 데이터처럼 가상 메모리에 있을 수 있는 주소를 잃지 않도록 `0x07FFFFFF`로 넓혔다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug/Release 빌드 | 성공, 경고 0 |
| kernel probe | 56/0(새 검사 14건: 작은 예약 정렬, 풀 slab, 힙 영역) |
| dsound / d3d8 probe | 48/0, 81/0 |
| d3d8 check / decomp check | 14,018/0, 6/0 |
| 부팅(숨김 창, 150초, 메모리 trace, frame dump) | 메모리 부족 0건, watchdog 종료. 가상 메모리는 `0x04000000`~`0x05830000`. 5,400·7,200·8,400번째 present에 도시 데모, 3,600번째는 검은 화면 |

## 남은 일

* 정점 셰이더 프로그램 그리기(i023 §9), `Begin`/`End` 즉시 모드, 고정 기능 조명, 텍스처 변환·좌표 생성.
* 3,600번째 present가 검은 원인.
* 커밋 총량이 64 MiB를 넘어도 실패하지 않는 편차(설계 §3).

---

# Issue #29 work log — guest memory runs out while loading the stage

Work order: [20261013-i029](../work-orders/20261013-i029-guest-memory.md) · Design: [20261013-i029](../design/20261013-i029-guest-memory.md)

## Summary

After the logo sequence, the boot stopped while reading the stage with `NtAllocateVirtualMemory out of guest memory`. Guest addresses were mapped flat onto one 64 MiB block, so every reserved page took memory. The fix has three parts:

* Small reservations take page alignment.
* Small HLE objects and pool blocks use slabs.
* The guest address space is 128 MiB, with the upper 64 MiB for virtual memory only.

The boot now gets past the stage load into the 3D attract demo.

## Process

1. A script tallied the `REXBOX_TRACE_MEMORY` log. 315 small base-0 requests (0x5CF50 bytes in total) were rounded to 64 KiB and took about 19 MiB.
2. Page-aligning requests under 64 KiB let the boot reach the attract demo. The 8 MiB heap-growth request still failed (the title retries with 4, 2 and 1 MiB). At that point 8.2 MiB was free, with a longest free run of 6.4 MiB.
3. Matching reserve-only blocks against their commits showed 3.8 MiB of the 12.1 MiB reserved never committed (e.g. 312 KiB of the 4 MiB at `0x03910000`). On the Xbox that costs only address space.
4. Small HLE objects each took a page:
   * D3D headers of 0x18–0x40 bytes
   * vertex shader objects
   * DSOUND 0x40-byte blocks
   * XInput handles
   * kernel pool blocks and events

   They moved to `GuestHeap::AllocateSmall` (size-class slabs). That alone freed about 260 KiB more, and the 8 MiB request still failed.
5. The guest address space grew to 128 MiB.
   * The low 64 MiB is physical RAM: contiguous memory, pool and HLE objects.
   * The upper 64 MiB serves only `NtAllocateVirtualMemory`.
   * `D3DResource_Register` cuts physical addresses to `0x03FFFFFF`, so contiguous memory must stay low.
   * The masks in drawing and the texture cache widen to `0x07FFFFFF`, so addresses that can be in virtual memory, such as index data, are not lost.

## Verification

| Check | Result |
|---|---|
| Win32 Debug and Release builds | succeed with zero warnings |
| kernel probe | 56/0, including 14 new checks: small-reservation alignment, pool slabs, heap regions |
| dsound and d3d8 probes | 48/0 and 81/0 |
| d3d8 check and decomp check | 14,018/0 and 6/0 |
| 150 s hidden-window boot with the memory trace and frame dumps | no memory exhaustion; ends by watchdog; virtual memory at `0x04000000`–`0x05830000`; the city demo in presents 5,400, 7,200 and 8,400; present 3,600 is black |

## Remaining

* Drawing with vertex-shader programs (i023 §9), `Begin`/`End` immediate mode, fixed-function lighting, texture transforms and coordinate generation.
* Why present 3,600 is black.
* The deviation that committed memory beyond 64 MiB does not fail (design §3).
