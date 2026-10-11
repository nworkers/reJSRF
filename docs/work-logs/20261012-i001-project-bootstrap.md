# Issue #1 작업 로그 — 프로젝트 부트스트랩

작업 지시: [20261012-i001](../work-orders/20261012-i001-project-bootstrap.md) · 설계: [20261012-i001](../design/20261012-i001-project-bootstrap.md)

## 요약

reJSRF 저장소를 만들고 rePIU의 작업·코딩 규칙을 이 프로젝트에 맞게 옮겼다. 레이어는 `platform` → `rexbox`(재사용 Xbox 런타임과 HLE) → `game/jsrf` → `app`이며, CMake 타깃 그래프가 의존 방향을 강제한다. Win32 x86에서 Debug/Release 모두 경고 없이 빌드된다. 앱은 사용자 XBE를 식별하고 64 MiB guest memory에 배치한 뒤 멈춘다. XBE 분석 결과 디스크의 `default_eng.xbe`가 US 소매판(MD5가 dplewis/jsrf의 기재값과 일치)이라 1차 타깃으로 정했다.

## 과정

1. rePIU(`AGENTS.md`, `docs/CODING_STYLE.md`, 문서·Git 규칙)를 읽고 다음을 바꿨다: 목표(DOS 직접 실행 → XBE 재컴파일·디컴파일), 라이선스(BSD-3 → MIT), 실행 모델(direct/cache → 모든 호스트에서 based 주소 모델), 문서명(`EXE_DESIGN` → `XBE_DESIGN`). 레이어 의존, 게임 데이터·생성 코드, copyleft 규칙을 새로 넣었다.
2. 외부 조사: xboxrecomp(MIT, Burnout 3 기준 툴킷)와 jsrf-recomp(MIT, JSRF US를 챕터 2와 60 fps까지 실행)가 핵심 참조다. xboxrecomp의 `src/apu/*`, `src/nv2a/nv2a_core.c`, `nv2a_regs.h`, `nv2a_state.h`는 xemu 유래 LGPL이고 umbrella 헤더가 `nv2a_regs.h`를 include하므로, 통째로 링크하지 않고 MIT 파일만 골라 쓰기로 했다. Codeberg 디컴파일 프로젝트는 라이선스가 확인되지 않았고 LLM 기여를 금지하므로 코드를 참조하지 않는다.
3. 사용자 확인: "xboxrecomp를 사용하는 건 문제없는가", "링크만 안 하면 되는가" → LGPL 파일을 컴파일·링크·복사하지 않으면 문제없다고 답했다. 이를 루트 CMake 가드 `REJSRF_COPYLEFT_PATHS`로 강제했다.
4. xboxrecomp를 `third_party/xboxrecomp` submodule로 v0.13.1(`193e2995`)에 고정하고, `xbe_parser.py`의 커널 export 표(371개)를 C++ 표로 옮겼다.
5. HLE 레지스트리를 처음에는 커널 ordinal 바인딩까지 맡게 썼는데, 그러면 런타임이 HLE 커널 모듈에 의존해 순환이 생긴다. ordinal 바인딩을 `hle::kernel::BindKernelImports`로 옮겨 런타임이 HLE를 모르게 했다.
6. `rexbox_xbe_info`로 두 XBE를 읽었다. 두 빌드는 title id, XDK 4134, 진입점, 섹션 배치, 커널 import 120개가 같지만 146,461바이트가 다르고 `.text`도 다르다. US판을 1차 타깃으로, JP판은 식별만 하도록 프로파일에 넣었다.
7. 빌드 스크립트 문제: Windows PowerShell 5.1의 `ConvertFrom-Json`이 배열을 객체 하나로 넘겨 `Sort-Object`가 실패했다. 변수에 받은 뒤 정렬하도록 고쳤다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug 클린 빌드(VS 2026, `/W4 /permissive- /utf-8`) | 성공, 경고 0 |
| Win32 x86 Release 클린 빌드 | 성공, 경고 0 |
| `rexbox_xbe_info` × 2 XBE | 헤더, 인증서, 섹션 11개, 라이브러리 7개, 커널 import 120개 출력 |
| `rejsrf default_eng.xbe` / `default.xbe` | `USA (region 1)` / `Japan (region 2)` 식별, 64 MiB에 섹션 11개 배치, 종료 코드 0 |
| `rejsrf LICENSE` / 인자 없음 | `not an XBE image` 종료 코드 1 / 사용법, 종료 코드 2 |
| LGPL 가드 | `apu_core.c`를 넣은 임시 타깃으로 configure 실패 확인 후 되돌림 |
| 코드 검사 | 35개 파일: 탭, 비ASCII(한국어 주석), CRLF 없음. `rexbox` 아래 타이틀 이름은 규칙을 설명하는 주석 하나뿐 |
| `git status` | `roms/`와 빌드 산출물이 staging되지 않음 |

