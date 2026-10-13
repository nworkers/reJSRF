# 외부 참조 프로젝트와 라이선스

2026-10-12에 확인한 상태다. "사용 범위"는 AGENTS.md의 라이선스 규칙을 적용한 결과다.

| 프로젝트 | 내용 | 라이선스 | 사용 범위 |
|---|---|---|---|
| [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) | 원본 Xbox 정적 재컴파일 툴킷: Python 파이프라인(XBE 파서, 디스어셈블, 함수 식별, ABI 분석, lifter), 커널·D3D8·DSOUND·입력 런타임 | MIT, 단 `src/apu/*`, `src/nv2a/nv2a_core.c`, `nv2a_regs.h`, `nv2a_state.h`는 xemu 유래 LGPL-2.1+ | submodule(`third_party/xboxrecomp`). v0.13.1 `193e2995` + 수정을 담은 포크 `nworkers/xboxrecomp` `rejsrf`(`541b554`); 수정은 업스트림 PR [#198](https://github.com/sp00nznet/xboxrecomp/pull/198)(lifter `lock xadd`), [#199](https://github.com/sp00nznet/xboxrecomp/pull/199)(디스어셈블러 별칭 분기), [#200](https://github.com/sp00nznet/xboxrecomp/pull/200)(다른 함수 본문 중간으로 가는 분기)로 기여했고, i044의 성능 개선 중 `perf/mem-base-local`은 업스트림 PR 보류, `perf/xmm-intrinsics`는 업스트림 설계(칸 단위 C)와 달라 우리 fork에만 둔다. 도구와 MIT 소스를 파일 단위로 사용. LGPL 파일은 빌드 금지(CMake 가드). 정점 셰이더 번역(i037)은 `src/kernel/nv2a_vsh_interp.c`(MIT)의 명령 필드 표를 지식으로 참고했다(코드 미사용). xemu 생성기를 따랐다고 밝힌 `src/kernel/nv2a_combiner.c`는 참고하지 않았다 |
| [andeecollard/jsrf-recomp](https://github.com/andeecollard/jsrf-recomp) | xboxrecomp 기반 JSRF(US) macOS 포트. 챕터 2 게임플레이, 60 fps(D3D8 API lift). JSRF 고유 분석 문서(`docs/jsrf/`, `experiments/*_boundary`) | MIT(상위와 같은 LGPL 예외 포함) | 분석 사실과 MIT 코드 참고. 주장은 우리 바이너리에서 다시 확인할 때까지 **추정** |
| [dplewis/jsrf](https://github.com/dplewis/jsrf) | JSRF(US) 재구현 패치 방식 디컴파일. US판 MD5 명시 | 명시 없음 | 사실(해시, 주소)만 출처를 달아 참고. 코드는 가져오지 않음 |
| [KeybadeBlox/JSRF-Decompilation](https://codeberg.org/KeybadeBlox/JSRF-Decompilation) (Codeberg) | JSRF 북미판 매칭 디컴파일(Ghidra, Visual C++ 7.0, delink → 재링크) | 확인되지 않음. LLM 생성 기여를 금지함 | 코드를 가져오지 않음. 공개 문서의 사실만 출처를 달아 참고 |
| [silveriviere/Project-Graffiti-Soul](https://github.com/silveriviere/Project-Graffiti-Soul) | JSRF 디컴파일 프로젝트(초기 단계) | 명시 없음 | 사실만 참고 |
| [Cxbx-Reloaded/XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) | XDK 빌드별 함수 시그니처(OOVPA)로 XBE 안의 XDK 함수·변수를 식별 | MIT | submodule(`third_party/XbSymbolDatabase`, `20eced5`). 도구 `rexbox_xdk_symbols`가 링크 |
| [xemu-project/xemu](https://github.com/xemu-project/xemu) | 원본 Xbox LLE 에뮬레이터. 동작 비교 기준(오라클)으로 유용 | GPL-2.0 / LGPL 혼합 | 실행 결과 비교와 동작 이해만. 코드 사용 금지 |
| [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded) | HLE 방식 Xbox 에뮬레이터. XDK 버전별 HLE 데이터베이스 | GPL-2.0 | 동작 이해만. 코드 사용 금지 |
| [XboxDev/nxdk](https://github.com/XboxDev/nxdk) | 오픈소스 Xbox SDK. 커널 헤더와 타입 | 구성 요소별로 다름(확인 필요) | 도입 전 파일별 라이선스 확인 |
| [xboxdevwiki](https://xboxdevwiki.net/) | XBE, 커널, 하드웨어 문서 | 위키 문서 | 지식 출처 |
| [N64Recomp](https://github.com/N64Recomp/N64Recomp), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) | 다른 플랫폼의 정적 재컴파일 사례 | MIT | 설계 참고 |
| [capstone](https://github.com/capstone-engine/capstone) | 디스어셈블러(xboxrecomp 파이프라인이 사용) | BSD-3-Clause | 파이프라인 도구 의존성 |
| [SDL3](https://github.com/libsdl-org/SDL) | 창·입력·오디오 크로스플랫폼 계층 | zlib | submodule(`third_party/SDL`, release-3.4.18). 표시 계약의 백엔드(`src/platform/sdl/`), 정적 링크 |
| [reexec/re2DJ](https://github.com/reexec/re2DJ) | EZ2DJ(Win32 x86) HLE 런타임. DirectDraw/Direct3D 3 → OpenGL(SDL3 공용 백엔드), DirectSound → SDL3 + SDL_mixer, SDL3 게임패드. rePIU와 같은 작업 규칙 | BSD-3-Clause | 필요할 때 참고하고 코드도 가져올 수 있다(고지 필요). 가져온 코드에서 문제를 찾으면 우리 쪽만 고치지 말고 re2DJ에 업스트림 기여한다(사용자 결정, 2026-10-12). 호스트 계층(SDL3 창·GL·오디오·입력)과 DSOUND 버퍼 모델이 공유 후보. D3D3 고정 기능 HLE는 Xbox D3D8(셰이더·푸시버퍼)과 API 세대가 달라 직접 재사용 범위가 좁다. DSOUND(i017)에서는 코드를 가져오지 않았다: re2DJ는 `Play` 때 버퍼를 복사해 SDL_mixer 트랙에 넣는데, Xbox `Unlock`은 아무것도 하지 않고 게임이 재생 중인 버퍼에 바로 써서 게스트 메모리를 직접 읽는 믹서가 필요하다 |

`scripts/fetch_references.ps1`은 참고용 저장소를 `third_party/refs/`(gitignore)에 받아 둔다. 그 안의 코드는 빌드에 쓰지 않는다.

---

# External reference projects and licenses

State as checked on 2026-10-12. "Use" applies the license rules of AGENTS.md.

The table above lists each project, what it is, its license, and how we may use it. In short:

* **xboxrecomp** (MIT, with xemu-derived LGPL-2.1+ files under `src/apu/` and `src/nv2a/`): a submodule at `third_party/xboxrecomp`, pinned to the fork `nworkers/xboxrecomp` branch `rejsrf` (`541b554`: v0.13.1 `193e2995` plus fixes contributed upstream, [#198](https://github.com/sp00nznet/xboxrecomp/pull/198) for the lifter's `lock xadd` flags, [#199](https://github.com/sp00nznet/xboxrecomp/pull/199) for the disassembler's alias branches and [#200](https://github.com/sp00nznet/xboxrecomp/pull/200) for branches into another function's interior, and i044's performance branches: `perf/mem-base-local` with its upstream PR held, and `perf/xmm-intrinsics`, which stays on our fork only because upstream deliberately writes the XMM helpers as lane-wise C). Its tools and MIT sources are used file by file; a CMake guard forbids building the LGPL files. For the vertex-shader translation (i037), the instruction field table in `src/kernel/nv2a_vsh_interp.c` (MIT) was consulted as knowledge (no code); `src/kernel/nv2a_combiner.c`, which says it follows xemu's generator, was not consulted.
* **jsrf-recomp** (MIT with the same LGPL exception): a JSRF (US) macOS port on xboxrecomp that reached chapter-2 gameplay at 60 fps through a D3D8 API lift. Its analysis facts and MIT code are references; its claims stay **inferred** until re-checked on our binary.
* **dplewis/jsrf**, **Project-Graffiti-Soul** (no stated license) and **Codeberg JSRF-Decompilation** (license not confirmed; forbids LLM-generated contributions): no code is taken; facts from their public documents may be cited.
* **xemu** (GPL/LGPL) and **Cxbx-Reloaded** (GPL-2.0): behavior and comparison only; no code.
* **nxdk**: licenses differ per component; check per file before adopting anything.
* **XbSymbolDatabase** (MIT) identifies XDK functions in an XBE; a submodule at `third_party/XbSymbolDatabase` (`20eced5`) linked into the `rexbox_xdk_symbols` tool.
* **capstone** (BSD-3-Clause) is a pipeline dependency; **SDL3** (zlib) is a submodule at `third_party/SDL` (release-3.4.18), the statically linked backend of the display contract (`src/platform/sdl/`).
* **re2DJ** (BSD-3-Clause): an HLE runtime for the Win32 x86 EZ2DJ games (DirectDraw/Direct3D 3 → OpenGL on a shared SDL3 backend, DirectSound → SDL3 + SDL_mixer, SDL3 gamepads), under the same working rules as rePIU. Consult it when useful and take code from it (with notice); when a problem is found in code taken from it, contribute the fix upstream to re2DJ rather than fixing only our copy (user decision, 2026-10-12). Its host layer (SDL3 window, GL context, audio, input) and DirectSound buffer model are the sharing candidates; its fixed-function Direct3D 3 HLE is a different API generation from Xbox D3D8 (shaders, push buffers), so direct reuse there is narrow. For DSOUND (i017) no code was taken: re2DJ copies a buffer into an SDL_mixer track at `Play`, while the Xbox `Unlock` does nothing and titles write into playing buffers, which needs a mixer that reads guest memory directly.

`scripts/fetch_references.ps1` clones the reference repositories into `third_party/refs/` (gitignored). Nothing there is built.
