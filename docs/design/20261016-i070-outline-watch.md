# 설계: 흰 윤곽선 조사용 정점 셰이더 그리기 감시 (issue #70)

## 1. 문제

* 플레이 중 캐릭터 주변 윤곽선이 자주 희게 보인다. 원본에서는 검은색이다. v0.0.29 알려진 문제에는 "윤곽선이 기대보다 가늘다"도 있다.
* 지금까지 확인한 것(데모 실행, 셰이더 덤프, 임시 계측):

  | 윤곽선 셰이더 | 쓰는 곳 | 색 | 두께 | 데모에서 |
  |---|---|---|---|---|
  | `66950977`, `66942593` (스키닝, 컬링 반대 `0x900`) | 데모 캐릭터 | `oD0 = c[97]` | `c[98].y` | `c[97] = (0.08, 0, 0, 1)`: 거의 검정(**확인됨**) |
  | `66949121` (위치·법선만, `sge`) | 플레이에만 나옴 | `oD0 = c[101].xxxx` | `c[101].y` | 그려지지 않음 |

  * `66949121`에서 `c[101].x`는 색이면서 `sge`의 기준값이다. 이 값이 0이 아니라 1에 가까우면 윤곽선은 **희고**, 기준을 넘는 정점이 적어 **가늘어진다**. 두 증상을 함께 설명한다(**추정**).
  * 다른 셰이더들은 `c[97] = (1, 1, 1, 1)`을 쓴다. 윤곽선 그리기가 엉뚱한 상수를 읽어도 흰색이 된다.
* 배제한 가설(**확인됨**):
  * 게임 코드에는 상수를 push buffer에 직접 쓰는 인라인 경로가 없다(NV2A 상수 메서드 즉값 없음).
  * `LoadVertexShader`/`SelectVertexShader`는 호출되지 않는다.
* 플레이 장면에서 그 그리기의 상수와 상태를 봐야 원인을 정할 수 있다. 데모로는 재현되지 않는다.

## 2. 설계

**정점 셰이더 그리기 감시**(진단, `rexbox_hle_d3d8`): 정한 정점 셰이더로 그릴 때, 정한 상수 범위와 그리기 상태를 **값이 달라질 때마다** 한 줄씩 출력한다.

* `REXBOX_VSH_WATCH=<핸들>[:<첫>-<끝>][,<핸들>[:<첫>-<끝>]...]`
  * 핸들은 셰이더 덤프와 같은 10진수다(`0x`를 붙이면 16진수).
  * 범위는 c[] 원시 번호(0..191, 덤프의 표기)다. 없으면 96–103이다.
* 한 줄의 내용: 프레임, 핸들, 상수 값, 컬링, 픽셀 셰이더, 0번 스테이지의 연산·인자·텍스처, 블렌드(켜짐, src/dst), z 쓰기, 텍스처 팩터, 조각 키.
* 같은 내용(해시)은 다시 쓰지 않는다. 핸들마다 최대 64줄까지 쓴다.
* 위치: `d3d8_draw_watch.h/.cpp`. 그리기 경로는 프로그램 그리기에서 한 번 부른다. 꺼져 있으면 비용은 bool 하나다.
* 사용자가 플레이에서 `REXBOX_VSH_WATCH=66949121:96-103,66950977,66942593`으로 실행해 흰 윤곽선이 보일 때의 줄을 보고하면, 다음 단계(수정)를 설계한다.

### 2.1 추가: `REXBOX_VSH_WATCH=all`

* 사용자 플레이(첫 시도)에서는 `[vsh watch]` 줄이 하나도 나오지 않았다. 데모의 세 핸들이 플레이에서는 쓰이지 않는다(**확인됨**). 플레이는 셰이더를 따로 만든다.
* `all`이면 모든 프로그램 핸들을 기본 범위로 감시하되, 핸들마다 4줄까지만 쓴다. `REXBOX_SHADER_DUMP=1`과 함께 실행하면 핸들을 마이크로코드와 맞출 수 있다.

### 2.2 추가: 프레임 캡처 (F9)

