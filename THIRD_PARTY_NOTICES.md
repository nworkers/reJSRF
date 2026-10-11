# 서드파티 고지 / Third-Party Notices

reJSRF는 MIT 라이선스다. 아래 서드파티 코드가 저장소에 포함되거나 옮겨져 있다.

reJSRF is MIT licensed. The third-party code below is included in, or ported into, the repository.

## xboxrecomp

* 출처 / Source: https://github.com/sp00nznet/xboxrecomp
* 라이선스 / License: MIT (Copyright (c) 2026 sp00nz). 전문은 `third_party/xboxrecomp/LICENSE`. / Full text in `third_party/xboxrecomp/LICENSE`.
* 포함 방식 / How included: git submodule `third_party/xboxrecomp`, v0.13.1 (`193e2995ffaec871bb3be43a43bd66aa5fccfeb1`).
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
