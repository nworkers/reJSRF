# 서드파티 고지 / Third-Party Notices

reJSRF는 MIT 라이선스다. 아래 서드파티 코드가 저장소에 포함되거나 옮겨져 있다.

reJSRF is MIT licensed. The third-party code below is included in, or ported into, the repository.

## xboxrecomp

* 출처 / Source: https://github.com/sp00nznet/xboxrecomp
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz). 전문은 `third_party/xboxrecomp/LICENSE`. / Full text in `third_party/xboxrecomp/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/xboxrecomp`. v0.13.1 (`193e2995ffaec871bb3be43a43bd66aa5fccfeb1`) 위에 lifter 수정 하나(`xadd`/`lock xadd` 플래그, `e108d0e9`)를 더한 포크 `nworkers/xboxrecomp` 브랜치 `rejsrf`를 가리킨다. 이 브랜치는 lifter 수정(`e108d0e9`, PR [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198))과 디스어셈블러 수정(`2649f5f7`, PR [#199](https://github.com/sp00nznet/xboxrecomp/pull/199))을 합친 것(`0a472deb`)이며, 둘 다 머지되면 submodule을 업스트림으로 되돌린다. / Points at the fork `nworkers/xboxrecomp`, branch `rejsrf` (`0a472deb`): v0.13.1 (`193e2995ffaec871bb3be43a43bd66aa5fccfeb1`) plus a lifter fix (`e108d0e9`, [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198)) and a disassembler fix (`2649f5f7`, [#199](https://github.com/sp00nznet/xboxrecomp/pull/199)); the submodule returns to upstream once both are merged.
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

## jsrf-recomp (data)

* 출처 / Source: https://github.com/andeecollard/jsrf-recomp, `experiments/d3d8_boundary/inferred_names.json`
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz; jsrf-recomp is a fork of xboxrecomp).
* 사용 / Use: `src/game/jsrf/recomp/xdk_symbols_extra.json`의 D3D8 함수 이름과 주소 5개 / five D3D8 function names and addresses in `src/game/jsrf/recomp/xdk_symbols_extra.json`.
