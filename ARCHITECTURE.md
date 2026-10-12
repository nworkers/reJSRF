# reJSRF 아키텍처

현재 구현된 코드의 구조를 기록한다. 결정의 근거는 [설계 i001](docs/design/20261012-i001-project-bootstrap.md), [i002](docs/design/20261012-i002-recomp-pipeline.md), [i003](docs/design/20261012-i003-kernel-hle.md), [i006](docs/design/20261012-i006-xdk-symbol-binding.md), [i007](docs/design/20261012-i007-d3d8-hle-first-batch.md), [i012](docs/design/20261012-i012-d3d8-surfaces-first-frame.md), [i015](docs/design/20261012-i015-xinput-hle.md), [i017](docs/design/20261012-i017-dsound-hle.md), [i019](docs/design/20261012-i019-user-apc.md)에 있다.

## 1. 레이어와 CMake 타깃

```mermaid
flowchart BT
    PLAT["rexbox_platform<br/>virtual memory, RunGuarded, host threads,<br/>display (SDL3 + OpenGL)"]
    XBE["rexbox_xbe"]
    RT["rexbox_runtime<br/>GuestMemory, GuestHeap, HleRegistry,<br/>GuestCpu/GuestExecutor contract, Environment, HleCall"]
    REC["rexbox_recomp<br/>lifter ABI adapter, RecompExecutor"]
    HLE["rexbox_hle_kernel / xapi / dsound / xinput"]
    D3D["rexbox_hle_d3d8<br/>adapter/mode/caps, CreateDevice -> display"]
    GAME["rejsrf_game<br/>profile, lookup, decomp/"]
    GEN["rejsrf_recomp<br/>lifted C (only when gen/ exists)"]
    APP["rejsrf (exe)"]
    TOOLS["rexbox_xbe_info, rexbox_kernel_probe,<br/>rejsrf_decomp_check, rejsrf_d3d8_check (exe)"]

    RT --> XBE
    RT --> PLAT
    REC --> RT
    HLE --> RT
    D3D --> RT
    D3D -. "display contract" .-> PLAT
    GAME --> REC
    GAME <--> GEN
    GEN --> REC
    APP --> GAME
    APP --> HLE
    APP --> D3D
    TOOLS --> HLE
    TOOLS --> GAME
```

