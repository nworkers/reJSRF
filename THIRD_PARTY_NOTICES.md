# 서드파티 고지 / Third-Party Notices

reJSRF는 MIT 라이선스다. 아래 서드파티 코드가 저장소에 포함되거나 옮겨져 있다.

reJSRF is MIT licensed. The third-party code below is included in, or ported into, the repository.

## xboxrecomp

* 출처 / Source: https://github.com/sp00nznet/xboxrecomp
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz). 전문은 `third_party/xboxrecomp/LICENSE`. / Full text in `third_party/xboxrecomp/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/xboxrecomp`. v0.13.1 (`193e2995ffaec871bb3be43a43bd66aa5fccfeb1`) 위에 수정을 합친 포크 `nworkers/xboxrecomp` 브랜치 `rejsrf`(`1991221`)를 가리킨다. lifter 수정 두 개(`e108d0e9` `lock xadd` 플래그, `01064f3b` `FCMOVcc`; PR [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198)), 디스어셈블러 수정 두 개(`2649f5f7`, PR [#199](https://github.com/sp00nznet/xboxrecomp/pull/199); `65648d41`, PR [#200](https://github.com/sp00nznet/xboxrecomp/pull/200)), 성능 개선 두 개다. `602c44c` 함수마다 메모리 기준(fork 브랜치 `perf/mem-base-local`, upstream PR 보류)과, XMM 도우미 SSE2 intrinsic(`perf/xmm-intrinsics`, upstream의 칸 단위 C 설계와 달라 우리 fork에만 두고 upstream에 내지 않음)이다. 그리고 lifter 수정 `dbae155`(MXCSR 모델링과 `CVTSS2SI` 반올림, i048)가 있다. 이 수정은 PR [#198](https://github.com/sp00nznet/xboxrecomp/pull/198)에 `c50c597`로 함께 올렸다. i055의 디스어셈블러 수정 `58f7166`(늦게 생긴 본문의 tail jump)은 [PR #200](https://github.com/sp00nznet/xboxrecomp/pull/200)에 합쳤다. i058의 디스어셈블러 수정 `0c32a78`(늦은 본문의 `jcc`)과 lifter 수정 `982cd28`·`7c58ec6`(복구된 switch case 안의 중첩 switch)도 같은 PR에 덧붙였다. 생성기 변경 `6a38c01`(`feat/stub-report`, 미해결 스텁의 실행 보고)은 upstream PR을 준비만 하고 보류했다. 모두 머지되면 submodule을 업스트림으로 되돌린다. / Points at the fork `nworkers/xboxrecomp`, branch `rejsrf` (`1991221`): v0.13.1 (`193e2995ffaec871bb3be43a43bd66aa5fccfeb1`) plus two lifter fixes (`e108d0e9` `lock xadd` flags and `01064f3b` `FCMOVcc`, [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198)) two disassembler fixes (`2649f5f7`, [#199](https://github.com/sp00nznet/xboxrecomp/pull/199); `65648d41`, [#200](https://github.com/sp00nznet/xboxrecomp/pull/200)), and two performance changes: `602c44c` reads the memory base once per function (fork branch `perf/mem-base-local`, upstream PR held), and SSE2 intrinsics for the XMM helpers (`perf/xmm-intrinsics`) stay on our fork only and are not proposed upstream, because upstream deliberately writes them as lane-wise C; and the lifter fix `dbae155` (MXCSR modelled, `CVTSS2SI` rounding, i048), also submitted in PR [#198](https://github.com/sp00nznet/xboxrecomp/pull/198) as `c50c597`; i055's disassembler fix `58f7166` (tail jumps from late bodies), added to [PR #200](https://github.com/sp00nznet/xboxrecomp/pull/200), together with i058's disassembler fix `0c32a78` (`jcc` from late bodies) and lifter fixes `982cd28` and `7c58ec6` (nested switches inside recovered arms); and i055's generator change `6a38c01` (`feat/stub-report`, unresolved stubs report when run), whose upstream PR is prepared but held; the submodule returns to upstream once all are merged.
* 옮긴 코드 / Ported code:
  * `src/rexbox/hle/kernel/kernel_exports.cpp` — `tools/xbe_parser/xbe_parser.py`의 `KERNEL_EXPORTS` 표 / the `KERNEL_EXPORTS` table of `tools/xbe_parser/xbe_parser.py`.
* 빌드에 쓰는 파일 / Files used by the build (MIT, submodule에서 직접 / straight from the submodule):
  * `templates/runtime/recomp_types.h` — 생성 코드 ABI 헤더, `rexbox_recomp`와 생성 코드가 include / the generated-code ABI header, included by `rexbox_recomp` and the generated code.
  * `src/kernel/recomp_icall_feedback.h` — 생성 코드의 include 경로 / on the generated code's include path.
  * `tools/` — 재컴파일 파이프라인(Python), `scripts/recomp/regenerate.py`가 실행 / the recompilation pipeline (Python), run by `scripts/recomp/regenerate.py`.
* 제외 / Excluded: xemu에서 가져온 LGPL-2.1-or-later 파일(`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`)과 그것을 include하는 `include/xbox/xboxrecomp.h`는 컴파일·링크하지 않는다. 루트 `CMakeLists.txt`의 `REJSRF_COPYLEFT_PATHS`가 이를 강제한다. / The LGPL-2.1-or-later files taken from xemu (`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`) and `include/xbox/xboxrecomp.h`, which includes them, are never compiled or linked; `REJSRF_COPYLEFT_PATHS` in the root `CMakeLists.txt` enforces this.

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
* 라이선스 / License: MIT (Copyright (c) 2023 The XbSymbolDatabase authors). 전문은 `third_party/XbSymbolDatabase/LICENSE`. / Full text in `third_party/XbSymbolDatabase/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/XbSymbolDatabase` (`20eced544726f5558c5a408458f38a086cc4e543`). 라이브러리 `libXbSymbolDatabase`만 빌드해 도구 `rexbox_xdk_symbols`에 링크한다. 게임 실행 파일에는 들어가지 않는다. / Only the `libXbSymbolDatabase` library is built, linked into the `rexbox_xdk_symbols` tool; it does not enter the game executable.

## SDL3

* 출처 / Source: https://github.com/libsdl-org/SDL
* 라이선스 / License: zlib (Copyright (C) 1997-2026 Sam Lantinga). 전문은 `third_party/SDL/LICENSE.txt`. / Full text in `third_party/SDL/LICENSE.txt`.
* 포함 방식 / How included: git submodule `third_party/SDL`, `release-3.4.18`. 정적 라이브러리 `SDL3-static`만 빌드해 `rexbox_platform`에 링크한다(표시·입력·오디오 계약의 백엔드 `src/platform/sdl/`). 카메라, GPU, 2D 렌더러, 햅틱, 센서, 대화상자, 트레이 하위 시스템과 테스트·예제·설치는 끈다. / Only the static library `SDL3-static` is built and linked into `rexbox_platform` (the backend of the display, input and audio contracts, `src/platform/sdl/`); the camera, GPU, 2D renderer, haptic, sensor, dialog and tray subsystems, tests, examples and installation are off.

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
* 라이선스 / License: MIT (Copyright (c) 2014-2026 Omar Cornut). 전문은 `third_party/imgui/LICENSE.txt`. / Full text in `third_party/imgui/LICENSE.txt`.
* 포함 방식 / How included: git submodule `third_party/imgui`, `v1.93.0` (`c1aaac65e`). 코어(`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`)는 정적 라이브러리 `imgui`로 빌드해 앱(설정 메뉴)과 `rexbox_platform`에 링크한다. SDL3·OpenGL3 백엔드(`backends/imgui_impl_sdl3.cpp`, `backends/imgui_impl_opengl3.cpp`)는 `rexbox_platform` 안에서 컴파일한다. / The core (`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`) is built as the static library `imgui`, linked into the app (the settings menu) and `rexbox_platform`; the SDL3 and OpenGL3 backends (`backends/imgui_impl_sdl3.cpp`, `backends/imgui_impl_opengl3.cpp`) compile inside `rexbox_platform`.

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

## jsrf-recomp (data)

* 출처 / Source: https://github.com/andeecollard/jsrf-recomp, `experiments/d3d8_boundary/inferred_names.json`
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz; jsrf-recomp is a fork of xboxrecomp).
* 사용 / Use: `src/game/jsrf/recomp/xdk_symbols_extra.json`의 D3D8 함수 이름과 주소 5개 / five D3D8 function names and addresses in `src/game/jsrf/recomp/xdk_symbols_extra.json`.
