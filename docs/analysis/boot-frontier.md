# 부팅 frontier

재컴파일된 US 빌드를 진입점부터 실행했을 때 어디까지 가는지 기록한다. 작업마다 갱신하며, 지난 frontier는 아래 이력에 남긴다.

This records how far the recompiled US build gets when run from its entry point. It is updated by each task; earlier frontiers stay in the history below.

## 현재 / Current (issue #12, 2026-10-12, 진행 중 / in progress)

| 항목 / Item | 값 / Value |
|---|---|
| 생성 코드 / Generated code | 함수 8,721개, D3D8 override 136개, D3D8 HLE export 110개, lifter 수정(`lock xadd` 플래그) 포함 / 8,721 functions, 136 D3D8 overrides, 110 D3D8 HLE exports, lifter fix (`lock xadd` flags) |
| 실행 결과 / Outcome | **D3D8 초기화와 게임 객체 생성을 지나 thread 2가 미구현 커널 import 65 `IoCreateDevice`에서 정지 (약 5초) / past D3D8 initialization and the game's object setup, thread 2 stops at the unimplemented kernel import 65 `IoCreateDevice` (about 5 s)** |

* **확인됨**: 게임의 COM 식 `Release`(`lock xadd [obj+8], -1; jne`)가 lifter 버그(`lock xadd`를 플래그 미정의로 취급 → `jne`가 항상 거짓)로 매번 파괴 경로를 타 객체를 두 번 해제했고, XAPI NT 힙의 `FreeLists` 체인이 순환해 `RtlAllocateHeap`이 멈췼다. 진단: `REXBOX_WATCHDOG`의 직접 호출 프로파일, `REXBOX_HEAP_CHECK`, 메모리 덤프 분석. 수정은 업스트림 PR [xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198).
* **확인됨**(해결): XPP 섹션의 `0x001BD274`는 USB 클래스 드라이버 표(`0x001BC970`)의 초기화 함수로, `KeInitializeTimerEx(0x002649A8, NotificationTimer)`를 부른다. 프롤로그 없이 `ret` 바로 뒤에 있어 탐지되지 않았고 간접 호출이 건너뛰어졌다. 같은 XPP 드라이버 표에서만 참조되는 FPO 함수 8개와 함께 seed로 추가해 해결했다([xdk-symbols §3](xdk-symbols.md)).
* **확인됨**: `IoCreateDevice`는 XAPI `MU_Init`(`0x001BC98C`, `XInitDevices`가 부름)의 `0x001BCA34`에서 호출된다. 메모리 유닛 장치 객체 생성이다.
* **미확정**: `IoCreateDevice` 뒤에 USB 장치 계층(XPP)이 요구하는 커널 서비스. 첫 `D3DDevice_Swap`은 아직 도달하지 않았다.
* **Confirmed**: the game's COM-style `Release` (`lock xadd [obj+8], -1; jne`) took the destroy arm every time because the lifter treated `lock xadd` as leaving undefined flags (the `jne` was always false), freeing objects twice; XAPI's NT heap `FreeLists` chain became circular and `RtlAllocateHeap` spun. Diagnosed with the `REXBOX_WATCHDOG` direct-call profile, `REXBOX_HEAP_CHECK` and a memory dump; fixed upstream in [xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198).
* **Confirmed** (resolved): `0x001BD274` in the XPP section is the init function of a USB class-driver table (`0x001BC970`) and calls `KeInitializeTimerEx(0x002649A8, NotificationTimer)`. It starts right after a `ret` with no prologue, so it was not detected and the indirect call to it was skipped. It was seeded together with eight more FPO functions referenced only from the same XPP driver tables ([xdk-symbols §3](xdk-symbols.md)).
* **Confirmed**: `IoCreateDevice` is called at `0x001BCA34` in XAPI's `MU_Init` (`0x001BC98C`, called by `XInitDevices`): creating the memory-unit device object.
* **Unresolved**: the kernel services the USB device layer (XPP) needs after `IoCreateDevice`. The first `D3DDevice_Swap` has not been reached.

## 이력 / History

