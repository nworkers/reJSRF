# 작업 로그: 흰 윤곽선 조사용 정점 셰이더 그리기 감시 (issue #70)

설계: [20261016-i070](../design/20261016-i070-outline-watch.md) · 작업 지시: [20261016-i070](../work-orders/20261016-i070-outline-watch.md)

## 한 일

1. 조사(임시 계측, 커밋하지 않음)
   * 데모에서 프로그램 그리기의 핸들, 컬링, `c[96..97]`, 0번 스테이지 상태를 셰이더·컬링 조합마다 한 번 찍었다.
   * 컬링이 반대(`0x900`)인 그리기는 `66950977`과 `66942593` 둘뿐이다. 둘 다 색을 `c[97] = (0.08, 0, 0, 1)`에서 받는다.
   * `66949121`(`c[101]`이 색·기준·두께)은 데모에서 그려지지 않는다.
2. 가설 배제
   * 게임 코드에 NV2A 상수 메서드(`0x1EA4`, `0x0B80`) 즉값이 없다. 인라인 상수 쓰기가 없다는 뜻이다.
   * `LoadVertexShader`/`SelectVertexShader` 호출 지점이 없다.
3. `d3d8_draw_watch.h/.cpp`: `REXBOX_VSH_WATCH`. 프로그램 그리기에서 부른다.
4. 문서: [outlines](../analysis/outlines.md)(새 문서, 색인), ARCHITECTURE.md, 가이드.

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드 | 새 경고 없음 |
| 데모, `REXBOX_VSH_WATCH=66950977,66942593:96-98` | 66줄(`66950977` 64줄 상한, `66942593` 2줄). `c[97] = (0.08 0 0 1)`, `c[98].y = 0.3`, 컬링 `900` |
| 데모 60초, 변수 없음 | 출력 없음 |
| Debug 빌드 | 새 경고 없음 |
| 사용자 플레이 | 대기. 흰 윤곽선이 보일 때의 `[vsh watch]` 줄이 필요 |

## 추가: `all` 모드 (설계 §2.1)

* 사용자 첫 플레이 로그에 감시 줄이 없었다. 플레이에서는 데모 핸들이 쓰이지 않는다.
* `REXBOX_VSH_WATCH=all`을 더했다. Debug 데모 150초에서 19개 핸들 × 4줄 = 76줄이 나왔다.

## 추가: 프레임 캡처 (설계 §2.2)

* 두 번째 플레이 로그: 윤곽선 셰이더 `66950913`, `66942593`의 `c[97] = (0.08, 0, 0, 1)`이다. 데모와 같고 거의 검정이다. 흰 테두리는 윤곽선 그리기의 상수 탓이 아니다.
* `d3d8_capture.h/.cpp`, 플랫폼 `ConsumeCaptureRequest`(SDL: F9), `REXBOX_CAPTURE_FRAME`을 더했다.
* 검증:
  * Release/Debug 빌드에서 경고가 없다.
  * 데모 `REXBOX_CAPTURE_FRAME=4650`에서 275개 호출 목록과 BMP가 나왔다.
  * 보이는 창에 PowerShell `SendKeys`로 보낸 F9는 게임에 닿지 않았다(활성화는 성공). 실제 키보드 확인은 사용자에게 맡긴다.

## 추가: 소유 지도 (설계 §2.3)

* 사용자 캡처: 7459·7171의 윤곽선이 밝은 회색이다. 그런데 윤곽선 그리기의 상태(`c[97]` 거의 검정, MODULATE, 텍스처 없음)대로라면 검어야 한다.
* 캡처에 소유 지도(`_owner.bmp`), 뷰포트, 원시 헤더 필드, 작은 그리기의 정점을 더했다. `d3d8_gl_targets`는 `ReadRenderTarget`, `WriteBmp`로 나눴다.
* 검증: Release 빌드 경고 없음. 데모 `REXBOX_CAPTURE_FRAME=4650`에서 282개 호출, `_owner.bmp`, 정점 줄을 확인했다.

## 추가: 픽셀마다 최근 세 번의 기록