| 타깃 | 경로 | 책임 | 상태 |
|---|---|---|---|
| `rexbox_platform` | `include/rexbox/platform/`, `src/platform/<os>/`, `src/platform/sdl/` | 가상 메모리, `RunGuarded`/`RaiseStop`, `CreateHostThread`, 표시 계약(`display.h`: 창 + OpenGL 계열 컨텍스트, GL 함수 포인터, present), 입력 계약(`input.h`), 오디오 계약(`audio.h`: 48 kHz 스테레오 float 출력, pull 렌더 함수, 장치가 없으면 실시간 pacing 스레드) | Win32 구현; 표시·입력·오디오는 SDL3 백엔드(모든 OS 공유) |
| `rexbox_xbe` | `src/rexbox/xbe/` | XBE 파싱 | 구현 |
| `rexbox_runtime` | `src/rexbox/runtime/` | `GuestMemory`(based 모델, 스칼라 접근), `GuestHeap`(페이지 할당자), `HleRegistry`, `GuestCpu`·`GuestExecutor` 계약, `Environment`, `HleCall`, 커널 thunk 합성 주소 | 구현 |
| `rexbox_recomp` | `src/rexbox/recomp/` | 생성 코드 ABI 전역·콜백, `RecompExecutor`(게스트 스레드 = 호스트 스레드, `CallGuest`, 스레드별 보고), XDK override 진입점 `rexbox_hle_enter` | 구현 |
| `rexbox_hle_kernel` | `src/rexbox/hle/kernel/` | 커널 export 104개(아래 표) | 부팅이 닿은 서비스 |
| `rexbox_hle_d3d8` | `src/rexbox/hle/d3d8/` | Direct3D 8 HLE(아래 표): 어댑터·모드·Caps 질의, 디바이스·서피스·리소스 객체(Xbox 헤더 레이아웃), 상태 setter, 리소스 생성·Lock, 셰이더 핸들, 그리기 스텁 | export 110개(그리기는 로그만) |
| `rexbox_hle_xinput` | `src/rexbox/hle/xinput/` | XAPI 장치·입력 함수 11개(`XInitDevices`, `XGetDevices`, `XGetDeviceChanges`, `XInput*`, `XMountMUA`/`XUnmountMU`) on 플랫폼 입력 계약 | 게임패드 포트 0(호스트 게임패드 또는 키보드), 메모리 유닛 없음 |
| `rexbox_hle_dsound` | `src/rexbox/hle/dsound/` | 게임이 부르는 DSOUND 함수 50개(`DirectSoundCreate`, `IDirectSound_*`, `IDirectSoundBuffer_*`) on 플랫폼 오디오 계약: 객체·버퍼 메모리, Lock·Play·Stop·커서(원본 의미), PCM 8/16·Xbox ADPCM 믹서(주파수·볼륨·루프 구간·믹스 빈), 효과 이미지 서술자와 상태 읽기·쓰기 | 3D·EG·LFO·필터·DSP 효과는 `S_OK`만 |
| `rexbox_hle_xapi` | `src/rexbox/hle/xapi/` | XDK 라이브러리 HLE | 등록 진입점만 |
| `rejsrf_game` | `src/game/jsrf/` | 프로파일, 생성된 lookup 노출, 디컴파일 함수, 함수 탐색 seed(`recomp/seed_functions.json` 손으로 20개(실행에서 닿은 `observed` 1개 포함), `recomp/seed_pointer_targets.json` 포인터로만 참조되는 함수 162개: `scripts/recomp/find_pointer_targets.py`) | 디컴파일 1개 |
| `rejsrf_recomp` | `gen/jsrf_us/gen/` (gitignore) | lift된 C 9,016개 함수 | `gen/`이 있을 때만 |
| `rejsrf` | `src/app/` | 부팅 orchestration과 스레드 보고 | D3D8 디바이스 생성(창 표시)까지 |
| `rexbox_xbe_info` | `tools/xbe_info/` | XBE 정보 | 구현 |
| `rexbox_xdk_symbols` | `tools/xdk_symbols/` | XbSymbolDatabase로 XDK 함수·변수 식별 → JSON (`regenerate.py`가 사용) | 구현 |
| `rexbox_kernel_probe` | `tools/kernel_probe/` | 게임 없이 커널 export 검사(사용자 APC 포함 42개, CI) | 구현 |
| `rexbox_dsound_probe` | `tools/dsound_probe/` | 게임 없이 DSOUND export 검사(수동 출력으로 믹서를 직접 돌림; Lock, 커서, 반복 구간, 형식 디코드, 믹스 빈, 효과 서술자, pop 수; 48개, CI) | 구현 |
| `rejsrf_decomp_check` | `tools/decomp_check/` | 디컴파일 함수와 lift 본문 비교 | 구현 |
| `rejsrf_d3d8_check` | `tools/d3d8_check/` | D3D8 HLE export와 lift된 XDK 원본(`sub_X_gen`) 비교(eax, esp, 스크래치 메모리), `--dump`로 원본의 답 출력; 리소스 헤더 케이스와 디바이스 서피스 인코딩 확인 포함 | 구현(13,557 케이스) |

### 커널 HLE 파일