* 두 번째 플레이 로그(`all`)에서 플레이의 윤곽선 셰이더는 `66950913`, `66942593`이었고, 색은 `c[97] = (0.08, 0, 0, 1)`로 데모와 같았다(**확인됨**). 윤곽선 그리기의 상수는 원인이 아니다.
* 흰 테두리를 그리는 그리기를 찾아야 한다. 그래서 프레임 하나의 모든 Clear·그리기와 상태를 남기는 캡처를 둔다.
  * 창에서 **F9**를 누르면 다음 프레임을 캡처한다. 플랫폼 계약은 `ConsumeCaptureRequest()` 하나만 더한다.
  * 결과: `capture_<프레임>.txt`(호출 순서, 셰이더, 렌더 타깃, 컬링, 깊이, 블렌드, 알파 테스트, 스텐실, 색 쓰기, 텍스처 팩터, 포그, 조명, 스테이지·텍스처·형식, 프로그램이면 `c[97..98]`), `capture_<프레임>.bmp`. 위치는 `REXBOX_FRAME_DUMP_DIR`이다.
  * `REXBOX_CAPTURE_FRAME=<n>`은 키 없이 n번째 프레임을 캡처한다(창 없는 실행, 검증용).
  * 위치: `d3d8_capture.h/.cpp`. 꺼져 있으면 그리기마다 bool 하나다.

### 2.3 추가: 픽셀 소유 지도, 원시 헤더, 작은 그리기의 정점

* 사용자 캡처(15개)에서 확인한 것:
  * 플레이 프레임 7459와 7171에서 윤곽선 전체가 밝은 회색 선이다.
  * 같은 프레임의 윤곽선 그리기(`vs 03FD9741` = 66950977, 컬링 `900`)는 `c[97] = (0.08, 0, 0, 1)`이다. 0번 스테이지는 MODULATE(텍스처 없음, diffuse)다. 상태대로라면 검게 나와야 한다(**확인됨**).
  * 그래서 어느 그리기가 그 픽셀을 칠했는지를 직접 봐야 한다.
