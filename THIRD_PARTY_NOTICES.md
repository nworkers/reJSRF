# 서드파티 고지 / Third-Party Notices

reJSRF는 MIT 라이선스다. 아래 서드파티 코드가 저장소에 포함되거나 옮겨져 있다.

reJSRF is MIT licensed. The third-party code below is included in, or ported into, the repository.

## rexbox

* 출처 / Source: https://github.com/reexec/rexbox
* 라이선스 / License: MIT (Copyright (c) 2026 rexbox contributors (reJSRF contributors)). 전문은 `third_party/rexbox/LICENSE`. / Full text in `third_party/rexbox/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/rexbox`, v0.0.11 (`d532817`). reJSRF에서 이력째 분리된 타이틀 비의존 Xbox 런타임(플랫폼 계층, XBE, 런타임, 재컴파일 ABI 어댑터, HLE, 도구, lifter를 모르는 재컴파일 보조 스크립트)이다. 아래 SDL3, Dear ImGui, SMAA, XbSymbolDatabase는 rexbox의 submodule(`third_party/rexbox/third_party/`)로 들어온다. rexbox 자체의 서드파티 고지는 `third_party/rexbox/THIRD_PARTY_NOTICES.md`에 있다. / The title-agnostic Xbox runtime split out of reJSRF with its history (platform layer, XBE, runtime, lifter ABI adapter, HLE, tools, lifter-agnostic recompilation helper scripts). SDL3, Dear ImGui, SMAA and XbSymbolDatabase below come in as rexbox's submodules (`third_party/rexbox/third_party/`); rexbox's own notices are in `third_party/rexbox/THIRD_PARTY_NOTICES.md`.

## xboxrecomp

* 출처 / Source: https://github.com/sp00nznet/xboxrecomp
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz). 전문은 `third_party/xboxrecomp/LICENSE`. / Full text in `third_party/xboxrecomp/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/xboxrecomp`. v0.16.0 (`f0f8a46`) 위에 fork 전용 수정을 합친 포크 `nworkers/xboxrecomp` 브랜치 `rejsrf`(`6acdc0c`)를 가리킨다. i093의 디스어셈블러 수정 세 커밋은 upstream PR [sp00nznet/xboxrecomp#236](https://github.com/sp00nznet/xboxrecomp/pull/236)으로 v0.16.0에 들어갔다. fork에만 남은 것은 성능 개선 두 개(`44cb039` 함수마다 메모리 기준, fork 브랜치 `perf/mem-base-local`, upstream PR 보류; `56b73d9`·`716547c` XMM 도우미 SSE2 intrinsic, `perf/xmm-intrinsics`, upstream의 칸 단위 C 설계와 달라 우리 fork에만 둠)와 C4310 수정 `16a4a0d`(`fix/fp-cc-cast`)다. 미해결 스텁 보고 `5aae5df`는 되돌렸다(i115, upstream의 `RECOMP_STUB_LOG`를 쓴다). 이전 핀은 fork 브랜치 `rejsrf-v0.15`(`5aae5df`, v0.15.0 기반)와 `rejsrf-v0.13`(`1991221`, v0.13.1 기반)에 남아 있다. v0.13 때의 PR #198, #199는 머지됐고, #200은 v0.14.0이 대신 해결해 닫았다. / Points at the fork `nworkers/xboxrecomp`, branch `rejsrf` (`6acdc0c`): v0.16.0 (`f0f8a46`) plus fork-only changes. i093's three disassembler commits went into v0.16.0 as upstream PR [sp00nznet/xboxrecomp#236](https://github.com/sp00nznet/xboxrecomp/pull/236). What stays fork-only is two performance changes (`44cb039` reads the memory base once per function, fork branch `perf/mem-base-local`, upstream PR held; `56b73d9` and `716547c` SSE2 intrinsics for the XMM helpers, `perf/xmm-intrinsics`, fork only because upstream deliberately writes them as lane-wise C) and the C4310 fix `16a4a0d` (`fix/fp-cc-cast`). The stub report `5aae5df` was reverted (i115; upstream's `RECOMP_STUB_LOG` is used). Earlier pins stay on the fork branches `rejsrf-v0.15` (`5aae5df`, on v0.15.0) and `rejsrf-v0.13` (`1991221`, on v0.13.1); PRs #198 and #199 from v0.13 days are merged, and #200 was closed because v0.14.0 covers it.
* 옮긴 코드 / Ported code: rexbox의 `src/hle/kernel/kernel_exports.cpp`(`KERNEL_EXPORTS` 표, rexbox 고지 참고) / rexbox's `src/hle/kernel/kernel_exports.cpp` (the `KERNEL_EXPORTS` table; see rexbox's notices).
* 빌드에 쓰는 파일 / Files used by the build (MIT, submodule에서 직접 / straight from the submodule):
  * `templates/runtime/recomp_types.h` — 생성 코드 ABI 헤더. lifter가 생성 코드 옆(`gen/<변형>/gen/recomp_types.h`)에 쓰고, 생성 코드와 rexbox의 `recomp_abi.c`(`rejsrf_recomp_abi`)가 그 사본을 include한다 / the generated-code ABI header; the lifter writes it next to the generated code (`gen/<variant>/gen/recomp_types.h`), and the generated code and rexbox's `recomp_abi.c` (`rejsrf_recomp_abi`) include that copy.
  * `tools/` — 재컴파일 파이프라인(Python), reJSRF의 `scripts/recomp/lifter.py`가 실행 / the recompilation pipeline (Python), run by reJSRF's `scripts/recomp/lifter.py`.