| 파일 | 내용 |
|---|---|
| `kernel_hle.cpp` | 등록, 초기화, thunk 슬롯 바인딩(함수 → 합성 주소, 데이터 → 변수 주소) |
| `kernel_state.cpp`, `kernel_internal.h` | 디스패처 잠금, 핸들 표, 구조체 오프셋, 공용 도우미 |
| `kernel_thread.cpp` | KPCR/ETHREAD/스택/TLS 구성, `PsCreateSystemThreadEx`, 종료, 일시정지, 우선순위, 초기 스레드 |
| `kernel_sync.cpp` | 이벤트, 세마포어, 뮤턴트, 타이머, 대기(경고 가능 사용자 모드 대기는 사용자 APC를 실행하고 `STATUS_USER_APC`) |
| `kernel_apc.cpp` | 스레드별 사용자 APC 큐(파일 I/O의 `ApcRoutine`), 전달, `NtUserIoApcDispatcher` |
| `kernel_memory.cpp` | 가상·연속·시스템 메모리, 풀 |
| `kernel_object.cpp` | `NtClose`, `Ob*` |
| `kernel_rtl.cpp` | 크리티컬 섹션, 문자열, 시간 변환, 상태 코드(공용 `NtStatusToDosError`) |
| `kernel_hal.cpp` | IRQL, 시간, 인터럽트·DPC(전달 안 함), PCI, AV, 비휘발 설정, `DbgPrint` |
| `kernel_crypto.cpp` | `XcSHA*` (SHA-1) |
| `kernel_file.cpp`, `kernel_vfs.*` | 파일·디렉터리·장치·볼륨·IOCTL, Xbox 이름공간 → 호스트 디렉터리 |
| `kernel_data.cpp` | 데이터 export, `KeTickCount` 갱신 스레드, 커널 시간 |

### D3D8 HLE 파일

| 파일 | 내용 |
|---|---|
| `d3d8_hle.cpp` | 등록, 모듈 상태 |
| `d3d8_internal.h` | Xbox D3D 상수, 구조체 오프셋(`D3DDISPLAYMODE`, `D3DPRESENT_PARAMETERS`, `D3DCAPS8`), `D3dState` |
| `d3d8_adapter.cpp` | `Direct3D_GetAdapterCount`, `D3D_GetAdapterModeCount`, `D3D_EnumAdapterModes`, `D3D_CheckDeviceFormat`, `Direct3D_CheckDeviceMultiSampleType`; 모드 표 24개와 포맷 능력 바이트 표(측정값) |
| `d3d8_caps.cpp` | `D3D_GetDeviceCaps`, `D3DDevice_GetDeviceCaps`; `D3DCAPS8` 53 DWORD 표(측정값) |
| `d3d8_device.cpp` | `Direct3D_CreateDevice`(표시 생성, 백버퍼 2개·깊이 서피스 생성), `GetBackBuffer`, `GetDepthStencilSurface`, `SetRenderTarget`, `Reset`, `SetViewport`, `Clear`, `Swap`, 디바이스 refcount, `GetRasterStatus`, 타일·감마·필터 등 |
| `d3d8_resource.h/.cpp` | Xbox `D3DResource` 헤더 레이아웃과 `Common`/`Format`/`Size` 비트, `ResourceTable`(헤더 주소 → 호스트 상태), `AddRef`/`Release`/`GetType`/`D3DSurface_GetDesc`, 디바이스 서피스 생성 |
| `d3d8_create.cpp` | `CreateTexture`/`CreateVolumeTexture`/`CreateImageSurface`/`CreateVertexBuffer`/`CreateIndexBuffer`, `D3DResource_Register`, `GetSurfaceLevel`, `Lock*`, 연속 메모리 |
| `d3d8_state.cpp` | `SetRenderState_*`·`SetTextureState_*`·`SetTransform`·스트림·인덱스·셰이더 바인딩 저장(`PipelineState`) |
| `d3d8_shader.cpp` | 정점·픽셀 셰이더 핸들과 상수 저장(GLSL 변환은 다음 묶음) |
| `d3d8_draw.cpp` | `Draw*`, `Begin`/`End`, `CopyRects`: 한 번 로그하고 통과(그리기 묶음에서 구현) |
| `d3d8_gl.h/.cpp` | 쓰는 GL 함수만 담은 함수 표, `platform::GetGlProcAddress`로 채움 |

## 2. 재컴파일 코드 생성

