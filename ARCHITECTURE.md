# reJSRF 아키텍처

현재 구현된 코드의 구조를 기록한다. reJSRF는 타이틀 계층(JSRF 프로파일, 디컴파일 함수, 생성 코드), 앱, 타이틀 도구만 갖는다. 타이틀을 모르는 Xbox 런타임(플랫폼 계층, XBE, 런타임, 재컴파일 ABI 어댑터, HLE 모듈, 범용 도구와 probe, 재컴파일 파이프라인 스크립트)은 submodule [`third_party/rexbox`](https://github.com/reexec/rexbox)에 있고, 그 구조와 계약은 rexbox의 [ARCHITECTURE.md](third_party/rexbox/ARCHITECTURE.md)에 있다([설계 i102](docs/design/20261022-i102-rexbox-submodule.md)). 결정의 근거는 [설계 i001](docs/design/20261012-i001-project-bootstrap.md), [i002](docs/design/20261012-i002-recomp-pipeline.md), [i006](docs/design/20261012-i006-xdk-symbol-binding.md), [i108](docs/design/20261020-i108-japanese-variant.md)에 있다.

## 1. 레이어와 CMake 타깃

```mermaid
flowchart BT
    RB["third_party/rexbox<br/>rexbox_platform, rexbox_xbe, rexbox_runtime,<br/>rexbox_recomp, rexbox_hle_*, imgui"]
    GAME["rejsrf_game<br/>profile, addresses, decomp/"]
    GEN["rejsrf_recomp<br/>lifted C (only when gen/ exists)"]
    APP["rejsrf (exe)"]
    TOOLS["rejsrf_decomp_check, rejsrf_d3d8_check (exe)"]
    XR["third_party/xboxrecomp (fork, rejsrf)"]

    GAME --> RB
    GAME <--> GEN
    GEN --> RB
    XR -. "REXBOX_XBOXRECOMP_DIR (ABI header)" .-> RB
    APP --> GAME
    APP --> RB
    TOOLS --> GAME
    TOOLS --> RB
```

| 타깃 | 경로 | 책임 | 상태 |
|---|---|---|---|
| rexbox 타깃 | `third_party/rexbox/` | 플랫폼(SDL3 + OpenGL), XBE, 런타임, 재컴파일 ABI 어댑터, HLE(kernel, xapi, d3d8, dsound, xinput), `imgui`; 도구 `rexbox_xbe_info`, `rexbox_xdk_symbols`, probe 세 개(kernel, dsound, d3d8; CTest) | rexbox v0.0.9, [rexbox ARCHITECTURE](third_party/rexbox/ARCHITECTURE.md) |
| `rejsrf_game` | `src/game/jsrf/`, `include/rejsrf/` | 프로파일(XBE 판 식별, HLE 주소 바인딩, 언어), 판별 자료 주소(`jsrf_addresses.h`), 디컴파일 함수, 함수 탐색 seed(`recomp/seed_functions.json` 손으로 20개(실행에서 닿은 `observed` 1개 포함), `recomp/seed_pointer_targets.json` 포인터로만 참조되는 함수 162개: `scripts/recomp/find_pointer_targets.py`) | 디컴파일 22개(`decomp/mmatrix.c`: MMATRIX 행렬 스택 21개, 이식 가능한 C, i073), 호스트 어댑터 2개(`decomp/adxm_lock.c`, CRI ADXM 잠금 콜백과 SVM "모두 풀기" 루프, i068·i091·i100; `decomp/camera_frustum.c`, Hor+ 와이드에서 절두체 컬링의 가로 시야, i097; 호스트 훅은 `include/rejsrf/jsrf_host.h`) |
| `rejsrf_recomp` | `gen/<변형>/gen/`(`jsrf_us` 또는 `jsrf_jp`, gitignore) | lift된 C 9,526개 함수. rexbox의 `rexbox_add_recompiled_library`로 만든다 | `gen/`이 있을 때만 |
| `rejsrf` | `src/app/` | 게임 디렉터리의 `default.xbe`(또는 `--xbe <파일>`)를 읽어 내용으로 식별, 부팅 orchestration과 스레드 보고, 설정 오버레이 화면과 `rejsrf.ini` | 로고 시퀀스와 3D 어트랙트 데모 |
| `rejsrf_decomp_check` | `tools/decomp_check/` | 디컴파일 함수와 lift 본문 비교(범용 레지스터, XMM0~7, 게스트 메모리) | 구현(129 케이스) |
| `rejsrf_d3d8_check` | `tools/d3d8_check/` | D3D8 HLE export와 lift된 XDK 원본(`sub_X_gen`) 비교(eax, esp, 스크래치 메모리), `--dump`로 원본의 답 출력; 리소스 헤더 케이스, 디바이스 서피스 인코딩, 상태 setter가 남긴 라이브러리 전역 비교 포함 | 구현(14,018 케이스) |

## 2. 재컴파일 코드 생성

```mermaid
flowchart LR
    XBE["roms/.../default.xbe"] --> WRAP["scripts/recomp/regenerate.py<br/>(build by MD5, fingerprints)"]
    WRAP --> REGEN["rexbox scripts/recomp/regenerate.py"]
    SEEDS["src/game/jsrf/recomp/*.json (seeds, hle_libraries, extra symbols, variants)"] --> REGEN
    SYM["rexbox_xdk_symbols (XbSymbolDatabase)"] --> REGEN
    DECOMP["src/game/jsrf/decomp/*.c"] --> REGEN
    REGEN --> TOOLS["third_party/xboxrecomp tools (fork)"]
    TOOLS --> GEN["gen/(variant)/gen/*.c + recomp_types.h"]
    GEN --> CMAKE["CMake: rejsrf_recomp<br/>ABI header hash must match the pin"]
```

* `scripts/recomp/regenerate.py`는 rexbox 파이프라인의 wrapper다. XBE의 MD5로 판을 정하고(`jsrf_variant.py`, `variants.json`), 미국판이 아니면 함수 지문을 확인한 뒤(`variant_fingerprints.py`, `function_fingerprints.json`), rexbox `regenerate.run(...)`을 reJSRF 경로와 fork xboxrecomp로 부른다.
* HLE 대상 라이브러리(`hle_libraries.json`, 지금은 D3D8·D3D8LTCG)의 함수마다 `rexbox_hle_overrides.c`에 override `sub_X`가 생성된다. override는 `rexbox_hle_enter(slot)`로 같은 이름의 HLE export를 부르고, 없으면 실행을 멈춰 `stopped at unimplemented XDK function <library> <name>`을 보고한다. 원본은 `sub_X_gen`으로 남는다(`REXBOX_HLE_FALLBACK=lifted`이면 원본을 실행하고 기록하는 진단 모드).
* `scripts/recomp/check_hle_pops.py`(D3D8·XInput·DSOUND export의 pop 수와 lift 원본의 `ret n` 대조)와 `find_pointer_targets.py`(데이터·즉시값으로만 참조되는 함수 시작 → seed 파일)도 rexbox 스크립트에 reJSRF 경로를 넘기는 wrapper다.

## 3. 실행 흐름

```mermaid
sequenceDiagram
    participant App as rejsrf main
    participant K as hle::kernel
    participant X as RecompExecutor
    participant G as lifted code (guest thread)
    participant D3D as hle::d3d8
    participant P as platform display (SDL3)
    App->>App: SetApplicationIdentity, SetDisplayTitle, load default.xbe (or --xbe), identify the build, GuestMemory, place XBE, ApplyLanguage (title profile), GuestHeap, SetEnvironment
    App->>K: RegisterKernelHle, InitializeKernel(game dir, hdd), BindKernelImports
    App->>X: Attach (offset, code range)
    App->>K: StartInitialThread(image)
    K->>X: StartThread(entry, esp, KPCR)
    X->>G: host thread: RunGuarded(entry)
    G->>K: PsCreateSystemThreadEx -> K->>X: StartThread(SystemRoutine)
    G->>D3D: Direct3D_CreateDevice (after 8 queries)
    D3D->>P: CreateDisplay(640x480, scale 2) -> SDL3 window + GL 3.3
    App->>X: WaitForThreads -> reports
```

## 4. 주요 계약

* **rexbox 소비자 계약**([rexbox ARCHITECTURE §2](third_party/rexbox/ARCHITECTURE.md)). 앱은 자기만 아는 것을 다음 계약으로 알려준다.
  * `platform::SetApplicationIdentity("reJSRF", "reJSRF")`: 사용자 설정 폴더
  * `platform::SetDisplayTitle`: `reJSRF v<버전> (<플랫폼>/<아키텍처> <구성>, <US|JP>) - Build <일시>`. 창이 GL 버전과 FPS를 붙인다
  * `platform::SetOverlayBuilder`: 설정 메뉴 화면
  * `hle::d3d8::SetDisplayDefaults`: `rejsrf.ini`의 표시 기본값
  * `platform::SetRumbleStrength`
* 게스트 주소, 레지스터, HLE 함수, 스레드, 동기화, 표시, 오디오, 입력의 계약은 rexbox의 것이다(rexbox ARCHITECTURE §4).
* **타이틀 프로파일**(`include/rejsrf/jsrf_profile.h`): 지원하는 XBE 판을 인증서로 식별하고(`BuiltVariant()`), XAPI 장치 형식 같은 XDK 변수를 생성된 `rexbox_xdk_variables[]`에서 `HleRegistry::BindVariable`로 묶고, 장치가 원본 기본 상태 루틴(`D3D_InitializeDeviceState`)을 부를 수 있게 그 주소를 묶는다.
* **타이틀 설정**: 게임 안의 값으로 정하는 설정(지금은 언어)은 프로파일이 주소와 값의 뜻을 갖고, 앱이 XBE를 올린 뒤·진입 전에 `ApplyLanguage`로 게스트 메모리에 쓴다(US판 `0x1FC724`, 일본판 `0x251F58`; 0 = 일본어, 1 = 영어). `rexbox`는 관여하지 않는다. 설정은 `rejsrf.ini`의 `[Game] Language`와 설정 메뉴의 Game 탭이며 다음 실행부터 적용된다([design i104](docs/design/20261020-i104-language-option.md)).
* **한국어**([design i113](docs/design/20261023-i113-korean-language.md)): `Language::kKorean`은 인덱스 값이 아니다. 게임 인덱스는 1(영어)이고, 앱이 디스크(D:)를 JSRF-kr-patch가 만든 한국어 폴더로 바꾸고 그 XBE를 올린다.
  * 프로파일의 `ClassifyXbeText`가 섹션 바이트의 64비트 FNV-1a 해시를 US판 값과 비교한다. 결과는 원본, 한국어 패치(코드 같음, `.rdata`·`.data` 다름), 코드가 다름(거부) 셋 중 하나다. lift된 코드는 원본 코드 섹션에서 나왔으므로 데이터만 바뀐 XBE만 실행할 수 있다.
  * 앱은 캐시 파티션(`Partition3`~`5`)을 채운 문장 세트를 `<hdd root>/rejsrf_cache_text.txt`에 적고, 세트가 바뀌면 커널을 초기화하기 전에 그 파티션을 비운다. rexbox는 바꾸지 않았다.
* **파일**: `\Device\CdRom0` → 게임 디렉터리(읽기 전용), `\Device\Harddisk0\Partition1..5` → `<hdd root>/PartitionN`(`REJSRF_HDD_ROOT`, 기본 `./hdd`).

## 5. 빌드 구성

* 루트 `CMakeLists.txt`
  * `REXBOX_XBOXRECOMP_DIR`를 reJSRF의 `third_party/xboxrecomp`로, `REXBOX_BUILD_TOOLS=ON`으로 정한다.
  * `add_subdirectory(third_party/rexbox)`와 `rexbox_apply_host_flags()`를 부른다(MSVC: `/utf-8 /permissive- /EHa`, `NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS`).
  * 우리 타깃에는 `rexbox_strict_warnings`(`/W4`)와 라이선스 가드 `rexbox_check_license`를 건다. 생성 코드는 `/w /bigobj /MP`로 빌드한다.
* `rejsrf`는 `/LARGEADDRESSAWARE`로 링크하고 이미지 기본 스택은 1 MiB로 둔다. 기본값이 크면 크기를 지정하지 않은 모든 스레드(오디오, SDL, GL 드라이버)가 그만큼 예약해 32비트 주소 공간이 바닥난다(issue #25). 게스트 스레드의 64 MiB 스택은 rexbox의 `CreateHostThread`가 명시한다. 원본을 메인 스레드에서 실행하는 `rejsrf_d3d8_check`만 `/STACK:64MB`를 쓴다. `rejsrf.map`은 `REXBOX_SAMPLE` 결과를 함수 이름으로 묶을 때 쓴다(rexbox `scripts/perf/resolve_samples.py`).
* **빌드 변형**: `REJSRF_VARIANT=jsrf_us|jsrf_jp`(기본 `jsrf_us`)가 실행 파일이 돌릴 디스크를 정한다. 실행 파일 하나는 한 판만 돌린다. 두 판은 함수 주소가 같아 seed와 디컴파일 함수 이름을 공유하고(재생성 때 `function_fingerprints.json`으로 확인), 자료 주소만 `include/rejsrf/jsrf_addresses.h`에서 `REJSRF_VARIANT_JP`로 갈린다. 프로파일의 `BuiltVariant()`와 `NativeLanguage()`가 그 판을 알려 준다. 판은 파일 이름이 아니라 내용(앱은 인증서, 스크립트는 MD5)으로 식별한다([design i108](docs/design/20261020-i108-japanese-variant.md)).
* `REJSRF_RECOMP_GEN_DIR`(기본 `gen/<REJSRF_VARIANT>/gen`)이 없으면 `rejsrf_recomp`, `rejsrf_decomp_check`, `rejsrf_d3d8_check`를 만들지 않는다(CI). rexbox probe는 항상 빌드되고 CI에서 실행된다. 생성 코드 진단 옵션은 rexbox의 `REXBOX_RECOMP_CALL_PROFILE`(직접 호출 프로파일; `REXBOX_WATCHDOG`가 덤프)과 `REXBOX_RECOMP_ABI_CHECK`다.
* 실행 시 환경 변수: 런타임의 것은 모두 `REXBOX_` 접두사이고 rexbox ARCHITECTURE §5에 있다. reJSRF 앱의 것은 `REJSRF_HDD_ROOT`(HDD 루트)와 `REJSRF_LANGUAGE`(그 실행의 언어)다.
* `third_party/rexbox`(submodule, reexec/rexbox v0.0.9 `b2f2653`, MIT)는 자기 submodule로 SDL3 release-3.4.18(zlib, 정적), Dear ImGui v1.93.0(MIT), SMAA(MIT), XbSymbolDatabase(MIT, 도구에만)를 가져온다. 그 안의 xboxrecomp(upstream v0.15.0)는 reJSRF가 쓰지 않는다.
* `third_party/xboxrecomp`는 포크 `nworkers/xboxrecomp`의 `rejsrf` 브랜치(`5aae5df`)를 가리킨다(`THIRD_PARTY_NOTICES.md`). 이 브랜치는 upstream v0.15.0(`6376eb8`)에 다음을 합친 것이다.
  * i093의 디스어셈블러 수정(표 항목 복구, 바이트 인덱스 표; upstream PR [#236](https://github.com/sp00nznet/xboxrecomp/pull/236))
  * i044의 성능 개선(upstream PR을 보류한 `perf/mem-base-local`, 우리 fork에만 두는 `perf/xmm-intrinsics`)
  * C4310 수정 `fix/fp-cc-cast`
  * 미해결 스텁 보고: 생성된 스텁 파일의 `RECOMP_STUB_HIT`가 `RECOMP_STUB_REPORT`일 때 `recomp_unresolved_stub_log(va)`를 먼저 부른다(fork 전용, rexbox가 정의를 건다)

  이전 핀(v0.13.1)은 `rejsrf-v0.13`에 남아 있다. upstream v0.15.0으로 옮기는 것은 나중 일이다([설계 i093](docs/design/20261020-i093-xboxrecomp-v0.15.md), [i102](docs/design/20261022-i102-rexbox-submodule.md)).
* HLE 대상은 `src/game/jsrf/recomp/hle_libraries.json`의 `libraries`(라이브러리 전체)와 `functions`(개별 함수)로 고른다. 재생성은 식별된 XDK 변수 표 `rexbox_xdk_variables[]`도 생성한다.

---

# reJSRF Architecture

This records the structure of the code as implemented now. reJSRF holds only the title layer (the JSRF profile, decompiled functions, generated code), the app and the title tools. The title-agnostic Xbox runtime (platform layer, XBE, runtime, lifter ABI adapter, HLE modules, generic tools and probes, the recompilation pipeline scripts) is the submodule [`third_party/rexbox`](https://github.com/reexec/rexbox), whose structure and contracts are in rexbox's [ARCHITECTURE.md](third_party/rexbox/ARCHITECTURE.md) ([design i102](docs/design/20261022-i102-rexbox-submodule.md)). The reasons are in [design i001](docs/design/20261012-i001-project-bootstrap.md), [i002](docs/design/20261012-i002-recomp-pipeline.md), [i006](docs/design/20261012-i006-xdk-symbol-binding.md) and [i108](docs/design/20261020-i108-japanese-variant.md).

## 1. Layers and CMake targets

The diagram and table above apply.

* **rexbox** (v0.0.9) supplies:
  * `rexbox_platform` (SDL3 + OpenGL), `rexbox_xbe`, `rexbox_runtime`, `rexbox_recomp`, the HLE modules and `imgui`
  * the tools `rexbox_xbe_info` and `rexbox_xdk_symbols`, and three probes (kernel, DSOUND, D3D8) registered with CTest
* **`rejsrf_game`** (`src/game/jsrf/`, `include/rejsrf/`) holds:
  * the profile: build identification by certificate, HLE address bindings, language
  * the per-build data addresses (`jsrf_addresses.h`)
  * the decompiled functions: `decomp/mmatrix.c` rewrites 21 MMATRIX matrix-stack functions in portable C ([design i073](docs/design/20261016-i073-mmatrix.md)). Two host adapters sit beside them:
    * `adxm_lock.c` wraps the CRI ADXM lock callbacks in rexbox's guest lock and keys the SVM "release all" loop on per-thread depth ([i068](docs/design/20261016-i068-adxm-lock.md), [i091](docs/design/20261018-i091-adxm-lock-priority.md), [i100](docs/design/20261019-i100-svm-unlock-all.md)).
    * `camera_frustum.c` widens the culling field of view for Hor+ through `include/rejsrf/jsrf_host.h` ([i097](docs/design/20261018-i097-frustum-culling.md)).
  * the function seeds: 20 by hand in `seed_functions.json`, one marked `observed`, and 162 pointer-only functions in `seed_pointer_targets.json`
* **`rejsrf_recomp`** is the generated code (9,526 functions), built with rexbox's `rexbox_add_recompiled_library`.
* **`rejsrf`** orchestrates the boot and owns the settings screens and `rejsrf.ini`.
* **`rejsrf_decomp_check`** (129 cases) and **`rejsrf_d3d8_check`** (14,018 cases) compare the decompiled functions and the HLE exports with their lifted originals.

## 2. Generating the recompiled code

`scripts/recomp/regenerate.py` wraps rexbox's pipeline. It picks the build from the XBE's MD5 (`jsrf_variant.py`, `variants.json`); for a build other than the US one it checks the function fingerprints (`variant_fingerprints.py`, `function_fingerprints.json`); then it calls rexbox's `regenerate.run(...)` with reJSRF's paths and the fork xboxrecomp. For every function of the libraries in `hle_libraries.json` (now D3D8 and D3D8LTCG), an override `sub_X` in `rexbox_hle_overrides.c` calls the HLE export of the same name through `rexbox_hle_enter(slot)`, or stops and reports `stopped at unimplemented XDK function <library> <name>`; the original stays as `sub_X_gen` (`REXBOX_HLE_FALLBACK=lifted` runs and logs it instead). `check_hle_pops.py` (export pop counts against the originals' `ret n`) and `find_pointer_targets.py` (functions referenced only from data or immediates → the seed file) are wrappers over rexbox's scripts too.

## 3. Execution flow

The app gives rexbox its identity and window title, loads and identifies the XBE, builds guest memory, places the XBE, sets the game's language through the title profile, creates the heap and installs the Environment; registers the HLE; initializes the kernel (data exports, VFS with the game directory and the HDD root) and binds the kernel imports; attaches the executor; and starts the entry point as the first guest thread. When the game calls `Direct3D_CreateDevice`, the D3D8 HLE opens the SDL3 window with a GL 3.3 context on that guest thread; the app waits for the threads and prints their reports.

## 4. Key contracts

* **rexbox's consumer contract** ([rexbox ARCHITECTURE §2](third_party/rexbox/ARCHITECTURE.md)). The app tells rexbox what only it knows:
  * `platform::SetApplicationIdentity("reJSRF", "reJSRF")`: the user settings folder
  * `platform::SetDisplayTitle`: `reJSRF v<version> (<platform>/<arch> <config>, <US|JP>) - Build <time>`; the window adds the GL version and the FPS
  * `platform::SetOverlayBuilder`: the settings menu's screens
  * `hle::d3d8::SetDisplayDefaults`: the display defaults from `rejsrf.ini`
  * `platform::SetRumbleStrength`
* Guest addresses, registers, HLE functions, threads, synchronization, display, audio and input follow rexbox's contracts (rexbox ARCHITECTURE §4).
* **Title profile** (`include/rejsrf/jsrf_profile.h`): it identifies the supported builds by certificate (`BuiltVariant()`), binds XDK variables such as the XAPI device types from the generated `rexbox_xdk_variables[]` into `HleRegistry::BindVariable`, and binds the library's default-state routine (`D3D_InitializeDeviceState`) for device creation.
* **Title settings**: settings made by a value inside the game (today the language) belong to the profile. The app writes the value with `ApplyLanguage` after placing the XBE and before the entry (the US build's `0x1FC724`, the Japanese build's `0x251F58`; 0 = Japanese, 1 = English). rexbox takes no part. The setting is `[Game] Language` in `rejsrf.ini` and the Game tab, applied from the next start ([design i104](docs/design/20261020-i104-language-option.md)).
* **Korean** ([design i113](docs/design/20261023-i113-korean-language.md)): `Language::kKorean` is not an index value. The game index is 1 (English); the app swaps the disc (D:) for the Korean folder JSRF-kr-patch made and loads its XBE.
  * The profile's `ClassifyXbeText` compares 64-bit FNV-1a hashes of the section bytes with the US build's. The result is original, Korean patch (same code, `.rdata` and `.data` changed) or changed code (refused). The lifted code came from the original code sections, so only an XBE with changed data can run.
  * The app records in `<hdd root>/rejsrf_cache_text.txt` which text set filled the cache partitions (`Partition3`-`5`), and empties them before the kernel starts when the set changes. rexbox is unchanged.
* **Files**: `\Device\CdRom0` → the game directory (read-only), `\Device\Harddisk0\Partition1..5` → `<hdd root>/PartitionN` (`REJSRF_HDD_ROOT`, default `./hdd`).

## 5. Build configuration

* **Root `CMakeLists.txt`**
  * It sets `REXBOX_XBOXRECOMP_DIR` to reJSRF's `third_party/xboxrecomp` and `REXBOX_BUILD_TOOLS=ON`.
  * It calls `add_subdirectory(third_party/rexbox)` and `rexbox_apply_host_flags()` (MSVC: `/utf-8 /permissive- /EHa`, `NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS`).
  * Our targets get `rexbox_strict_warnings` (`/W4`) and the license guard `rexbox_check_license`. Generated code builds with `/w /bigobj /MP`.
* **Linking**
  * `rejsrf` links with `/LARGEADDRESSAWARE` and keeps the image default stack at 1 MiB (issue #25); guest threads get their 64 MiB stacks from rexbox's `CreateHostThread`.
  * Only `rejsrf_d3d8_check` uses `/STACK:64MB`.
  * `rejsrf.map` names the functions in a `REXBOX_SAMPLE` report (rexbox's `scripts/perf/resolve_samples.py`).
* **Build variant**: `REJSRF_VARIANT=jsrf_us|jsrf_jp` (default `jsrf_us`) picks the disc the executable runs; one executable runs one build.
  * The two builds have the same function addresses, so they share the seeds and decompiled function names (checked against `function_fingerprints.json` at regeneration).
  * Only data addresses differ, through `REJSRF_VARIANT_JP` in `include/rejsrf/jsrf_addresses.h`.
  * A build is identified by content (certificate in the app, MD5 in the scripts), never by file name ([design i108](docs/design/20261020-i108-japanese-variant.md)).
* **No generated code**: without `REJSRF_RECOMP_GEN_DIR` (default `gen/<REJSRF_VARIANT>/gen`), `rejsrf_recomp` and the title tools are not created (CI). The rexbox probes are always built and run in CI. The generated-code diagnostics are rexbox's options `REXBOX_RECOMP_CALL_PROFILE` and `REXBOX_RECOMP_ABI_CHECK`.
* **Environment variables**: the runtime's all carry the `REXBOX_` prefix and are listed in rexbox ARCHITECTURE §5. The app's own are `REJSRF_HDD_ROOT` and `REJSRF_LANGUAGE`.
* **`third_party/rexbox`** (submodule, reexec/rexbox v0.0.9 `b2f2653`, MIT) brings its own submodules: SDL3 release-3.4.18 (zlib, static), Dear ImGui v1.93.0 (MIT), SMAA (MIT) and XbSymbolDatabase (MIT, tools only). Its own xboxrecomp (upstream v0.15.0) is not used by reJSRF.
* **`third_party/xboxrecomp`** points at the fork's `rejsrf` branch (`5aae5df`): upstream v0.15.0 plus
  * i093's disassembler fixes (upstream PR [#236](https://github.com/sp00nznet/xboxrecomp/pull/236))
  * i044's performance branches
  * the C4310 fix
  * the unresolved-stub report under `RECOMP_STUB_REPORT`, which rexbox defines

  The previous pin stays on `rejsrf-v0.13`. Moving to upstream v0.15.0 is later work ([design i093](docs/design/20261020-i093-xboxrecomp-v0.15.md), [i102](docs/design/20261022-i102-rexbox-submodule.md)).
* **HLE targets** are chosen in `hle_libraries.json` by `libraries` and single `functions`, and regeneration also emits the `rexbox_xdk_variables[]` table.
