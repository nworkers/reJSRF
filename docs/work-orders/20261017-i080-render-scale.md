# 작업 지시: 내부 렌더링 해상도 배율 (issue #80)

설계: [20261017-i080](../design/20261017-i080-render-scale.md)

1. `d3d8_gl_targets`: 타깃의 물리 크기(게스트 × 배율), 배율이 다른 타깃 다시 만들기(색은 내용 복사), `RenderScale()`, `SetRenderScale()`, present 뒤 적용. 샘플링 사본·present·읽기·덤프의 크기.
2. `d3d8_device`, `d3d8_draw`: GL 뷰포트와 `Clear` 시저에 배율.
3. 플랫폼: `ConsumeRenderScaleRequest()`(Ctrl+1~8), `include/rexbox/platform/display.h`.
4. 환경 변수 `REXBOX_RENDER_SCALE`.
5. 검증: 빌드, 회귀 도구, 배율 1 데모 BMP 비교, 배율 2·4·8 데모 덤프.
6. 문서: 작업 로그, 가이드(환경 변수·단축키), `ARCHITECTURE.md`.

---

# Work order: internal rendering resolution scale (issue #80)

Design: [20261017-i080](../design/20261017-i080-render-scale.md)

1. `d3d8_gl_targets`: targets' physical size (guest × scale), remaking targets whose scale differs (copying color contents), `RenderScale()`, `SetRenderScale()`, applied after a present; the sizes of sampling copies, present, reads and dumps.
2. `d3d8_device`, `d3d8_draw`: the scale on GL viewports and the `Clear` scissor.
3. Platform: `ConsumeRenderScaleRequest()` (Ctrl+1 to 8) in `include/rexbox/platform/display.h`.
4. The `REXBOX_RENDER_SCALE` environment variable.
5. Verification: builds, regression tools, scale 1 demo BMP comparison, scale 2/4/8 demo dumps.
6. Documents: work log, the guide (environment variable, hotkey), `ARCHITECTURE.md`.
