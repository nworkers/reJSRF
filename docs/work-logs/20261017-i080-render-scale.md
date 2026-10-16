# 작업 로그: 내부 렌더링 해상도 배율 (issue #80)

설계: [20261017-i080](../design/20261017-i080-render-scale.md) · 작업 지시: [20261017-i080](../work-orders/20261017-i080-render-scale.md)

## 한 일

1. `d3d8_gl_targets`
   * 색·깊이 타깃에 `scale`을 더했다. GL 텍스처·렌더버퍼는 `게스트 크기 × 배율`로 만든다. `width`/`height`는 게스트 크기 그대로다.
   * `Color`/`Depth`: 배율이 다르면 다시 만든다. 색은 옛 내용을 새 텍스처로 늘려 복사하고, `sampleable`도 이어 간다.
   * 샘플링 사본, present blit은 물리 크기를 쓴다. `SampleRenderTarget`이 돌려주는 크기는 게스트 크기다.
   * `ReadRenderTarget`은 게스트 크기로 줄여 읽는다(nearest). `DumpRenderTarget`은 물리 크기를 쓴다(`ReadPixelsAt`).
   * `RenderScale()`, `SetRenderScale()`. `REXBOX_RENDER_SCALE`은 처음 쓸 때 읽는다(정적 초기화 순서를 피함). 범위 밖이면 잘라서 보고한다.
   * present 끝에서 `platform::ConsumeRenderScaleRequest()`와 대기 중인 배율을 적용한다. 해석 캐시와 GL 상태 캐시를 무효로 하고, `[d3d8] render scale N`을 낸다.
2. `d3d8_device`: `ApplyViewport`의 GL 뷰포트, `Clear` 사각형의 시저에 배율.
3. `d3d8_draw`: `FixedState` 뷰포트 적용에 배율. 폴리곤 오프셋의 기울기 계수에도 배율을 곱했다. 픽셀당 깊이 기울기가 배율만큼 작아지기 때문이다.
4. 플랫폼: `ConsumeRenderScaleRequest()`(`include/rexbox/platform/display.h`), SDL에서 Ctrl+1~8.
5. 문서: 가이드(환경 변수, 단축키, 덤프 크기), `ARCHITECTURE.md`.

## 검증

| 검증 | 결과 |
|---|---|
| Release, Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 데모, 배율 1 | 프레임 1500, 3000, 4650 BMP가 변경 전과 바이트 단위로 같음. Debug 빌드와 동시에 돌린 첫 실행은 3000, 4650이 달랐다. 장면은 같고 인물·차 위치만 달라서 CPU 부하로 인한 진행 시점 차이로 보이고, 부하 없이 다시 돌리니 같았다 |
| 데모, 배율 2·4·8 | 덤프가 1280×960, 2560×1920, 5120×3840. GL 오류·불완전 FBO 보고 없음. 배율 4 프레임 4650을 줄여 보면 배율 1과 같은 장면이고, 원래 크기로 보면 계단 현상 없이 선명하다 |
| 실행 중 변경(Ctrl+1~8), 부스트·그래피티·윤곽선 | 미검증(창 입력이 필요하다) |

---

# Work log: internal rendering resolution scale (issue #80)

Design: [20261017-i080](../design/20261017-i080-render-scale.md) · Work order: [20261017-i080](../work-orders/20261017-i080-render-scale.md)

## Done

1. `d3d8_gl_targets`
   * Color and depth targets gained `scale`. GL textures and renderbuffers are made at `guest size × scale`; `width`/`height` stay the guest size.
   * `Color`/`Depth` remake a target whose scale differs. For color, the old contents are copied, stretched, into the new texture, and `sampleable` carries over.
   * Sampling copies and the present blit use the physical size. The size `SampleRenderTarget` returns is the guest size.
   * `ReadRenderTarget` reads at the guest size, scaled down (nearest). `DumpRenderTarget` writes the physical size (`ReadPixelsAt`).
   * `RenderScale()` and `SetRenderScale()`. `REXBOX_RENDER_SCALE` is read on first use (avoiding static-initialization order); out-of-range values are clamped and reported.
   * At the end of a present, `platform::ConsumeRenderScaleRequest()` and any waiting scale apply. The resolved-target and GL state caches are invalidated, and `[d3d8] render scale N` is printed.
2. `d3d8_device`: the scale on `ApplyViewport`'s GL viewport and on the `Clear` rectangles' scissor.
3. `d3d8_draw`: the scale on applying the `FixedState` viewport. The polygon-offset slope factor is scaled too, since the depth slope per pixel shrinks by the scale.
4. Platform: `ConsumeRenderScaleRequest()` (`include/rexbox/platform/display.h`), Ctrl+1 to 8 in SDL.
5. Documents: the guide (environment variable, hotkey, dump sizes) and `ARCHITECTURE.md`.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Demo, scale 1 | frames 1500, 3000 and 4650 are byte-identical to before. A first run alongside the Debug build differed at 3000 and 4650: the same scene with characters and a car moved, which looks like a timing shift from CPU load; run again without load, they matched |
| Demo, scales 2, 4 and 8 | dumps are 1280×960, 2560×1920 and 5120×3840, with no GL error or incomplete-FBO report. Scale 4 frame 4650 scaled down shows the same scene as scale 1, and at full size it is sharp with no stair-stepping |
| Changing at run time (Ctrl+1 to 8); boost, graffiti and outlines | not verified (needs window input) |
