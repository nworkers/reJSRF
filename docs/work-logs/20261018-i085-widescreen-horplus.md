# 작업 로그: Hor+ 와이드스크린 (issue #85)

설계: [20261018-i085](../design/20261018-i085-widescreen-horplus.md) · 작업 지시: [20261018-i085](../work-orders/20261018-i085-widescreen-horplus.md)

## 한 일

1. 조사(임시 그리기 설문, 제거함): 데모 프레임 4650의 그리기 269건이 모두 "640x480 기준 4:3 클립 공간"으로 모이는 것을 확인했다. 고정 기능 3D 180건은 같은 원근 투영(`P[0]` 1, `P[5]` 1.333), 정점 프로그램 89건은 뷰포트 상수 `c[58]` (320, −240), `c[59]` (320, 240)다. 로고 화면(1500)은 RHW다.
2. `d3d8_display`: `AspectMode`(4:3, 16:9, 21:9), `DisplayDefaults.aspect`, `Aspect()`/`SetAspect()`. 창 종횡비는 넓힌 너비(854·1120)×480이다.
3. `d3d8_gl_targets`
   * `ColorTarget`/`DepthTarget`의 `wide`: 백버퍼와 깊이 스텐실만 `WideWidth(aspect, height)`(짝수)로 넓힌다. GL 저장 너비는 `GlWidth()`다.
   * `wide`가 바뀌면 배율처럼 다시 만든다. 변경은 present 끝에 적용하고 `[d3d8] aspect 16:9`를 찍는다. 타깃이 하나도 없을 때(시작 전)는 바로 적용한다.
   * `BindRenderTarget`이 `RenderTargetWideFactor()`(width / wide)를 둔다. MSAA, resolve, 샘플 복사, `ReadPixels`, present가 GL 너비를 쓴다. present의 늘리기는 없앴다.
4. 셰이더와 그리기
   * 고정 기능 정점 셰이더 두 분기와 정점 프로그램 에필로그에 `uniform float u_wide`를 두고 클립 x에 곱한다. 프로그램 구조체에 위치를 두고 그리기마다 넣는다.
   * GL 뷰포트의 x와 너비, `D3DDevice_Clear` 사각형 시저의 x를 `scale / k`배 한다.
   * RHW 그리기는 가운데 4:3 영역으로 시저한다(하드웨어가 640 가장자리에서 잘라내던 것).
5. 설정: `rejsrf.ini`의 `Aspect = 4:3|16:9|21:9`(예전 `Widescreen = true`는 16:9), Graphics 탭의 "Aspect ratio" 콤보와 설명.
6. 타이틀 화면의 세로 띠 조사(임시 픽셀 프로브, 제거함): 프레임 3000에서 양옆의 반투명 세로 띠는 정점 프로그램 `03FD63C1`의 6정점 사각형 13건이고, x 범위가 모두 4:3 영역 밖이다. 이 프로그램은 월드·투영 변환을 거치는 일반 3D 변환이라(셰이더 덤프) 2D 주차 요소로 가려낼 수 없다. 설계 3.5에 적었다.
7. 문서: 가이드, `ARCHITECTURE.md`.

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 4:3 데모 프레임 1500·4650 | 변경 전 빌드와 바이트 단위로 같음 |
| 16:9(854x480) 프레임 1500·4650의 가운데 640 | 1500은 4:3과 같음. 4650은 13,038픽셀이 1~4 단계 다름(평균 0.045, 래스터화 차이), 그 밖은 같음 |
| 21:9(1120x480) 프레임 1500·4650의 가운데 640 | 같은 게임 순간이면 4:3과 같음(한 실행에서 4650이 평균 차이 0.000; 다른 실행은 진행이 한 프레임 어긋나 비교 불가) |
| 양옆 | 장면이 이어지고 HUD(JSRF 로고, 저작권 문자)와 캐릭터는 가운데 같은 자리(몽타주) |
| 실행 중 전환(보이는 창, 메뉴) | 4:3 → 16:9 → 21:9 → 4:3이 바로 적용되고 창이 854·1120·640 기준으로 바뀜. Window Size 라벨 `2x (1708x960)`, `2x (2240x960)` |
| 타이틀 화면 양옆의 세로 띠 | 남아 있음(설계 3.5) |
| 사용자 플레이 | 확인 대기 |

---

# Work log: Hor+ widescreen (issue #85)

Design: [20261018-i085](../design/20261018-i085-widescreen-horplus.md) · Work order: [20261018-i085](../work-orders/20261018-i085-widescreen-horplus.md)

## Done

1. Investigation (a temporary draw survey, removed): all 269 draws of demo frame 4650 meet in a "4:3 clip space for 640x480": 180 fixed-function 3D draws share one perspective projection (`P[0]` 1, `P[5]` 1.333), 89 vertex-program draws use the viewport constants `c[58]` (320, −240) and `c[59]` (320, 240), and the logo screen (1500) is RHW.
2. `d3d8_display`: `AspectMode` (4:3, 16:9, 21:9), `DisplayDefaults.aspect`, `Aspect()`/`SetAspect()`. The window's aspect is the widened width (854 or 1120) by 480.
3. `d3d8_gl_targets`
   * `wide` on `ColorTarget`/`DepthTarget`: only the back buffer and depth stencil are widened to `WideWidth(aspect, height)` (even); the GL storage width is `GlWidth()`.
   * A changed `wide` remakes the target like a changed scale; the change applies at the end of a present (`[d3d8] aspect 16:9`), or at once while no target exists.
   * `BindRenderTarget` publishes `RenderTargetWideFactor()` (width / wide). MSAA, resolve, the sampling copy, `ReadPixels` and present use the GL width; present no longer stretches.
4. Shaders and drawing
   * `uniform float u_wide` in both fixed-function vertex branches and the vertex-program epilogue, multiplied into clip x; located in the program structs and set per draw.
   * The GL viewport's x and width and `D3DDevice_Clear`'s scissor x are scaled by `scale / k`.
   * RHW draws are scissored to the middle 4:3 region (the hardware clipped them at the 640 edge).
5. Settings: `Aspect = 4:3|16:9|21:9` in `rejsrf.ini` (the old `Widescreen = true` reads as 16:9), an "Aspect ratio" combo with a tooltip in the Graphics tab.
6. The title screen's vertical bars (a temporary pixel probe, removed): at frame 3000 the translucent bars at the sides are 13 six-vertex quads of vertex program `03FD63C1`, all outside the 4:3 region. The program is an ordinary 3D transform through the world and projection matrices (shader dump), so they cannot be told apart as parked 2D elements. Noted in design 3.5.
7. Documents: the guide, `ARCHITECTURE.md`.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| 4:3 demo frames 1500 and 4650 | byte-identical to the previous build |
| Middle 640 of the 16:9 (854x480) frames 1500 and 4650 | 1500 equals 4:3; 4650 differs in 13,038 pixels by 1-4 levels (mean 0.045, rasterization), the rest equal |
| Middle 640 of the 21:9 (1120x480) frames 1500 and 4650 | equal to 4:3 at the same game moment (one run: mean difference 0.000 at 4650; another run drifted a frame and could not be compared) |
| The sides | the scene continues; the HUD (JSRF logo, copyright text) and the character stay in the same place in the middle (montage) |
| Switching while running (visible window, menu) | 4:3 → 16:9 → 21:9 → 4:3 apply at once and the window follows 854, 1120 and 640; Window Size reads `2x (1708x960)`, `2x (2240x960)` |
| Vertical bars at the title screen's sides | remain (design 3.5) |
| User play | waiting |