* 캡처에 더하는 것:
  * **소유 지도**: 호출마다 그 뒤 묶인 타깃을 읽어, 바뀐 픽셀의 "마지막 호출"을 기록한다. 보이는 화면의 지도를 `capture_<n>_owner.bmp`로 쓴다(빨강 = 번호+1의 낮은 바이트, 초록 = 높은 바이트).
  * 뷰포트, 렌더 타깃·텍스처 헤더의 원시 형식·크기·데이터 필드.
  * 정점 8개 이하 그리기의 처음 4개 정점(원시 DWORD). 부스트 합성 사각형의 텍스처 좌표를 보기 위해서다(#69).
* 캡처 프레임에서만 호출마다 화면을 읽으므로 그 프레임만 느려진다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 빌드, 회귀 도구 | 경고 0, 통과 |
| 데모, `REXBOX_VSH_WATCH=66950977,66942593` | 두 셰이더의 줄. `c[97] = (0.08, 0, 0, 1)`, 컬링 `900` |
| 데모, 변수 없음 | 출력 없음, 프레임 시간 같음 |

---

# Design: watching vertex-shader draws to investigate white outlines (issue #70)

## 1. Problem

* In play, the outlines around characters often show white; in the original they are black. v0.0.29's known issues also list outlines thinner than expected.
* What is known so far (demo runs, the shader dump, a temporary trace):
  * `66950977` and `66942593` (skinned, reversed culling `0x900`) draw the demo's character outlines with color `oD0 = c[97]` and thickness `c[98].y`; in the demo `c[97] = (0.08, 0, 0, 1)`, almost black (**confirmed**).
  * `66949121` (position and normal only, `sge`) appears only in play; color `oD0 = c[101].xxxx`, thickness `c[101].y`; the demo never draws with it.
  * In `66949121`, `c[101].x` is both the color and `sge`'s threshold. If it were near 1 rather than 0, the outline would be **white**, and few vertices would pass the threshold, so it would be **thin**: one cause for both symptoms (**inferred**).
  * Other shaders use `c[97] = (1, 1, 1, 1)`, so an outline draw that read the wrong constants would also come out white.
* Ruled out (**confirmed**):
  * The game code has no inline path writing constants into the push buffer (no NV2A constant-method immediates).
  * `LoadVertexShader`/`SelectVertexShader` are never called.
* Settling the cause needs the constants and state of that draw in play; the demo does not reproduce it.

## 2. Design

**Vertex-shader draw watch** (a diagnostic in `rexbox_hle_d3d8`): for draws with a chosen vertex shader, print a line with a chosen constant range and the draw state **whenever they change**.

* `REXBOX_VSH_WATCH=<handle>[:<first>-<last>][,<handle>[:<first>-<last>]...]`
  * Handles are decimal, as in the shader dump (`0x` for hexadecimal).
  * The range is in raw c[] numbers (0..191, the dump's notation); without one, 96–103.
* A line holds the frame, the handle, the constant values, culling, the pixel shader, stage 0's operation, arguments and texture, blending (on, src/dst), z writes, the texture factor and the fragment key.
* The same content (hash) is not printed again, and each handle prints at most 64 lines.
* Location: `d3d8_draw_watch.h/.cpp`, called once from the program draw path. When off it costs one bool.
* The user runs play with `REXBOX_VSH_WATCH=66949121:96-103,66950977,66942593` and reports the lines from when white outlines show; the fix is designed from those.

### 2.1 Added: `REXBOX_VSH_WATCH=all`

* The user's first play run printed no `[vsh watch]` line: the demo's three handles are not used in play (**confirmed**), which creates its own shaders.
* `all` watches every program handle with the default range, at most 4 lines each. Run with `REXBOX_SHADER_DUMP=1` to match handles to their microcode.

### 2.2 Added: frame capture (F9)

* The second play log (`all`) showed play's outline shaders are `66950913` and `66942593`, with `c[97] = (0.08, 0, 0, 1)` as in the demo (**confirmed**). The outline draw's constants are not the cause.
* The draw that paints the white rim has to be found, so a capture records every Clear and draw of one frame with its state.
  * Pressing **F9** in the window captures the next frame. The platform contract gains only `ConsumeCaptureRequest()`.
  * Output: `capture_<frame>.txt` (call order, shaders, render target, culling, depth, blending, alpha test, stencil, color writes, texture factor, fog, lighting, stages with textures and formats, and `c[97..98]` for programs) and `capture_<frame>.bmp`, in `REXBOX_FRAME_DUMP_DIR`.
  * `REXBOX_CAPTURE_FRAME=<n>` captures frame n without the key (windowless runs, verification).
  * Location: `d3d8_capture.h/.cpp`; when off, one bool per draw.

### 2.3 Added: pixel owner map, raw headers, vertices of small draws

* From the user's captures (15):
  * In play frames 7459 and 7171 the whole outline is a light grey line.
  * The outline draws in the same frames (`vs 03FD9741` = 66950977, culling `900`) have `c[97] = (0.08, 0, 0, 1)` and stage 0 MODULATE(no texture, diffuse), so by their state they should come out black (**confirmed**).
  * Which draw painted those pixels has to be seen directly.
* Added to the capture:
  * **Owner map**: after each call the bound target is read back and each changed pixel records that call as its last writer. The map of the shown picture is written to `capture_<n>_owner.bmp` (red = low byte of index + 1, green = high byte).
  * The viewport, and the raw format, size and data fields of the render target and texture headers.
  * The first 4 vertices (raw DWORDs) of draws with at most 8 vertices, to see the texture coordinates of the boost composite quad (#69).
* The screen is read after every call only in the captured frame, so only that frame is slow.

## 3. Verification

| Check | Expected |
|---|---|
| Builds, regression tools | no warnings; pass |
| Demo, `REXBOX_VSH_WATCH=66950977,66942593` | lines for both shaders with `c[97] = (0.08, 0, 0, 1)` and culling `900` |
| Demo, no variable | no output; frame time unchanged |