* 사용자 캡처 7499에서 윤곽선 픽셀은 정확히 (128,128,128)이다. 그런데 소유 지도의 마지막 호출은 스텐실 그림자 칠하기(#253, 화면 전체를 절반 어둡게 함)다. 칠하기 전에는 흰색(255)이었다는 뜻이다.
* 데모 캡처의 소유 지도: 윤곽선 그리기는 정상적으로 (20,0,0)을 칠한다(**확인됨**).
* 칠하기 전의 작성자를 보려고, 픽셀마다 최근 세 호출과 그 직후 색을 남긴다(`_owner2.bmp`, `_owner3.bmp`, `_color2.bmp`, `_color3.bmp`). 데모 캡처로 확인했다.

## 추가: 원인과 수정 (텍스처 없는 스테이지)

* 사용자 캡처 9753·10055(3단 소유 지도):
  * 윤곽선을 흰색으로 칠한 것은 윤곽선 그리기 자신이다(10055의 #139·#151). 그 뒤 그림자 칠하기가 128로 만든다.
  * 9753의 흰 픽셀은 트럭 아래 흰 덩어리다(#139, `vs 03FD75C1`).
* 원인과 규칙: [outlines §2](../analysis/outlines.md). `d3d8_ff_shader.cpp`의 `ReadsTexture`를 더했다. 텍스처 없는 스테이지에서 텍스처를 읽는 색 연산은 current를 유지하고, 알파는 이전대로 1을 읽는다.
* 검증:
  * `rexbox_d3d8_probe` 162/0(새 검사 2개).
  * 데모 프레임 비교: 600·2400·3200·6000은 같다. 4650은 아래쪽 흰 조각 545픽셀만 달라졌다.
  * 거쳐 간 두 가설("검정", "알파도 건너뜀")은 데모 화면이 깨져서 버렸다.
* 실제 플레이 확인은 사용자에게 필요하다.

## 남은 것

* 사용자 보고를 받아 원인을 정하고, 이 issue에 수정 설계를 이어 쓴다.

---

# Work log: watching vertex-shader draws to investigate white outlines (issue #70)

Design: [20261016-i070](../design/20261016-i070-outline-watch.md) · Work order: [20261016-i070](../work-orders/20261016-i070-outline-watch.md)

## Done

1. Investigation (temporary trace, not committed)
   * In the demo, logged the handle, culling, `c[96..97]` and stage-0 state of program draws once per shader and culling combination.
   * Only `66950977` and `66942593` draw with reversed culling (`0x900`); both take their color from `c[97] = (0.08, 0, 0, 1)`.
   * `66949121` (where `c[101]` is the color, threshold and thickness) is never drawn in the demo.
2. Hypotheses ruled out
   * The game code has no immediates of the NV2A constant methods (`0x1EA4`, `0x0B80`), so it has no inline constant writes.
   * There are no call sites of `LoadVertexShader`/`SelectVertexShader`.
3. `d3d8_draw_watch.h/.cpp`: `REXBOX_VSH_WATCH`, called from the program draw path.
4. Documents: [outlines](../analysis/outlines.md) (new, indexed), ARCHITECTURE.md, the guide.

## Verification

| Check | Result |
|---|---|
| Release build | no new warnings |
| Demo, `REXBOX_VSH_WATCH=66950977,66942593:96-98` | 66 lines (64, the cap, for `66950977`; 2 for `66942593`); `c[97] = (0.08 0 0 1)`, `c[98].y = 0.3`, culling `900` |
| Demo, 60 s, no variable | no output |
| Debug build | no new warnings |
| User play | waiting: needs the `[vsh watch]` lines from when white outlines show |

## Added: the `all` mode (design §2.1)

* The user's first play log had no watch lines; play does not use the demo's handles.
* Added `REXBOX_VSH_WATCH=all`. A 150 s Debug demo run printed 19 handles × 4 lines = 76 lines.

## Added: frame capture (design §2.2)

* Second play log: outline shaders `66950913` and `66942593` have `c[97] = (0.08, 0, 0, 1)`, as in the demo and almost black. The white rim is not caused by the outline draw's constants.
* Added `d3d8_capture.h/.cpp`, the platform's `ConsumeCaptureRequest` (SDL: F9) and `REXBOX_CAPTURE_FRAME`.
* Verification:
  * No warnings in the Release and Debug builds.
  * The demo with `REXBOX_CAPTURE_FRAME=4650` wrote a 275-call list and a BMP.
  * F9 sent by PowerShell `SendKeys` to the visible window did not reach the game (activation succeeded); checking with a real keyboard is left to the user.

## Added: owner map (design §2.3)

* The user's captures: the outlines in 7459 and 7171 are light grey, while the outline draws' state (`c[97]` almost black, MODULATE, no texture) says black.
* The capture gained the owner map (`_owner.bmp`), the viewport, the raw header fields and the vertices of small draws; `d3d8_gl_targets` split out `ReadRenderTarget` and `WriteBmp`.
* Verification: no Release warnings; the demo with `REXBOX_CAPTURE_FRAME=4650` gave 282 calls, `_owner.bmp` and vertex lists.

## Added: the last three writers of each pixel

* In the user's capture 7499 the outline pixels are exactly (128,128,128), and their last writer in the owner map is the stencil shadow fill (#253, which halves the whole picture). So they were white (255) before the fill.
* The demo capture's owner map shows the outline draws painting (20,0,0) as they should (**confirmed**).
* To see the writer before the fill, each pixel now keeps its last three calls and the color right after each (`_owner2.bmp`, `_owner3.bmp`, `_color2.bmp`, `_color3.bmp`), checked with a demo capture.

## Added: the cause and the fix (stages without a texture)

* The user's captures 9753 and 10055 (three-deep owner maps):
  * The outline draws themselves paint the outline white (#139 and #151 in 10055); the shadow fill then makes it 128.
  * The white pixels in 9753 are the white patch under the truck (#139, `vs 03FD75C1`).
* Cause and rule: [outlines §2](../analysis/outlines.md). Added `ReadsTexture` in `d3d8_ff_shader.cpp`: on a stage without a texture, a color operation that reads the texture keeps the current color, and alpha still reads 1.
* Verification:
  * `rexbox_d3d8_probe` 162/0 (two new checks).
  * Demo frame comparison: 600, 2400, 3200 and 6000 are unchanged; in 4650 only 545 pixels of white fragments at the bottom changed.
  * Two rules tried on the way (black, and skipping alpha too) broke the demo picture and were dropped.
* Real play still needs the user.

## Left

* Settle the cause from the user's report and continue with a fix design in this issue.
