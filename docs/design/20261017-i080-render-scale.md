# 설계: 내부 렌더링 해상도 배율 (issue #80)

관련: [i023 고정 기능 그리기 §3.5](20261012-i023-d3d8-fixed-function-draw.md), [i071 렌더 타깃 메모리 키](20261017-i071-render-target-by-memory.md)

## 1. 요구

* 모든 렌더 타깃을 게스트 크기의 1~8배로 그려 더 선명한 화면을 얻는다.
* 처음 배율은 환경 변수로 준다(아직 UI가 없다).
* 실행 중에도 배율을 바꿀 수 있어야 한다. UI가 생기기 전까지는 단축키로 바꾼다.

## 2. 설계

### 2.1 원칙: D3D가 보는 크기는 그대로

게임이 보는 값은 바꾸지 않는다. 렌더 타깃 크기, 뷰포트, 뷰포트 상수 `c[58]`/`c[59]`, 선형 텍스처의 텍셀 좌표가 그렇다. 배율은 GL 쪽에만 곱한다.

* 프로그램 셰이더는 화면 좌표를 `c[58]`/`c[59]`로 clip 좌표로 되돌린다. 화면 좌표 정점(고정 기능)은 `u_target`(게스트 크기)으로 NDC로 바꾼다. 둘 다 GL 뷰포트가 픽셀로 매핑하므로, **GL 뷰포트만 배율을 곱하면 된다**(확인됨: `d3d8_vsh.cpp` 에필로그, `d3d8_ff_shader.cpp`).

### 2.2 배율을 곱하는 곳

| 곳 | 바꾸는 것 |
|---|---|
| `d3d8_gl_targets` 색·깊이 타깃 | GL 텍스처·렌더버퍼를 `게스트 크기 × 배율`로 만든다. 게스트 크기와 배율을 함께 기억한다 |
| 샘플링 사본(`SampleRenderTarget`) | 사본과 blit은 물리 크기. 돌려주는 크기는 게스트 크기(선형 텍스처의 `u_tex_scale`이 게스트 텍셀 기준이라) |
| 뷰포트(`ApplyViewport`, 그리기의 `FixedState` 적용) | GL 뷰포트의 x·y·너비·높이에 배율 |
| `Clear`의 사각형 | 시저에 배율 |
| present | 물리 크기에서 창으로 blit(이미 창 크기에 맞춰 늘린다) |
| `ReadRenderTarget` | 게스트 크기로 줄여 읽는다(F9 캡처의 owner 맵이 게스트 픽셀 기준이라) |
| `DumpRenderTarget`(프레임 덤프, 캡처 BMP) | 물리 크기 그대로 쓴다(배율 효과를 확인할 수 있게) |

### 2.3 실행 중 변경

* `SetRenderScale(n)`은 바라는 배율만 기록한다. present가 끝난 뒤, 곧 프레임 사이에서 적용한다. 그래서 한 프레임 안에서 크기가 섞이지 않는다.
* 적용하면 해석해 둔 타깃 캐시와 GL 상태 캐시를 무효로 한다. 각 타깃은 다음에 쓰일 때(`Color`/`Depth`) 배율이 다르면 다시 만든다.
  * 색 타깃은 옛 내용을 새 텍스처로 늘려 복사한다. 그 프레임에 다시 그리지 않고 샘플링만 하는 표면(부스트의 128×128 등)도 내용이 이어진다.
  * 깊이는 다음 프레임에서 지워지므로 복사하지 않는다.
* 입력
  * `REXBOX_RENDER_SCALE=<1..8>`: 처음 배율(기본 1). 범위 밖이면 잘라서 한 번 보고한다.
  * 창에서 **Ctrl+1 … Ctrl+8**: 실행 중 배율 변경(게임 입력에 쓰지 않는 키). 바뀔 때 `[d3d8] render scale N` 한 줄을 낸다.
  * 플랫폼 계층은 요청만 전한다(`ConsumeRenderScaleRequest`). 적용은 D3D8 HLE가 한다. 나중의 UI도 `SetRenderScale`을 부른다.

### 2.4 하지 않는 것

* 선 굵기·점 크기 배율(지금 HLE가 그리지 않는다).
* 배율에 맞춘 텍스처 필터링이나 밉맵 조정, MSAA.
* 8배에서의 메모리: 640×480 백 버퍼는 5120×3840 RGBA8로 약 75 MiB다. 깊이와 다른 타깃도 커진다. 드라이버가 만들지 못하면 GL 오류로 보이므로, 그때 보고를 보고 다룬다.