* 제외 / Excluded: xemu에서 가져온 LGPL-2.1-or-later 파일(`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`)과 그것을 include하는 `include/xbox/xboxrecomp.h`는 컴파일·링크하지 않는다. 이 파일들을 어떤 소스 목록에도 넣지 않는다는 AGENTS.md의 라이선스 규칙으로 지킨다(CMake 가드 없음, [설계 i117](docs/design/20261024-i117-lifter-pipeline.md)). / The LGPL-2.1-or-later files taken from xemu (`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`) and `include/xbox/xboxrecomp.h`, which includes them, are never compiled or linked; AGENTS.md's license rule, which keeps them out of every source list, holds this (no CMake guard, [design i117](docs/design/20261024-i117-lifter-pipeline.md)).

```text
MIT License

Copyright (c) 2026 sp00nz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## XbSymbolDatabase

* 출처 / Source: https://github.com/Cxbx-Reloaded/XbSymbolDatabase
* 라이선스 / License: MIT (Copyright (c) 2023 The XbSymbolDatabase authors). 전문은 `third_party/rexbox/third_party/XbSymbolDatabase/LICENSE`. / Full text in `third_party/rexbox/third_party/XbSymbolDatabase/LICENSE`.
* 포함 방식 / How included: rexbox의 git submodule / rexbox's git submodule `third_party/XbSymbolDatabase` (`20eced544726f5558c5a408458f38a086cc4e543`). 라이브러리 `libXbSymbolDatabase`만 빌드해 도구 `rexbox_xdk_symbols`에 링크한다. 게임 실행 파일에는 들어가지 않는다. / Only the `libXbSymbolDatabase` library is built, linked into the `rexbox_xdk_symbols` tool; it does not enter the game executable.

## SDL3

* 출처 / Source: https://github.com/libsdl-org/SDL
* 라이선스 / License: zlib (Copyright (C) 1997-2026 Sam Lantinga). 전문은 `third_party/rexbox/third_party/SDL/LICENSE.txt`. / Full text in `third_party/rexbox/third_party/SDL/LICENSE.txt`.
* 포함 방식 / How included: rexbox의 git submodule / rexbox's git submodule `third_party/SDL`, `release-3.4.18`. 정적 라이브러리 `SDL3-static`만 빌드해 `rexbox_platform`에 링크한다(표시·입력·오디오 계약의 백엔드 `src/platform/sdl/`). 카메라, GPU, 2D 렌더러, 햅틱, 센서, 대화상자, 트레이 하위 시스템과 테스트·예제·설치는 끈다. / Only the static library `SDL3-static` is built and linked into `rexbox_platform` (the backend of the display, input and audio contracts, `src/platform/sdl/`); the camera, GPU, 2D renderer, haptic, sensor, dialog and tray subsystems, tests, examples and installation are off.

```text
This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## Dear ImGui

* 출처 / Source: https://github.com/ocornut/imgui
* 라이선스 / License: MIT (Copyright (c) 2014-2026 Omar Cornut). 전문은 `third_party/rexbox/third_party/imgui/LICENSE.txt`. / Full text in `third_party/rexbox/third_party/imgui/LICENSE.txt`.
* 포함 방식 / How included: rexbox의 git submodule / rexbox's git submodule `third_party/imgui`, `v1.93.0` (`c1aaac65e`). 코어(`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`)는 정적 라이브러리 `imgui`로 빌드해 앱(설정 메뉴)과 `rexbox_platform`에 링크한다. SDL3·OpenGL3 백엔드(`backends/imgui_impl_sdl3.cpp`, `backends/imgui_impl_opengl3.cpp`)는 `rexbox_platform` 안에서 컴파일한다. / The core (`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`) is built as the static library `imgui`, linked into the app (the settings menu) and `rexbox_platform`; the SDL3 and OpenGL3 backends (`backends/imgui_impl_sdl3.cpp`, `backends/imgui_impl_opengl3.cpp`) compile inside `rexbox_platform`.

```text
The MIT License (MIT)

Copyright (c) 2014-2026 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## SMAA

* 출처 / Source: https://github.com/iryoku/smaa
* 라이선스 / License: MIT (Copyright (C) 2013 Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro, Diego Gutierrez). 전문은 `third_party/rexbox/third_party/smaa/LICENSE.txt`. / Full text in `third_party/rexbox/third_party/smaa/LICENSE.txt`.
* 포함 방식 / How included: rexbox의 git submodule / rexbox's git submodule `third_party/smaa` (`71c806a`). `SMAA.hlsl`은 빌드 때 바이트 배열로 `rexbox_hle`에 들어가 GLSL 3 셰이더로 컴파일되고, `Textures/AreaTex.h`, `Textures/SearchTex.h`는 `d3d8_smaa.cpp`가 그대로 include한다. / `SMAA.hlsl` is embedded in `rexbox_hle` as bytes at build time and compiled as GLSL 3 shaders; `d3d8_smaa.cpp` includes `Textures/AreaTex.h` and `Textures/SearchTex.h` as they are.

```text
Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)

Permission is hereby granted, free of charge, to any person obtaining a copy
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. As clarification, there is no
requirement that the copyright notice and permission be included in binary
distributions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## jsrf-recomp (data)

* 출처 / Source: https://github.com/andeecollard/jsrf-recomp, `experiments/d3d8_boundary/inferred_names.json`
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz; jsrf-recomp is a fork of xboxrecomp).
* 사용 / Use: `src/game/jsrf/recomp/xdk_symbols_extra.json`의 D3D8 함수 이름과 주소 5개 / five D3D8 function names and addresses in `src/game/jsrf/recomp/xdk_symbols_extra.json`.