```mermaid
flowchart LR
    XBE["roms/.../default_eng.xbe"] --> REGEN["scripts/recomp/regenerate.py"]
    SEEDS["src/game/jsrf/recomp/*.json (seeds, hle_libraries, extra symbols)"] --> REGEN
    SYM["rexbox_xdk_symbols (XbSymbolDatabase)"] --> REGEN
    DECOMP["src/game/jsrf/decomp/*.c"] --> REGEN
    REGEN --> TOOLS["third_party/xboxrecomp tools"]
    TOOLS --> GEN["gen/jsrf_us/gen/*.c + recomp_types.h"]
    GEN --> CMAKE["CMake: rejsrf_recomp<br/>ABI header hash must match the pin"]
```

* HLE 대상 라이브러리(`hle_libraries.json`, 지금은 D3D8·D3D8LTCG)의 함수마다 `rexbox_hle_overrides.c`에 override `sub_X`가 생성된다. override는 `rexbox_hle_enter(slot)`로 같은 이름의 HLE export를 부르고, 없으면 실행을 멈춰 `stopped at unimplemented XDK function <library> <name>`을 보고한다. 원본은 `sub_X_gen`으로 남는다(`REXBOX_HLE_FALLBACK=lifted`이면 원본을 실행하고 기록하는 진단 모드).

## 3. 실행 흐름

```mermaid
sequenceDiagram
    participant App as rejsrf main
    participant K as hle::kernel
    participant X as RecompExecutor
    participant G as lifted code (guest thread)
    participant D3D as hle::d3d8
    participant P as platform display (SDL3)
    App->>App: GuestMemory, place XBE, GuestHeap, SetEnvironment
    App->>K: RegisterKernelHle, InitializeKernel(game dir, hdd), BindKernelImports
    App->>X: Attach (offset, code range)
    App->>K: StartInitialThread(image)
    K->>X: StartThread(entry, esp, KPCR)
    X->>G: host thread: RunGuarded(entry)
    G->>K: PsCreateSystemThreadEx -> K->>X: StartThread(SystemRoutine)
    G->>D3D: Direct3D_CreateDevice (after 8 queries)
    D3D->>P: CreateDisplay(640x480) -> SDL3 window + GL 3.3
    App->>X: WaitForThreads -> reports
```

## 4. 주요 계약

* **게스트 주소**: 32비트 `GuestAddress`. C++은 `GuestMemory::Translate`/`Read32`/`Write32`, 생성·디컴파일 C는 `MEM32` 등.
* **레지스터**: HLE와 런타임은 `runtime::GuestCpu`로만 접근한다. 생성 코드의 TLS 전역을 아는 것은 `rexbox_recomp`뿐이다.
* **HLE 함수**: `void()`; 안에서 `runtime::HleCall`로 stdcall 인자(`[esp+4+4i]`)·fastcall 레지스터를 읽고 `Return(value, n)`으로 eax와 esp를 맞춘다.
* **스레드**: 커널이 게스트 메모리에 KPCR/ETHREAD/스택/TLS를 만들고 `GuestExecutor::StartThread`에 맡긴다. 종료는 `ExitThread`(SEH 해제, `/EHa`).
* **동기화**: 디스패처 객체 상태는 게스트 메모리의 `SignalState`, 대기는 전역 디스패처 잠금과 condition variable.
* **표시**: HLE는 `platform/display.h`(폭·높이·제목으로 창과 OpenGL 계열 컨텍스트 생성, `GetGlProcAddress`, `PresentDisplay`)만 부른다. SDL 타입은 `src/platform/sdl/` 밖으로 나오지 않는다. 창은 `Direct3D_CreateDevice`를 부른 게스트 스레드에서 만들어진다(Win32·X11 유효; 메인 스레드가 필요한 플랫폼은 표시 서비스로 바꾼다, 설계 i007 §2.5).
* **오디오**: HLE는 `platform/audio.h`만 부른다. `StartAudioOutput`이 출력 스레드(SDL 오디오 스트림 콜백 또는 pacing 스레드)에서 렌더 함수를 부르고, DSOUND 믹서가 그때마다 게스트 메모리의 버퍼를 직접 읽어 섞고 재생 커서를 옮긴다(원본 `Unlock`이 아무것도 하지 않으므로). 게임 스레드의 HLE 호출과 렌더는 DSOUND mutex 하나를 쓴다.
* **파일**: `\Device\CdRom0` → XBE 폴더(읽기 전용), `\Device\Harddisk0\Partition1..5` → `<hdd root>/PartitionN`, `Partition0`은 원시 장치. 파티션 루트는 볼륨(원시 쓰기 허용·폐기).