## 3. 검증

* Release·Debug 빌드, 회귀 도구 세 개.
* 배율 1: 데모 프레임 BMP가 변경 전과 바이트 단위로 같다.
* 배율 2·4·8(hidden 데모): 프레임 덤프 크기가 배율에 맞고, 화면 구성이 배율 1과 같다(축소해 비교).
* 사용자 플레이: Ctrl+1~8로 실행 중에 바뀌는지, 부스트·그래피티·윤곽선이 깨지지 않는지.

---

# Design: internal rendering resolution scale (issue #80)

Related: [i023 fixed-function draw §3.5](20261012-i023-d3d8-fixed-function-draw.md), [i071 render targets keyed by memory](20261017-i071-render-target-by-memory.md)

## 1. Requirement

* Draw every render target at 1x to 8x its guest size for a sharper picture.
* The initial scale comes from an environment variable (there is no UI yet).
* The scale must also change while the game runs. Until there is a UI, a hotkey changes it.

## 2. Design

### 2.1 Principle: the sizes D3D sees stay the same

The values the title sees do not change: render-target sizes, viewports, the viewport constants `c[58]`/`c[59]`, and linear textures' texel coordinates. Only the GL side is scaled.

* Program shaders turn screen coordinates back into clip coordinates with `c[58]`/`c[59]`. Screen-space vertices (fixed function) go to NDC through `u_target` (the guest size). In both cases the GL viewport maps to pixels, so **only the GL viewport needs the scale** (confirmed: the `d3d8_vsh.cpp` epilogue, `d3d8_ff_shader.cpp`).

### 2.2 Where the scale applies

| Where | What changes |
|---|---|
| `d3d8_gl_targets` color and depth targets | GL textures and renderbuffers are made at `guest size × scale`; the guest size and the scale are both kept |
| Sampling copies (`SampleRenderTarget`) | the copy and its blit use the physical size; the size returned is the guest size (a linear texture's `u_tex_scale` counts guest texels) |
| Viewports (`ApplyViewport`, applying a draw's `FixedState`) | the GL viewport's x, y, width and height are scaled |
| `Clear` rectangles | the scissor is scaled |
| Present | blits from the physical size to the window (it already stretches to the window) |
| `ReadRenderTarget` | reads at the guest size, scaled down (the F9 capture's owner maps count guest pixels) |
| `DumpRenderTarget` (frame dumps, capture BMPs) | writes the physical size as it is (so the effect of the scale can be checked) |

### 2.3 Changing at run time

* `SetRenderScale(n)` only records the wanted scale. It applies after a present, that is between frames, so no frame mixes sizes.
* Applying it invalidates the resolved-target cache and the GL state cache. Each target is remade the next time it is used (`Color`/`Depth`) if its scale differs.
  * A color target's old contents are copied, stretched, into the new texture, so a surface only sampled and not redrawn that frame (the boost's 128×128 one, say) keeps its contents.
  * Depth is not copied; the next frame clears it.
* Input
  * `REXBOX_RENDER_SCALE=<1..8>`: the initial scale (default 1). Out-of-range values are clamped and reported once.
  * **Ctrl+1 … Ctrl+8** in the window: change the scale while running (keys the game input does not use). A change prints `[d3d8] render scale N`.
  * The platform layer only passes the request on (`ConsumeRenderScaleRequest`); the D3D8 HLE applies it. A later UI calls `SetRenderScale` too.

### 2.4 Not done

* Scaling line widths and point sizes (the HLE does not draw them yet).
* Texture filtering or mipmap adjustments for the scale, and MSAA.
* Memory at 8x: the 640×480 back buffer becomes 5120×3840 RGBA8, about 75 MiB, and depth and the other targets grow too. If the driver cannot make them, GL errors will show, and the reports will be handled then.

## 3. Verification

* Release and Debug builds, the three regression tools.
* Scale 1: demo frame BMPs byte-identical to before the change.
* Scales 2, 4 and 8 (hidden demo): frame dumps have the scaled size, and the picture matches scale 1 (compared scaled down).
* User play: Ctrl+1 to 8 changes the scale while running, and the boost, graffiti and outlines stay intact.