CI(`.github/workflows/ci.yml`)는 아직 GitHub에서 실행되지 않았다. 첫 push 또는 PR에서 확인한다.

## 남은 일

* 다음 task: 재컴파일 파이프라인 연결(설계 §11의 1번). xboxrecomp 파이프라인으로 US XBE를 `gen/`에 lift하고, 생성 코드 ABI와 `HleRegistry`를 잇는 dispatcher를 설계한다. 직접 호출에 HLE·디컴파일 우선순위를 적용하는 방법도 이때 정한다.
* 이 디스크가 어느 발매판인지 미확정(`docs/analysis/jsrf-xbe-overview.md` §1).

---

# Issue #1 work log — project bootstrap

Work order: [20261012-i001](../work-orders/20261012-i001-project-bootstrap.md) · Design: [20261012-i001](../design/20261012-i001-project-bootstrap.md)

## Summary

Created the reJSRF repository and carried rePIU's working and coding rules over to this project. The layers are `platform` → `rexbox` (the reusable Xbox runtime and HLE) → `game/jsrf` → `app`, with the CMake target graph enforcing the direction. Win32 x86 builds in Debug and Release with no warnings. The app identifies the user's XBE, places it in 64 MiB of guest memory and stops. The XBE analysis showed the disc's `default_eng.xbe` is the US retail build (its MD5 matches the one dplewis/jsrf lists), so it is the primary target.

## Process

1. Read rePIU (`AGENTS.md`, `docs/CODING_STYLE.md`, documentation and Git rules) and changed: the goal (direct DOS execution → XBE recompilation and decompilation), the license (BSD-3 → MIT), the execution model (direct/cache → the based address model on every host) and a document name (`EXE_DESIGN` → `XBE_DESIGN`). Added rules for layer dependencies, game data and generated code, and copyleft.
2. External survey: xboxrecomp (MIT, a toolkit built against Burnout 3) and jsrf-recomp (MIT, runs JSRF US into chapter 2 at 60 fps) are the key references. xboxrecomp's `src/apu/*`, `src/nv2a/nv2a_core.c`, `nv2a_regs.h` and `nv2a_state.h` are xemu-derived LGPL, and its umbrella header includes `nv2a_regs.h`, so it is not linked wholesale; only MIT files are taken. The Codeberg decompilation has no confirmed license and forbids LLM contributions, so its code is not referenced.
3. User questions: "is using xboxrecomp fine?" and "is it enough not to link it?" → yes, as long as the LGPL files are not compiled, linked or copied. The root CMake guard `REJSRF_COPYLEFT_PATHS` enforces this.
4. Pinned xboxrecomp as the `third_party/xboxrecomp` submodule at v0.13.1 (`193e2995`) and ported the kernel export table (371 entries) of `xbe_parser.py` to a C++ table.
5. The HLE registry first also bound kernel ordinals, which made the runtime depend on the HLE kernel module, a cycle. Ordinal binding moved to `hle::kernel::BindKernelImports`, so the runtime knows nothing of HLE.
6. Read both XBEs with `rexbox_xbe_info`. They share the title id, XDK 4134, entry point, section layout and 120 kernel imports, but differ in 146,461 bytes, `.text` included. The profile takes the US build as the primary target and only recognizes the JP build.
7. Build script issue: Windows PowerShell 5.1's `ConvertFrom-Json` passes an array as one object, so `Sort-Object` failed; the result is now collected first and then sorted.

## Verification

The table in the Korean section applies: clean Debug and Release builds with zero warnings; `rexbox_xbe_info` on both XBEs; the app identifies both builds, places 11 sections in 64 MiB and exits 0, rejects a non-XBE file with exit code 1 and prints usage with exit code 2 when given no argument; the LGPL guard failed configure for a temporary target naming `apu_core.c` (reverted); 35 code files contain no tabs, non-ASCII (Korean comments) or CRLF, and the only title name under `rexbox` is the comment stating the rule; `git status` shows `roms/` and build output unstaged. CI (`.github/workflows/ci.yml`) has not run on GitHub yet; the first push or PR will show it.

## Remaining

* Next task: connect the recompilation pipeline (design §11, item 1). Lift the US XBE into `gen/` with the xboxrecomp pipeline and design the dispatcher joining the generated-code ABI to `HleRegistry`, including how direct calls get the HLE and decompiled priority.
* Which release this disc is remains unresolved (`docs/analysis/jsrf-xbe-overview.md` §1).
