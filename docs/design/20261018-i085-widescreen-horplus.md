# 설계: Hor+ 와이드스크린 (issue #85)

## 1. 목표

* 4:3에서 보이던 것은 모두 그대로 보이고, 와이드스크린에서는 잃는 것 없이 좌우가 더 보인다(Hor+).
* 16:9와 21:9를 지원한다.
* 늘릴 수 없는 것(게임 내 OSD, HUD, 2D 화면)은 가운데에 4:3 크기 그대로 둔다.

## 2. 조사(**확인됨**, 데모 프레임 4650과 1500의 그리기 설문)

* 게임은 Xbox 와이드 플래그를 무시하고 늘 640x480으로 그린다(issue #85).
* 프레임 4650의 그리기 269건은 세 경로로 나뉜다.
  * **고정 기능 3D(180건)**: 모두 같은 원근 투영이다. `P[0]` = 1(x 배율), `P[5]` = 1.333(y 배율), `P[11]` = 1. 종횡비 4:3이 `P[5]/P[0]`에 들어 있다. 뷰포트는 늘 0,0 640x480이다.
  * **정점 프로그램(89건)**: 모두 뷰포트 상수 `c[58]` = (320, −240, 1, 1), `c[59]` = (320, 240, 0, 0)로 640x480 화면 좌표를 만든다(투영은 프로그램 상수 어딘가에 있고 따로 고칠 필요가 없다).
  * **RHW(미리 변환된 정점)**: 프레임 1500의 로고 화면은 이 경로다(x 0~640, y 0~480의 화면 픽셀).
* 세 경로가 모두 "640x480 기준의 4:3 클립 공간"으로 모인다. 우리 셰이더는 세 경로 모두에서 마지막에 `gl_Position`을 만든다(`d3d8_ff_shader.cpp`의 두 분기, `d3d8_vsh.cpp`의 에필로그).
* 백버퍼 말고 다른 렌더 타깃(512x512 텍스처)도 쓴다. 그것은 게임이 텍스처로 다시 읽으므로 크기를 바꾸면 안 된다.
* 현재 Widescreen 설정은 4:3 그림을 present에서 16:9로 늘린다.

## 3. 설계

### 3.1 원리

백버퍼(와 그 깊이 버퍼)만 너비 W로 넓힌다(16:9: 854, 21:9: 1120; 높이 480 기준, 짝수). 백버퍼에 그리는 모든 정점의 클립 x에 `k = 640 / W`를 곱하고, 뷰포트는 W 너비 전체로 한다.

* 픽셀 x = clip.x / w × (W/2) × k + W/2 = clip.x / w × 320 + W/2. 원래 보이던 기하(|clip.x/w| ≤ 1)는 가운데 640픽셀의 **같은 자리**에 그려지고, 전에 잘리던 기하(1 < |clip.x/w| ≤ W/640)가 양옆에 나타난다. 세로 시야는 그대로다. 이것이 Hor+다.
* 같은 식이 2D에도 적용된다. 화면 좌표 0~640으로 만든 클립 공간(RHW, 프로그램의 `c[58]`/`c[59]`, 직교 투영)은 모두 |clip.x/w| ≤ 1이므로 가운데 640픽셀에 그대로 놓인다. HUD를 따로 가려낼 필요가 없다.
* 투영 행렬이나 프로그램 상수는 건드리지 않는다. 게임이 보는 모든 값(뷰포트, 백버퍼 크기, 상수)은 640x480 그대로다.

### 3.2 타깃(`d3d8_gl_targets`)

* `ColorTarget`/`DepthTarget`에 `wide`(넓힌 게스트 픽셀 너비, 0이면 그대로)를 둔다. GL 저장 너비는 `(wide ? wide : width) × scale`이다.
* 백버퍼(`ResourceKind::kBackBuffer`)와 깊이 스텐실(`kDepthStencil`)만 `wide = WideWidth(aspect, height)`다. 다른 타깃은 0이다.
* `wide`가 바뀌면 배율이 바뀔 때처럼 타깃을 다시 만든다(옛 픽셀을 새 크기로 늘려 복사). 변경은 present 끝에 적용한다(`g_pending_aspect`).
* `BindRenderTarget`이 묶인 색 타깃의 `k = width / wide`(없으면 1)를 `RenderTargetWideFactor()`로 둔다.
* MSAA 렌더버퍼, resolve, 샘플용 복사, `ReadPixels`(덤프·캡처는 W×480으로 저장), present의 원본 크기 모두 GL 너비를 쓴다.
* present: 그림 종횡비는 GL 너비/높이다. 늘리기는 없앤다.

### 3.3 그리기(`d3d8_draw`, 셰이더)

* 고정 기능 정점 셰이더와 정점 프로그램 GLSL에 `uniform float u_wide`를 두고 `gl_Position.x *= u_wide`(프로그램은 `clip.x × u_wide`)한다. 매 그리기마다 `RenderTargetWideFactor()`를 넣는다(유니폼 캐시가 같은 값은 거른다).
* GL 뷰포트: x와 너비를 `scale / k`배 한다(0,0,640x480 → 0,0,W×scale,480×scale). y는 그대로다. 부분 뷰포트도 비례해 넓어지고, 그 안의 내용은 뷰포트 중심 기준 같은 자리에 놓인다.
* `D3DDevice_Clear`의 사각형 시저도 x를 `scale / k`배 한다.
* RHW(화면 좌표) 그리기는 가운데 4:3 영역으로 시저한다. 하드웨어가 640픽셀 가장자리에서 잘라내던 것과 같다.
* 화면 전체(0..640 × 0..480)를 덮고 텍스처를 샘플하지 않는 RHW 사각형(정점 6개 이하)은 전체 화면 효과(페이드, 그림자·색조 패스)로 보고 넓힌 백버퍼 전체로 늘린다(클립 x 배율 1, 시저 없음). 그대로 두면 가운데 4:3만 어두워지고 양옆이 밝게 남아 경계가 보인다(**확인됨**, 사용자 플레이 화면). 텍스처가 있는 전체 화면 사각형(2D 그림)은 늘리지 않고 가운데에 둔다.
* `D3DDevice_SetViewport`는 GL 뷰포트를 직접 놓지 않는다(뷰포트 상수만 쓴다). 그리기 상태 캐시가 매 그리기마다 배율과 넓힘을 더해 뷰포트를 놓으므로, 직접 놓으면 캐시가 모르는 640 너비 뷰포트가 남아 메뉴의 RHW 글자가 왼쪽으로 몰렸다(**확인됨**, LOAD 화면).
* 셰이더 소스가 바뀌므로 프로그램 바이너리 캐시는 다시 컴파일한다.

### 3.4 설정

* `DisplayDefaults.widescreen`(bool)을 `aspect`(`AspectMode`: 4:3, 16:9, 21:9)로 바꾼다. `Aspect()`/`SetAspect()`. 실행 중 바꾸면 다음 present에 적용된다.
* `rejsrf.ini`: `Aspect = 4:3|16:9|21:9`(기본 4:3). 예전 `Widescreen = true`는 16:9로 읽는다.
* 메뉴 Graphics 탭: Widescreen 체크박스를 "Aspect ratio" 콤보로 바꾼다. 설명: Hor+, HUD는 가운데.
* 창 종횡비(`SetPictureAspect`)와 Window Size 프리셋은 W×480 기준이다(16:9: 854x480의 1~3배).

### 3.5 알려진 한계

* 게임의 절두체 컬링은 4:3 기준이라, 화면 가장자리에서 물체가 늦게 나타날 수 있다. 게임 로직을 고치지 않는 한 남는다.
* 게임이 백버퍼를 텍스처로 읽어 전체 화면 효과를 그리면, 그 사각형은 가운데 640에만 놓인다. 데모와 지금까지의 플레이 로그에서는 백버퍼를 읽는 경우가 없었다(512x512 타깃만 읽는다).
* 화면 좌표로 계산하는 효과(예: 화면 공간 텍스처 좌표)가 있다면 가운데 640 기준이다.
* 2D 메뉴(LOAD 화면 등)의 패널은 원근 투영을 쓰는 고정 기능 3D 모델이라 Hor+로 양옆에 패널 아트가 더 보인다(**확인됨**, 캡처의 `proj`). 글자(RHW)와 상대 위치는 4:3과 같다. 메뉴만 4:3으로 가두려면 "정점 프로그램을 쓰지 않는 프레임은 UI 프레임"처럼 프레임 단위 판정이 필요한데, 아직 넣지 않았다.
* 타이틀 화면은 카메라 주변 4:3 시야 바로 바깥에 반투명 세로 띠(정점 프로그램 `03FD63C1`, 월드·투영 변환을 거치는 3D 사각형)를 두는데, 와이드에서는 양옆에 보인다(**확인됨**, 데모 프레임 3000의 그리기별 픽셀 조사). 일반 규칙으로 가려낼 근거가 없어 그대로 둔다. 게임 플레이 화면(프레임 4650)에는 없다.

## 4. 검증

| 검증 | 기대 |
|---|---|
| 빌드, 회귀 도구 | 통과 |
| 4:3 데모 프레임 | 변경 전과 같음 |
| 16:9·21:9 데모 프레임 | 가운데 640x480 영역이 4:3 프레임과 같고, 양옆에 장면이 이어짐. HUD 문자가 가운데 |
| 실행 중 전환 | 4:3 ↔ 16:9 ↔ 21:9가 다음 프레임에 적용 |

---

# Design: Hor+ widescreen (issue #85)

## 1. Goal

* Everything visible at 4:3 stays visible; widescreen adds picture at the sides and loses nothing (Hor+).
* 16:9 and 21:9.
* What cannot be widened (in-game OSD, HUD, 2D screens) stays centered at its 4:3 size.

## 2. Findings (**confirmed**, a draw survey of demo frames 4650 and 1500)

* The game ignores the Xbox widescreen flag and always draws 640x480 (issue #85).
* Frame 4650's 269 draws take three paths.
  * **Fixed-function 3D (180)**: one perspective projection throughout: `P[0]` = 1 (x scale), `P[5]` = 1.333 (y scale), `P[11]` = 1. The 4:3 aspect sits in `P[5]/P[0]`. The viewport is always 0,0 640x480.
  * **Vertex programs (89)**: all make 640x480 screen coordinates with the viewport constants `c[58]` = (320, −240, 1, 1) and `c[59]` = (320, 240, 0, 0) (the projection lives somewhere in the program's constants and needs no change).
  * **RHW (pretransformed)**: frame 1500's logo screen (screen pixels, x 0..640, y 0..480).
* All three meet in a "4:3 clip space for 640x480", and our shaders build `gl_Position` last on each path (the two branches in `d3d8_ff_shader.cpp`, the epilogue in `d3d8_vsh.cpp`).
* Render targets other than the back buffer (a 512x512 texture) are read back as textures, so their size must not change.
* The current Widescreen setting stretches the 4:3 picture to 16:9 at present.

## 3. Design

### 3.1 Principle

Only the back buffer (and its depth buffer) is widened to W (16:9: 854, 21:9: 1120, for height 480, even). Every vertex drawn to the back buffer has its clip x multiplied by `k = 640 / W`, and the viewport spans the full W.

* Pixel x = clip.x / w × (W/2) × k + W/2 = clip.x / w × 320 + W/2. Geometry that was visible (|clip.x/w| ≤ 1) lands at the **same place** in the middle 640 pixels; geometry that was clipped before (1 < |clip.x/w| ≤ W/640) appears at the sides. The vertical field of view is unchanged. That is Hor+.
* The same rule covers 2D. Clip positions made from 640-wide screen coordinates (RHW, the programs' `c[58]`/`c[59]`, orthographic projections) all satisfy |clip.x/w| ≤ 1, so they stay in the middle 640 pixels. The HUD needs no detection.
* Projection matrices and program constants are untouched; everything the game sees (viewport, back buffer size, constants) stays 640x480.

### 3.2 Targets (`d3d8_gl_targets`)

* `ColorTarget`/`DepthTarget` get `wide` (the widened width in guest pixels, 0 for none). GL storage width is `(wide ? wide : width) × scale`.
* Only the back buffer (`ResourceKind::kBackBuffer`) and the depth stencil (`kDepthStencil`) get `wide = WideWidth(aspect, height)`; other targets keep 0.
* A changed `wide` remakes the target as a changed scale does (old pixels stretched into the new size); the change applies at the end of a present (`g_pending_aspect`).
* `BindRenderTarget` publishes the bound color target's `k = width / wide` (1 without) as `RenderTargetWideFactor()`.
* The MSAA renderbuffer, resolve, the sampling copy, `ReadPixels` (dumps and captures come out W×480) and present's source size all use the GL width.
* Present: the picture's aspect is GL width over height; the stretch goes away.

### 3.3 Drawing (`d3d8_draw`, shaders)

* The fixed-function vertex shader and the vertex-program GLSL get `uniform float u_wide` and `gl_Position.x *= u_wide` (`clip.x × u_wide` for programs). Every draw sets `RenderTargetWideFactor()` (the uniform cache drops repeats).
* GL viewport: x and width are scaled by `scale / k` (0,0,640x480 → 0,0,W×scale,480×scale); y is unchanged. A partial viewport widens in proportion and its contents keep their place about its centre.
* `D3DDevice_Clear`'s rectangle scissors scale x by `scale / k` too.
* RHW (screen-space) draws are scissored to the middle 4:3 region, as the hardware clipped them at the 640-pixel edge.
* An RHW quad (up to six vertices) that covers the whole picture (0..640 × 0..480) and samples no texture is a full-screen effect (a fade, a shadow or tint pass) and is stretched across the whole widened back buffer (clip x scale 1, no scissor). Left alone it darkens only the 4:3 middle and leaves the sides bright, with a visible edge (**confirmed**, the user's play screenshot). Textured full-screen quads (2D pictures) are not stretched and stay centered.
* `D3DDevice_SetViewport` no longer sets the GL viewport itself (it writes the viewport constants only). The draw-state cache sets the viewport per draw with the scale and the widening; a direct set left a 640-wide viewport the cache did not know about, which pushed the menus' RHW text to the left (**confirmed**, the LOAD screen).
* The shader sources change, so the program binary cache recompiles.

### 3.4 Settings

* `DisplayDefaults.widescreen` (bool) becomes `aspect` (`AspectMode`: 4:3, 16:9, 21:9), with `Aspect()`/`SetAspect()`; a change while running applies at the next present.
* `rejsrf.ini`: `Aspect = 4:3|16:9|21:9` (default 4:3); the old `Widescreen = true` reads as 16:9.
* Graphics tab: the Widescreen checkbox becomes an "Aspect ratio" combo, with a tooltip (Hor+, HUD centered).
* The window's aspect (`SetPictureAspect`) and the Window Size presets use W×480 (16:9: 1x-3x of 854x480).

### 3.5 Known limits

* The game's frustum culling is 4:3, so objects can appear late at the screen edges. That stays unless game logic is patched.
* If the game reads the back buffer as a texture for a full-screen effect, that quad covers only the middle 640. The demo and the play logs so far never read the back buffer (only the 512x512 target).
* Effects computed from screen coordinates (screen-space texture coordinates, say) are relative to the middle 640.
* The panels of the 2D menus (the LOAD screen, say) are fixed-function 3D models under a perspective projection, so Hor+ shows more of their art at the sides (**confirmed**, the capture's `proj`); their placement against the RHW text is as at 4:3. Confining menus to 4:3 would need a per-frame judgement such as "a frame that uses no vertex program is a UI frame", not done yet.
* The title screen places translucent vertical bars (vertex program `03FD63C1`, 3D quads through the world and projection transforms) just outside the 4:3 view around the camera, and widescreen shows them at the sides (**confirmed**, a per-draw pixel probe of demo frame 3000). No general rule tells them apart, so they stay. The gameplay picture (frame 4650) has none.

## 4. Verification

| Check | Expected |
|---|---|
| Builds, regression tools | pass |
| 4:3 demo frames | identical to before |
| 16:9 and 21:9 demo frames | the middle 640x480 equals the 4:3 frame, the scene continues at the sides, HUD text centered |
| Switching while running | 4:3 ↔ 16:9 ↔ 21:9 applies at the next frame |
