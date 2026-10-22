# 작업 로그: 공용 런타임 rexbox를 submodule로 쓰기 (issue #102)

설계: [20261022-i102](../design/20261022-i102-rexbox-submodule.md) · 작업 지시: [20261022-i102](../work-orders/20261022-i102-rexbox-submodule.md)

## 한 일

1. 기준 측정: v0.0.46 Release 빌드(미국판, 일본판)로 데모의 파일 열기 목록과 프레임 300~1800 BMP를 저장했다. 같은 실행 파일을 두 번 돌려 보니 프레임은 바이트 단위로 같았다. 파일 열기 목록은 45초 안에 진행한 길이만 달랐으므로, 비교는 짧은 쪽 길이까지만 한다.
2. submodule
   * `third_party/rexbox`(reexec/rexbox v0.0.9, `b2f2653`)를 더했다.
   * reJSRF 자체의 SDL, imgui, smaa, XbSymbolDatabase submodule을 지웠다. rexbox의 submodule이 같은 커밋을 가리킨다.
   * `third_party/xboxrecomp`(fork `rejsrf`, `5aae5df`)는 그대로 두었다.
3. CMake
   * 루트: `REXBOX_XBOXRECOMP_DIR`(reJSRF의 fork), `REXBOX_BUILD_TOOLS=ON`, `add_subdirectory(third_party/rexbox)`, `rexbox_apply_host_flags()`
   * `REJSRF_VARIANT`와 생성 디렉터리 처리는 그대로 두었다.
   * `src/game/jsrf`: `rexbox_add_recompiled_library(rejsrf_recomp …)`. `include/`는 `rejsrf_game`의 공개 include 경로로 직접 지정했다(전에는 in-tree rexbox 타깃이 노출했다).
   * `tools/`: 타이틀 도구 두 개만 남겼다.
   * 우리 타깃에 `rexbox_check_license`, `rexbox_strict_warnings`
4. 지운 것
   * `src/rexbox/`, `src/platform/`, `include/rexbox/`, `cmake/embed_text.cmake`
   * rexbox 도구 다섯 개
   * `scripts/perf/`, 원래의 `scripts/recomp/check_hle_pops.py`
5. 스크립트
   * `regenerate.py`는 wrapper다. MD5로 판을 정하고, 미국판이 아니면 지문을 확인한 뒤, rexbox `run(...)`을 fork xboxrecomp와 새 `rexbox_xdk_symbols` 경로(`build/win32_x86/third_party/rexbox/tools/…`)로 부른다.
   * `find_pointer_targets.py`, `check_hle_pops.py`도 wrapper가 되었다.
   * reJSRF의 `xbe_variant.py`는 `jsrf_variant.py`로 이름을 바꿨다. rexbox의 `regenerate.py`가 자기 `xbe_variant`(시그니처가 다름)를 import하므로 같은 이름이면 충돌한다.
