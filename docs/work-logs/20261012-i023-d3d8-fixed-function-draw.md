# Issue #23 작업 로그 — D3D8 HLE 세 번째 묶음: 고정 기능 그리기를 GL로

작업 지시: [20261012-i023](../work-orders/20261012-i023-d3d8-fixed-function-draw.md) · 설계: [20261012-i023](../design/20261012-i023-d3d8-fixed-function-draw.md)

## 요약

`Draw*`가 실제로 그린다. 부팅의 첫 화면인 **"Presented by SEGA" 로고**가 창에 나오고, 프레임 루프는 가상 vblank 시계로 60 Hz에 맞춰 돈다. 그리기를 막던 원인은 두 가지였다.

1. HLE `SetIndices`가 라이브러리 전역 `D3D__IndexData`를 쓰지 않았다. 게임 inline이 이 값을 읽어 인덱스 포인터로 넘기므로 포인터가 0이었다.
2. `D3DCLEAR`를 PC 비트로 읽었다. 게임의 그리기 뒤 `Clear(3)`(Xbox: 깊이+스텐실)가 색까지 지웠다.

상태 setter는 이제 원본과 같은 라이브러리 전역 칸에 쓰고, 그 결과를 `rejsrf_d3d8_check`가 원본과 비교한다(14,018 케이스, 차이 0). 텍스처 포맷, FVF, 고정 기능 GLSL은 순수 함수로 두어 새 `rexbox_d3d8_probe`(73건, CI)로 검사한다. 게임은 로고에서 다음 화면으로 넘어가지 않는다. 페이싱 전에도 같았으며(issue #21), 다음 task 후보로 기록한다.

## 과정

1. **측정**(임시 계측, 커밋하지 않음): export별 호출 수와 그리기 시점 상태를 셌다. 프레임마다 사각형 2개를 그리고, FVF `0x1C4`, 픽셀 셰이더 0, DXT1 텍스처 하나를 쓴다. 인덱스 포인터는 0이었다. 생성 코드에서 게임 함수가 D3D8 전역에 접근하는 곳을 모두 세어 `D3D__IndexData`·dirty 플래그 읽기와 렌더·텍스처 상태 배열 쓰기를 찾았다.
2. **상태 정합성**: `d3d8_globals`(전역 위치를 XDK 변수표에서 찾음)를 만들고, setter가 원본과 같은 칸에 쓰게 했다. 바인딩의 내부 refcount와 `Lock` 표시도 원본대로 처리한다. `rejsrf_d3d8_check`에 "호출 순서열 → 전역 영역 비교" 케이스를 더했다. 비교로 잡은 차이는 다음과 같고, 모두 원본에 맞췄다. `ZBias`가 칸 77~81에도 씀, `NotInline(ZBias)`도 같음, `SetIndices`는 `Lock`을 찍지 않음, void setter의 eax는 비교 대상이 아님.
3. **기본 상태**: 첫 그리기 시점의 배열을 덤프해 `COLORWRITEENABLE`이 0임을 보았다. 원본 하드웨어 초기화가 부르는 기본 상태 루틴 `0x001919C0`을 찾았다. 타이틀 프로파일이 이 주소를 `D3D_InitializeDeviceState`로 바인딩하고 HLE `CreateDevice`가 실행한다. 덤프에서 simple 칸 57~74의 값이 게임이 넘긴 NV2A method 값과 일치함을 확인해 번호표를 확정했다. NV2A 그림자는 없앴다.
4. **순수 부분**: `d3d8_texture_format`(Morton 해제, 16·32비트, DXT1/3/5), `d3d8_fvf`, `d3d8_ff_shader`를 만들고 `rexbox_d3d8_probe`로 검사했다.
5. **GL 백엔드**: 진입점 표를 넓혔다. 렌더 타깃 FBO·blit·BMP 덤프, 텍스처 캐시·샘플러, 프로그램 캐시·스트리밍 버퍼를 더하고 그리기에서 묶었다. `REXBOX_DISPLAY=hidden`과 `REXBOX_FRAME_DUMP`로 창 없이 결과를 확인했다.
6. **디버깅**: 첫 덤프는 검은색이었다. 컬링·깊이·블렌드를 꺼도 같았다. 그리기 직후 중앙 픽셀을 읽으니 색이 있었다(8, 28, 123). 그래서 그린 뒤 지워진다고 판단했고, `Clear(3)`의 비트 해석 문제를 찾았다. Xbox 비트로 고치자 로고가 나왔다.
7. **수직 동기**: `d3d8_vblank` 가상 시계로 `Swap`·`BlockUntilVerticalBlank`·`GetRasterStatus`를 맞췄다. 호스트 vsync는 껐다. 게임의 타이머 스레드(`sub_0013B1C0`)는 vblank마다 카운터를 올린다. 이전에는 초당 수백만 번 돌았고, 이제 60번 돈다.

부수 작업: 빌드를 위해 `third_party/SDL` 서브모듈을 초기화했다. 생성 코드가 `rexbox_xdk_variables[]` 이전 것이어서 다시 생성했다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` | 42/0, 48/0 (Debug·Release) |
| `rexbox_d3d8_probe` (새, CI) | 73 checks, 0 failures (Debug·Release) |
| `rejsrf_decomp_check` | 6 cases, 0 differences |
| `rejsrf_d3d8_check` | 14,018 cases, 0 differences (Debug·Release; setter 461건 추가) |
| `check_hle_pops.py` | 불일치 0 |
| 부팅(창) | 640×480 창에 "Presented by SEGA" 로고, 60 Hz |
| 부팅(`REXBOX_DISPLAY=hidden`, `REXBOX_FRAME_DUMP`) | 프레임 400·600·1,800·8,700 덤프가 모두 같은 로고(145초) |

원본 화면과는 눈으로 비교했다. 로고의 배치와 색(흰 글자, 파란 SEGA, 검은 바탕)이 실기 화면과 같다. 덤프 이미지는 게임 자산이라 커밋하지 않는다.

## 남은 일과 미해결

* **로고에서 머묾**(미확정): 게임은 로고를 그리며 스테이지·"PRESS" 자원을 읽은 뒤 넘어가지 않는다. 사운드 `Play` 미호출, 커널 타이머(`KeSetTimer` 미발화)와의 관련을 다음 task에서 본다.
* **숨김 창 생성이 가끔 멈춤**: `REXBOX_DISPLAY=hidden`에서 8번 중 1번, `CreateDevice` 안의 디스플레이 생성 뒤 진행하지 않는다. 창 모드에서는 이번 작업 중 재현되지 않았다. 원인은 아직 모른다.
* **다음 묶음**(설계 §9): 프로그래머블 정점 셰이더(부팅 중 154개 생성)와 레지스터 컴바이너 픽셀 셰이더(11개), 고정 기능 조명·안개, texgen·텍스처 변환, 큐브맵, `CopyRects`, render-to-texture 샘플링.
* **성능**: 상태를 그리기마다 모두 다시 적용하고 정점을 매번 올린다. 로고 화면에서는 문제가 없고, 게임플레이에서 측정한다.

---

# Issue #23 work log — D3D8 HLE, third batch: fixed-function drawing on GL

Work order: [20261012-i023](../work-orders/20261012-i023-d3d8-fixed-function-draw.md) · Design: [20261012-i023](../design/20261012-i023-d3d8-fixed-function-draw.md)

## Summary

`Draw*` now draws. The boot's first screen, the **"Presented by SEGA" logo**, shows in the window, and the frame loop runs at 60 Hz on a virtual vblank clock. Two things had blocked drawing:

1. The HLE `SetIndices` did not write the library global `D3D__IndexData`, which the game's inline reads and passes as the index pointer, so the pointer was 0.
2. `D3DCLEAR` was read with PC bits, so the game's `Clear(3)` after drawing (on the Xbox: depth + stencil) also wiped the color.

The state setters now write the same library-global slots as the originals, and `rejsrf_d3d8_check` compares the result with them (14,018 cases, 0 differences). Texture formats, FVF and the fixed-function GLSL are pure functions checked by the new `rexbox_d3d8_probe` (73 checks, CI). The game does not move past the logo. It did not before pacing either (issue #21), and this is recorded as a next-task candidate.

## Process

1. **Measurement** (temporary instrumentation, not committed): counted calls per export and the state at draw time. Each frame draws two quads with FVF `0x1C4`, pixel shader 0 and one DXT1 texture, and the index pointer was 0. Counting every access from game functions to the D3D8 globals in the generated code found the `D3D__IndexData` and dirty-flag reads and the render- and texture-state array writes.
2. **State consistency**: added `d3d8_globals` (global locations from the XDK variable table) and made the setters write the original's slots. Bindings also keep the original's internal refcount and `Lock` stamp. `rejsrf_d3d8_check` gained "call sequence → compare the global area" cases. The comparison caught these differences, all now matched: `ZBias` also writes slots 77–81, `NotInline(ZBias)` does the same, `SetIndices` does not stamp `Lock`, and a void setter's eax is not compared.
3. **Default state**: a dump of the arrays at the first draw showed `COLORWRITEENABLE` at 0, which led to the default-state routine `0x001919C0` that the original hardware initialization calls. The title profile binds that address as `D3D_InitializeDeviceState`, and the HLE `CreateDevice` runs it. The same dump showed slots 57–74 holding the values the game passes with known NV2A methods, which fixed the numbering. The NV2A shadow was removed.
4. **Pure parts**: `d3d8_texture_format` (Morton unswizzling, 16/32-bit, DXT1/3/5), `d3d8_fvf` and `d3d8_ff_shader`, checked by `rexbox_d3d8_probe`.
5. **GL backend**: a wider entry-point table; render-target FBOs, blit and BMP dumps; the texture cache and samplers; the program cache and streaming buffers; all tied together in the draw path. `REXBOX_DISPLAY=hidden` and `REXBOX_FRAME_DUMP` let results be checked without a window.
6. **Debugging**: the first dumps were black, even with culling, depth and blending off. Reading the center pixel right after the draw showed color (8, 28, 123), so the image was being wiped after drawing. That led to the bit reading of `Clear(3)`. With the Xbox bits the logo appeared.
7. **Vertical blank**: the `d3d8_vblank` virtual clock paces `Swap`, `BlockUntilVerticalBlank` and `GetRasterStatus`, with host vsync off. The game's timer thread (`sub_0013B1C0`) increments a counter every vblank; it used to loop millions of times per second and now runs 60.

Side work: the `third_party/SDL` submodule was initialized for the build, and the generated code, older than `rexbox_xdk_variables[]`, was regenerated.

## Verification

The table in the Korean section applies: Debug and Release builds with zero warnings on our targets; kernel and DSOUND probes 42/0 and 48/0; the new `rexbox_d3d8_probe` 73 checks with 0 failures; `rejsrf_decomp_check` 6/0; `rejsrf_d3d8_check` 14,018 cases with 0 differences (461 setter cases added); `check_hle_pops.py` 0 mismatches. A windowed boot shows the logo at 60 Hz in a 640×480 window. A hidden boot with frame dumps saves the same logo for frames 400, 600, 1,800 and 8,700 over 145 s.

Comparison with the original is visual: the logo's layout and colors (white text, blue SEGA, black background) match real hardware. Dump images are game assets and are not committed.

## Remaining and unresolved

* **Stays on the logo** (unresolved): while drawing the logo the game reads the stage and "PRESS" assets, then does not move on. The next task looks at the missing sound `Play` and the kernel timer (`KeSetTimer` never fires).
* **Hidden window creation sometimes stalls**: with `REXBOX_DISPLAY=hidden`, one run in eight stops after the display creation inside `CreateDevice`. It was not seen in windowed mode during this work. The cause is unknown.
* **Next batch** (design §9): programmable vertex shaders (154 created during boot) and register-combiner pixel shaders (11), fixed-function lighting and fog, texgen and texture transforms, cube maps, `CopyRects`, and sampling render-to-texture targets.
* **Performance**: all state is reapplied for every draw and vertices are uploaded every time. That is fine on the logo screen and will be measured in gameplay.