| issue | frontier |
|---|---|
| #2 | 첫 실행: 미구현 커널 import 255 `PsCreateSystemThreadEx` / first run: unimplemented kernel import 255 `PsCreateSystemThreadEx` |
| #3 | `NtOpenFile` → `HalReturnToFirmware`(Partition0 없음) → D3D8의 NV2A `0xFD001804` 읽기 / `NtOpenFile` → `HalReturnToFirmware` (no Partition0) → D3D8 reading NV2A `0xFD001804` |
| #6 | 첫 D3D8 API `Direct3D_GetAdapterCount` / the first D3D8 API, `Direct3D_GetAdapterCount` |
| #7 | `Direct3D_CreateDevice` 통과(SDL3 창), `D3DDevice_GetBackBuffer` / past `Direct3D_CreateDevice` (SDL3 window), `D3DDevice_GetBackBuffer` |
| #12 | D3D8 초기화·게임 객체 생성 통과, 커널 import 65 `IoCreateDevice` / past D3D8 initialization and object setup, kernel import 65 `IoCreateDevice` |

### issue #7 frontier 상세 / issue #7 frontier details

| 항목 / Item | 값 / Value |
|---|---|
| 생성 코드 / Generated code | 함수 8,721개, D3D8 override 136개, D3D8 HLE export 8개 / 8,721 functions, 136 D3D8 overrides, 8 D3D8 HLE exports |
| 실행 결과 / Outcome | `Direct3D_CreateDevice`가 SDL3 창(640×480, OpenGL 3.3)을 열고 성공한 뒤, thread 2가 `D3DDevice_GetBackBuffer`(`0x0018D620`)에서 정지 / `Direct3D_CreateDevice` succeeds, opening an SDL3 window (640×480, OpenGL 3.3), then thread 2 stops at `D3DDevice_GetBackBuffer` (`0x0018D620`) |

* **확인됨**: 어댑터·모드·Caps 질의 8개의 HLE 답은 원본과 일치한다(`rejsrf_d3d8_check` 12,202 케이스). 게임은 640×480 `D3DFMT_LIN_R5G6B5`, 백버퍼 2개, 깊이 `LIN_D24S8`, `D3DCREATE_HARDWARE_VERTEXPROCESSING`으로 디바이스를 만든다([d3d8-adapter §5](d3d8-adapter.md)).
* **Confirmed**: the HLE answers of the eight adapter, mode and caps queries match the originals (`rejsrf_d3d8_check`, 12,202 cases); the game creates its device at 640×480 `D3DFMT_LIN_R5G6B5` with 2 back buffers, a `LIN_D24S8` depth buffer and `D3DCREATE_HARDWARE_VERTEXPROCESSING` ([d3d8-adapter §5](d3d8-adapter.md)).

### issue #6 frontier 상세 / issue #6 frontier details

| 항목 / Item | 값 / Value |
|---|---|
| 생성 코드 / Generated code | 함수 8,721개, D3D8 override 136개 / 8,721 functions, 136 D3D8 overrides |
| 실행 결과 / Outcome | thread 2가 첫 D3D8 API `Direct3D_GetAdapterCount`(`0x00199820`)에서 정지 / thread 2 stops at the first D3D8 API, `Direct3D_GetAdapterCount` (`0x00199820`) |

* **확인됨**: D3D8 함수가 모두 HLE 진입점으로 가므로 NV2A 직접 접근 fault는 나지 않는다. 진단 모드에서 원본을 돌리면 `Direct3D_CreateDevice` → `CMiniport_InitHardware`가 `0xFD001804`를 읽는다. 호출 순서는 [xdk-symbols](xdk-symbols.md) §2.
* **Confirmed**: every D3D8 function enters the HLE, so the direct NV2A fault no longer happens; running the originals in diagnostic mode shows `Direct3D_CreateDevice` → `CMiniport_InitHardware` reading `0xFD001804` (call order in [xdk-symbols](xdk-symbols.md) §2).

### issue #3 frontier 상세 / issue #3 frontier details

| 항목 / Item | 값 / Value |
|---|---|
| 생성 코드 / Generated code | xboxrecomp v0.13.1, seed 10개 / 10 seeds, 함수 8,721개 / functions |
| 커널 / Kernel | export 103개, import 120개 중 106개 바인딩 / 103 exports, 106 of 120 imports bound |
| 실행 결과 / Outcome | **thread 2(게임 메인)가 D3D8 초기화에서 NV2A 레지스터 `0xFD001804`를 읽다 fault / thread 2 (game main) faults reading NV2A register `0xFD001804` during D3D8 initialization** |
| 시간 / Time | 약 1.7초(Debug) / about 1.7 s (Debug) |
| 레지스터 / Registers | ecx = esi = `0019D468`(D3D 섹션 안의 디바이스 객체 / device object inside the D3D section), eax `0019D638`, edx `00007F7F` |