6. 앱: `SetApplicationIdentity("reJSRF", "reJSRF")`. 전의 `paths_sdl.cpp`가 고정해 둔 이름과 같다.
7. CI: checkout을 `submodules: recursive`로 바꾸고, probe 경로를 `build\win32_x86\third_party\rexbox\tools\<구성>\`로 고쳤다.
8. 문서
   * `ARCHITECTURE.md`: 런타임 내부 설명을 rexbox ARCHITECTURE로 넘기고, 타이틀 계층·계약·빌드만 남겼다.
   * `AGENTS.md`: 레이어 규칙에 "런타임 수정은 rexbox 저장소에서", 플랫폼 경로, 라이선스 가드를 반영했다.
   * `CODING_STYLE.md`(디렉터리 정책), 가이드(submodule, 도구 경로, probe 수), `THIRD_PARTY_NOTICES.md`(rexbox 항목, 서드파티 경로), `README.md`
   * 분석·kb 문서의 소스 경로

## 검증

| 검증 | 결과 |
|---|---|
| 재생성(미국판, 일본판) | wrapper로 다시 만든 lift 코드(`gen/*/gen/recomp_*.c`)가 v0.0.46 생성분과 같다. 다른 것은 다음 셋이다. ① override 파일 첫 줄 주석(생성기 이름). ② lifter 스캔 입력 `overrides.c`(컴파일하지 않음)에 디컴파일 소스의 `#include` 한 줄. ③ 일본판 `summary.json`의 XBE 경로. 일본판 지문 확인 통과 |
| 미국판 Release 빌드 | 컴파일러 경고 0(생성 코드 대상의 명령줄 D9025는 기존과 같다) |
| 일본판 Release 빌드 | 같음 |
| rexbox probe(두 트리) | kernel 94, dsound 55, d3d8 220 검사, 실패 0. 분리 전 56/48/163보다 많은 것은 rexbox v0.0.2~v0.0.9가 더한 검사다 |
| `rejsrf_decomp_check`(두 트리) | 129 케이스, 차이 0 |
| `rejsrf_d3d8_check` | 미국판 14,018 케이스, 일본판 14,018 케이스, 차이 0. 일본판 도구에 미국판 XBE를 주면 거부 |
| 미국판 데모 | 파일 열기 2,315개(기준 길이)와 프레임 300~1800이 v0.0.46과 바이트 단위로 같다 |
| 일본판 데모 | 파일 열기 2,348개와 프레임 300~1800이 같다 |
| 미국판 긴 데모 | 프레임 2100~3600(300 간격)도 같다 |
| stderr 차이 | ① `[kernel] KeSetTimer armed; timers do not fire yet`가 사라졌다. rexbox #12로 타이머가 실제로 울린다. 원본 동작에 가까워졌고, 이 구간 화면은 같다. ② 프로파일 안내의 CMake 옵션 이름이 `REXBOX_RECOMP_CALL_PROFILE`로 바뀌었다 |
| 설계 §2의 우려 | 안개·정점 블렌딩(rexbox #7), Swap 즉시 반환(#18)으로 달라진 프레임은 3600까지 없었다 |
| 사용자 설정 폴더 | `SDL_GetPrefPath("reJSRF", "reJSRF")`로 전과 같다(코드로 확인) |
| CI | 브랜치에서 수동 실행(`workflow_dispatch`, run 39713016590): Debug·Release 모두 성공. Release 작업의 일본판 앱 빌드·스모크 단계 포함 |
| 다른 작업 폴더에서 재확인 | `git submodule update --init --recursive` 뒤 미국판 Release를 처음부터 빌드: 경고 0(D9025 5건은 기존과 같음). probe 94/55/220 실패 0, `rejsrf_decomp_check` 129 차이 0, `rejsrf_d3d8_check` 14,018 차이 0. 데모 프레임 300~1800이 v0.0.46 빌드와 바이트 단위로 같다 |

## 남은 것

* xboxrecomp를 upstream v0.15.0(rexbox 기본)으로 옮기는 일(별도 작업).
* 3600프레임 뒤의 장면(Press Start 이후)은 사용자 확인.

---

# Work log: use the shared runtime rexbox as a submodule (issue #102)

Design: [20261022-i102](../design/20261022-i102-rexbox-submodule.md) · Work order: [20261022-i102](../work-orders/20261022-i102-rexbox-submodule.md)

## Done

1. Baseline: with the v0.0.46 Release builds (US and Japanese), the demos' file-open lists and frames 300-1800 were saved. Two runs of the same executable gave byte-identical frames. The file-open lists differed only in how far each run got in 45 s, so they are compared up to the shorter one.
2. Submodules
   * Added `third_party/rexbox` (reexec/rexbox v0.0.9, `b2f2653`).
   * Removed reJSRF's own SDL, imgui, smaa and XbSymbolDatabase submodules; rexbox's point at the same commits.
   * Kept `third_party/xboxrecomp` (fork `rejsrf`, `5aae5df`).
3. CMake
   * Root: `REXBOX_XBOXRECOMP_DIR` (reJSRF's fork), `REXBOX_BUILD_TOOLS=ON`, `add_subdirectory(third_party/rexbox)`, `rexbox_apply_host_flags()`.
   * The `REJSRF_VARIANT` and generated-directory handling is unchanged.
   * `src/game/jsrf`: `rexbox_add_recompiled_library(rejsrf_recomp …)`. `rejsrf_game` now names `include/` as its own public include path; the in-tree rexbox targets used to export it.
   * `tools/`: the two title tools only.
   * `rexbox_check_license` and `rexbox_strict_warnings` on our targets.
4. Deleted
   * `src/rexbox/`, `src/platform/`, `include/rexbox/`, `cmake/embed_text.cmake`
   * the five rexbox tools
   * `scripts/perf/` and the original `scripts/recomp/check_hle_pops.py`
5. Scripts
   * `regenerate.py` is a wrapper: it picks the build by MD5, checks the fingerprints for builds other than the US one, then calls rexbox's `run(...)` with the fork xboxrecomp and the new `rexbox_xdk_symbols` paths (`build/win32_x86/third_party/rexbox/tools/…`).
   * `find_pointer_targets.py` and `check_hle_pops.py` are wrappers too.
   * reJSRF's `xbe_variant.py` became `jsrf_variant.py`: rexbox's `regenerate.py` imports its own `xbe_variant`, which has a different signature, so the same name would clash.
6. App: `SetApplicationIdentity("reJSRF", "reJSRF")`, the name the old `paths_sdl.cpp` hard-coded.
7. CI: checkout with `submodules: recursive`; probe paths under `build\win32_x86\third_party\rexbox\tools\<config>\`.
8. Documents
   * `ARCHITECTURE.md`: runtime internals handed to rexbox's ARCHITECTURE; the title layer, contracts and build remain.
   * `AGENTS.md`: layer rules ("runtime changes are made in the rexbox repository"), platform paths, license guard.
   * `CODING_STYLE.md` (directory policy), the guide (submodules, tool paths, probe counts), `THIRD_PARTY_NOTICES.md` (a rexbox entry, third-party paths), `README.md`.
   * Source paths in the analysis and kb documents.

## Verification

| Check | Result |
|---|---|
| Regeneration (US, Japanese) | The lifted code from the wrapper (`gen/*/gen/recomp_*.c`) equals v0.0.46's. Three things differ: ① the first comment line of the override files (the generator's name); ② one decompiled source's `#include` line in the lifter's scan input `overrides.c`, which is never compiled; ③ the XBE path in the Japanese `summary.json`. The Japanese fingerprint check passed |
| US Release build | zero compiler warnings (the generated-code target's command-line D9025 as before) |
| Japanese Release build | the same |
| rexbox probes (both trees) | kernel 94, dsound 55, d3d8 220 checks, 0 failures; more than the 56/48/163 before the split because rexbox v0.0.2-v0.0.9 added checks |
| `rejsrf_decomp_check` (both trees) | 129 cases, 0 differences |
| `rejsrf_d3d8_check` | US 14,018 cases, Japanese 14,018 cases, 0 differences; the Japanese tool refuses the US XBE |
| US demo | 2,315 file opens (the baseline's length) and frames 300-1800 byte-identical to v0.0.46 |
| Japanese demo | 2,348 file opens and frames 300-1800 identical |
| Longer US demo | frames 2100-3600 (every 300) identical too |
| stderr differences | ① `[kernel] KeSetTimer armed; timers do not fire yet` is gone: timers now fire (rexbox #12), which is closer to the original, and the pictures in this stretch are unchanged. ② The profile hint names the CMake option `REXBOX_RECOMP_CALL_PROFILE` |
| The concern of design §2 | fog and vertex blending (rexbox #7) and Swap returning at once (#18) changed no frame up to 3600 |
| User settings folder | `SDL_GetPrefPath("reJSRF", "reJSRF")`, as before (checked in code) |
| CI | run by hand on the branch (`workflow_dispatch`, run 39713016590): Debug and Release both succeeded, including the Release job's Japanese app build and smoke step |
| Rechecked in another working folder | after `git submodule update --init --recursive`, a clean US Release build: zero warnings (the five D9025 as before); probes 94/55/220 with 0 failures, `rejsrf_decomp_check` 129 with 0 differences, `rejsrf_d3d8_check` 14,018 with 0 differences; demo frames 300-1800 equal the v0.0.46 build byte for byte |

## Open

* Moving xboxrecomp to upstream v0.15.0 (rexbox's default) is separate work.
* Scenes after frame 3600 (past Press Start) are for the user to check.