## 5. 빌드 구성

* MSVC: 전역 `/utf-8 /permissive- /EHa`, `NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS`; 우리 타깃 `/W4`; 생성 코드 `/w /bigobj /MP`. 게스트 스레드의 호스트 스택은 64 MiB.
* `REJSRF_RECOMP_GEN_DIR`(기본 `gen/jsrf_us/gen`)이 없으면 `rejsrf_recomp`와 `rejsrf_decomp_check`를 만들지 않는다(CI). `rexbox_kernel_probe`는 항상 빌드되고 CI에서 실행된다.
* 라이선스 가드 `REJSRF_COPYLEFT_PATHS`는 모든 우리 타깃과 `rejsrf_recomp`에 적용된다.
* 실행 시 환경 변수: `REJSRF_HDD_ROOT`(HDD 루트, 기본 `./hdd`), `REXBOX_TRACE_FILES`(모든 파일 열기 기록), `REXBOX_HLE_FALLBACK=lifted`(미구현 XDK 함수의 원본 실행, 진단용), `REXBOX_DISPLAY=none`(창 없이 실행; 헤드리스 진단), `REXBOX_AUDIO=none`(오디오 장치를 열지 않음; 커서는 실시간으로 움직임), `REXBOX_DSOUND_TRACE=1`(버퍼 생성 형식·믹스 빈과 `Play` 기록).
* 진단 환경 변수(멈춤·손상 추적): `REXBOX_HLE_TRACE=<n>`(XDK HLE 호출을 인자·ecx·eax와 함께, 커널 import는 ordinal로 처음 n개 기록), `REXBOX_TRACE_MEMORY`(가상 메모리 할당·커밋 기록), `REXBOX_WATCHDOG=<초>`(그 시간이 지나면 게스트 스레드의 직접 호출 프로파일을 덤프하고 종료; `REXBOX_WATCHDOG_DUMP=<파일>`이면 게스트 RAM 64 MiB도 저장), `REXBOX_HEAP_CHECK=<주소>`(NT 힙의 가변 크기 자유 리스트를 HLE 진입마다 검사해 첫 손상 지점 보고). 직접 호출 프로파일은 CMake `REJSRF_RECOMP_CALL_PROFILE=ON`으로 생성 코드를 빌드했을 때만 채워진다.
* `scripts/recomp/check_hle_pops.py`는 D3D8·XInput·DSOUND HLE export의 `Return(value, n)`(템플릿 `AcceptSetting<n>`은 인자 n)과 lift 원본의 `ret n`을 대조한다(스택 불균형 예방). `scripts/recomp/find_pointer_targets.py`는 데이터·즉시값으로만 참조되어 lift되지 않은 함수 시작을 찾아 seed 파일을 쓴다.
* `third_party/SDL`(submodule, release-3.4.18, zlib)은 정적 라이브러리만 빌드되어 `rexbox_platform`에 PRIVATE로 링크된다. 카메라·GPU·렌더러·햅틱·센서·대화상자·트레이는 끄고, 오디오(DSOUND)·조이스틱·게임패드(XInput)는 켜 둔다.
* `third_party/XbSymbolDatabase`(submodule)는 `rexbox_xdk_symbols`용으로 라이브러리만 빌드된다. 생성 코드를 처음 만들 때는 도구를 먼저 빌드해야 한다.
* `third_party/xboxrecomp`는 업스트림 PR [#198](https://github.com/sp00nznet/xboxrecomp/pull/198)(`lock xadd` 플래그)과 [#199](https://github.com/sp00nznet/xboxrecomp/pull/199)(별칭 본문의 분기 고아 블록)이 머지되기 전까지 두 수정을 합친 포크 `nworkers/xboxrecomp`의 `rejsrf` 브랜치를 가리킨다(`THIRD_PARTY_NOTICES.md`).
* HLE 대상은 `src/game/jsrf/recomp/hle_libraries.json`의 `libraries`(라이브러리 전체)와 `functions`(개별 함수)로 고른다. `regenerate.py`는 식별된 XDK 변수 표 `rexbox_xdk_variables[]`도 생성한다.
* 진단(추가): `REXBOX_WATCH32=<주소,…>`(HLE 진입마다 DWORD 변화 기록), `REXBOX_PROFILE_ON_SKIP`(첫 건너뛴 간접 호출에서 호출 프로파일 덤프; 기록에 esp와 호출 지점 포함), `REXBOX_SKIP_DUMP=<파일>`/`REXBOX_STOP_DUMP=<파일>`(건너뛴 호출·비정상 정지 때 게스트 RAM 저장), CMake `REJSRF_RECOMP_ABI_CHECK`(직접 호출마다 callee의 `ebx/esi/edi` 보존과 반환 주소 pop 확인; `REXBOX_ABI_IGNORE`, `REXBOX_ABI_ESP_ONLY`로 걸러냄). 건너뛴 간접 호출 경고는 호출 지점을 함께 찍는다.

---

# reJSRF Architecture

This records the structure of the code as implemented now. The reasons are in [design i001](docs/design/20261012-i001-project-bootstrap.md), [i002](docs/design/20261012-i002-recomp-pipeline.md), [i003](docs/design/20261012-i003-kernel-hle.md), [i006](docs/design/20261012-i006-xdk-symbol-binding.md), [i007](docs/design/20261012-i007-d3d8-hle-first-batch.md), [i012](docs/design/20261012-i012-d3d8-surfaces-first-frame.md), [i015](docs/design/20261012-i015-xinput-hle.md), [i017](docs/design/20261012-i017-dsound-hle.md) and [i019](docs/design/20261012-i019-user-apc.md).

## 1. Layers and CMake targets

The diagrams and tables above apply. In short: `rexbox_platform` (virtual memory, guarded runs, host threads, and the display, input and audio contracts implemented on SDL3 in `src/platform/sdl/`) ← `rexbox_runtime` (guest memory and heap, HLE registry, the GuestCpu/GuestExecutor contract, Environment, HleCall) ← `rexbox_recomp` (lifter ABI adapter and `RecompExecutor`, one host thread per guest thread) and the HLE modules; `rejsrf_game` and the generated `rejsrf_recomp` sit above; `rejsrf` orchestrates. The kernel HLE exports 104 functions, one file per area (threads, sync, memory, objects, Rtl, HAL, crypto, files with the VFS, data exports, and user APCs: a per-thread queue fed by file I/O completion routines, run by alertable user-mode waits that then return `STATUS_USER_APC`, with `NtUserIoApcDispatcher`). The D3D8 HLE (`rexbox_hle_d3d8`) has one file per area: registration and state, constants and layouts, the adapter/mode/format queries with their measured tables, the caps table, device creation (which opens the display), and the GL function table. The DSOUND HLE (`rexbox_hle_dsound`) implements the 50 DSOUND functions the game calls on the platform audio contract: objects and buffer memory, Lock/Play/Stop and cursors with the original semantics, a PCM 8/16 and Xbox ADPCM mixer (frequency, volume, loop region, mix bins), and the effects image descriptor with state reads and writes; 3D, EG, LFO, filters and DSP effects only return `S_OK`. Function seeds: 20 by hand in `seed_functions.json` (one marked `observed`) and 162 pointer-only functions in `seed_pointer_targets.json` (from `scripts/recomp/find_pointer_targets.py`); 9,016 functions are lifted. Tools: `rexbox_xbe_info`, `rexbox_kernel_probe` (42 checks without a game, user APCs included, run in CI), `rexbox_dsound_probe` (48 DSOUND checks without a game, driving the mixer by hand, run in CI), `rejsrf_decomp_check` and `rejsrf_d3d8_check` (HLE exports against the lifted XDK originals, 13,557 cases).

## 2. Generating the recompiled code

`scripts/recomp/regenerate.py` runs the pinned xboxrecomp tools on the user's XBE, passing the committed function seeds (`src/game/jsrf/recomp/*.json`) to the disassembler and the decompiled `sub_X` definitions to the lifter; CMake checks the generated ABI header against the pin. It also runs `rexbox_xdk_symbols` (XbSymbolDatabase) and, for every function of the libraries in `hle_libraries.json` (now D3D8 and D3D8LTCG), generates an override `sub_X` in `rexbox_hle_overrides.c` that calls the HLE export of the same name through `rexbox_hle_enter(slot)`, or stops and reports `stopped at unimplemented XDK function <library> <name>`; the original stays as `sub_X_gen` (`REXBOX_HLE_FALLBACK=lifted` runs and logs it instead, a diagnostic mode).

## 3. Execution flow

The app builds guest memory, places the XBE, creates the heap and installs the Environment; registers the HLE; initializes the kernel (data exports, VFS with the game directory and the HDD root) and binds the kernel imports; attaches the executor; and starts the entry point as the first guest thread. When the game calls `Direct3D_CreateDevice`, the D3D8 HLE opens the SDL3 window with a GL 3.3 context on that guest thread. The kernel builds each thread's KPCR/ETHREAD/stack/TLS and hands it to `RecompExecutor::StartThread`; the app waits for the threads and prints their reports.

## 4. Key contracts

* **Guest address**: 32-bit `GuestAddress`; C++ uses `GuestMemory::Translate`/`Read32`/`Write32`, generated and decompiled C uses `MEM32` and friends.
* **Registers**: HLE and runtime code go through `runtime::GuestCpu` only; only `rexbox_recomp` knows the generated code's TLS globals.
* **HLE functions**: `void()`, using `runtime::HleCall` to read stdcall arguments (`[esp+4+4i]`) and fastcall registers, and `Return(value, n)` to set eax and esp.
* **Threads**: the kernel builds KPCR/ETHREAD/stack/TLS in guest memory and hands the thread to `GuestExecutor::StartThread`; `ExitThread` unwinds with SEH (`/EHa`).
* **Synchronization**: dispatcher state is the guest `SignalState`; waits use one global dispatcher lock and a condition variable.
* **Input**: `include/rexbox/platform/input.h` gives a gamepad snapshot as plain values (digital buttons, analog face buttons and triggers 0–255, sticks with +Y up), implemented on SDL3 in `src/platform/sdl/input_sdl.cpp` with a keyboard fallback for the first controller. XAPI's device types are addresses of XDK variables (`g_DeviceType_Gamepad`), which the title profile binds into `HleRegistry::BindVariable` from the generated `rexbox_xdk_variables[]` table.
* **Resources**: a D3D resource is a guest header in the Xbox layout (`Common` refcount/type bits, `Data`, `Format`/`Size` XGRAPHC bit fields, surfaces add `Parent`), because the title reads and builds headers itself; the HLE keeps host state per header address in a `ResourceTable` (docs/kb/xbox-d3d8-api.md).
* **Display**: the HLE calls only `platform/display.h` (window and OpenGL-family context from width, height and title; `GetGlProcAddress`; `PresentDisplay`). SDL types never leave `src/platform/sdl/`. The window is created on the guest thread that calls `Direct3D_CreateDevice` (valid on Win32 and X11; platforms that need the main thread get a display service, design i007 §2.5).
* **Audio**: the HLE calls only `platform/audio.h`. `StartAudioOutput` calls a render function on the output thread (SDL's audio-stream callback, or a pacing thread without a device); the DSOUND mixer then reads the buffers straight from guest memory, mixes them and moves the play cursors (the original `Unlock` does nothing). HLE calls on game threads and renders share one DSOUND mutex.
* **Files**: `\Device\CdRom0` → the XBE's folder (read-only), `\Device\Harddisk0\Partition1..5` → `<hdd root>/PartitionN`, `Partition0` a raw device; a partition root is a volume (raw writes accepted and dropped).

## 5. Build configuration

* MSVC: `/utf-8 /permissive- /EHa` and `NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS` globally; `/W4` on our targets; `/w /bigobj /MP` for generated code. Guest threads get 64 MiB host stacks.
* Without `REJSRF_RECOMP_GEN_DIR` (default `gen/jsrf_us/gen`), `rejsrf_recomp` and `rejsrf_decomp_check` are not created (CI). `rexbox_kernel_probe` is always built and runs in CI.
* The license guard `REJSRF_COPYLEFT_PATHS` applies to all our targets and to `rejsrf_recomp`.
* Run-time environment variables: `REJSRF_HDD_ROOT` (HDD root, default `./hdd`), `REXBOX_TRACE_FILES` (log every file open), `REXBOX_HLE_FALLBACK=lifted` (run the original of an unimplemented XDK function; diagnostic) , `REXBOX_DISPLAY=none` (run without a window; headless diagnostics), `REXBOX_AUDIO=none` (open no audio device; cursors still move in real time) and `REXBOX_DSOUND_TRACE=1` (log buffer formats, mix bins and `Play`). Diagnostics for hangs and corruption: `REXBOX_HLE_TRACE=<n>` (the first n XDK HLE calls with arguments, ecx and eax, and kernel imports by ordinal), `REXBOX_TRACE_MEMORY` (virtual-memory allocations and commits), `REXBOX_WATCHDOG=<seconds>` (dump the guest threads' direct-call profile and exit; with `REXBOX_WATCHDOG_DUMP=<file>` also write the 64 MiB guest RAM), `REXBOX_HEAP_CHECK=<address>` (validate an NT heap's variable-size free list at every HLE entry and report the first corruption). The direct-call profile is filled only when the generated code is built with CMake `REJSRF_RECOMP_CALL_PROFILE=ON`. `scripts/recomp/check_hle_pops.py` compares each D3D8, XInput and DSOUND export's `Return(value, n)` (for the `AcceptSetting<n>` template, its argument) with the lifted original's `ret n`; `scripts/recomp/find_pointer_targets.py` finds unlifted function starts referenced only from data or immediates and writes a seed file. More diagnostics: `REXBOX_WATCH32=<addresses>` (log DWORD changes at every HLE entry), `REXBOX_PROFILE_ON_SKIP` (dump the call profile, with esp and call site per record, at the first skipped indirect call), `REXBOX_SKIP_DUMP=<file>`/`REXBOX_STOP_DUMP=<file>` (guest RAM at a skipped call or an abnormal stop), and the CMake option `REJSRF_RECOMP_ABI_CHECK` (every direct call checks the callee kept `ebx/esi/edi` and popped its return address; filtered with `REXBOX_ABI_IGNORE` and `REXBOX_ABI_ESP_ONLY`); skipped-call warnings name the call site. HLE targets are chosen in `hle_libraries.json` by `libraries` and single `functions`, and `regenerate.py` also emits the `rexbox_xdk_variables[]` table. `third_party/xboxrecomp` points at the fork's `rejsrf` branch, which carries upstream PRs #198 and #199 until they are merged.
* `third_party/SDL` (submodule, release-3.4.18, zlib) is built as a static library only and linked PRIVATE into `rexbox_platform`; camera, GPU, renderer, haptic, sensor, dialog and tray are off, audio (DSOUND), joystick and gamepad (XInput) stay on.
* `third_party/XbSymbolDatabase` (submodule) is built, library only, for `rexbox_xdk_symbols`; the tool must be built before generating code for the first time.