### 확인된 부팅 순서 / Confirmed boot sequence

```mermaid
sequenceDiagram
    participant T1 as thread 1 (entry 0x148023)
    participant T2 as thread 2 (XAPI 0x147EBB -> main 0x147FB4)
    participant K as kernel HLE
    T1->>K: PsCreateSystemThreadEx (SystemRoutine 0x147EBB)
    T1->>T1: CloseHandle, ret (ends normally)
    T2->>K: TLS, critical sections, events, timer, settings
    T2->>K: \Device\Harddisk0\Partition1\TDATA|UDATA\5345000a -> T:, U:
    T2->>K: Partition0 offset 0x800 read/write (cache drive config)
    T2->>K: Partition5: geometry, partition info, raw format writes, dismount -> Z:
    T2->>K: AvSendTVEncoderOption(6), ExQueryNonVolatileSetting(...)
    T2->>T2: Direct3D device init reads NV2A 0xFD001804 -> fault
```

* **확인됨**: 진입 스레드는 메인 스레드를 만든 뒤 정상 종료한다(eax 1). 게임 메인 스레드는 XAPI 초기화(T:, U:, Z: 드라이브 준비 포함)를 마치고 D3D8 초기화로 들어간다.
* **확인됨**: 파티션을 장치 이름으로 열면 XAPI는 그 볼륨에 원시 섹터를 써서 포맷한다. HLE는 이 쓰기를 버리는 볼륨 객체로 받아들인다(파일은 호스트 디렉터리에 있으므로 FATX 구조가 필요 없다).
* **확인됨**: seed 없이 생성한 코드에서는 `0x0018AFB0`부터 `0x0018C3C0`까지 10개 주소가 함수로 탐지되지 않아, 부팅 중 그곳으로 가는 간접 호출이 건너뛰어졌다. `src/game/jsrf/recomp/seed_functions.json`으로 탐지시켜 해결했다.
* **확인됨**: fault 주소는 게스트 `0xFD001804`(NV2A MMIO, 64 MiB RAM 밖)다. Win32에서는 base만큼 wrap되어 호스트의 할당되지 않은 주소에 닿아 fault로 드러났다(운이 좋은 경우다; 설계 i003 §6).
* **추정**: D3D8 디바이스 생성(`Direct3D_CreateDevice` 계열)이 GPU를 초기화하는 지점이다. 다음 task는 D3D8을 API 수준에서 HLE해 이 경로를 대체하는 것이다(i001 §4).
* **미확정**: 정확히 어느 D3D 함수인지(레지스터 base를 디바이스 객체에서 읽으므로 정적 grep으로는 나오지 않는다). D3D8 함수 식별 task에서 특정한다.
* **미확정**: KeSetTimer로 건 타이머와 KeInsertQueueDpc로 넣은 DPC는 아직 실행되지 않는다. 이것에 기대는 코드가 있는지는 D3D·DSOUND HLE 뒤에 다시 본다.

* **Confirmed**: the entry thread creates the main thread and ends normally (eax 1). The game's main thread completes XAPI initialization (including preparing the T:, U: and Z: drives) and enters D3D8 initialization.
* **Confirmed**: opening a partition by its device name, XAPI formats the volume by writing raw sectors to it; the HLE accepts them through a volume object that drops them (files live in host directories, so no FATX structures are needed).
* **Confirmed**: code generated without seeds missed the 10 functions from `0x0018AFB0` to `0x0018C3C0`, so indirect calls to them were skipped during boot. `src/game/jsrf/recomp/seed_functions.json` makes the disassembler find them.
* **Confirmed**: the fault address is guest `0xFD001804` (NV2A MMIO, outside the 64 MiB RAM). On Win32 it wrapped by the base onto an unallocated host address and so showed up as a fault (the lucky case; design i003 §6).
* **Inferred**: this is D3D8 device creation (the `Direct3D_CreateDevice` family) initializing the GPU. The next task replaces this path with an API-level D3D8 HLE (i001 §4).
* **Unresolved**: exactly which D3D function (it reads the register base from the device object, so a static grep does not find it); pinned down by the D3D8 function-identification task.
* **Unresolved**: timers armed with KeSetTimer and DPCs queued with KeInsertQueueDpc never run yet; whether any code depends on them is revisited after the D3D and DSOUND HLE.
