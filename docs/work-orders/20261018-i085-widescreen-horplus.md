# 작업 지시: Hor+ 와이드스크린 (issue #85)

설계: [20261018-i085](../design/20261018-i085-widescreen-horplus.md)

1. `d3d8_display.h/.cpp`: `AspectMode`, `DisplayDefaults.aspect`, `Aspect()`/`SetAspect()`; 창 종횡비를 W×480으로.
2. `d3d8_gl_targets`: `wide` 필드, 백버퍼·깊이 스텐실만 넓힘, GL 너비 도우미, 보류 변경, `RenderTargetWideFactor()`, present·덤프·샘플 복사·MSAA의 너비.
3. 셰이더: `u_wide`(고정 기능 두 분기, 정점 프로그램 에필로그), 프로그램 구조체의 위치, 그리기마다 설정.
4. `d3d8_draw` 뷰포트와 `D3DDevice_Clear` 시저의 x 배율.
5. 설정 파일 `Aspect`(예전 `Widescreen` 호환), Graphics 탭 콤보.
6. 검증: 빌드, 회귀 도구, 4:3 프레임 동일성, 16:9·21:9 프레임의 가운데 영역 비교, 실행 중 전환.
7. 문서: 작업 로그, 가이드, `ARCHITECTURE.md`.

---

# Work order: Hor+ widescreen (issue #85)

Design: [20261018-i085](../design/20261018-i085-widescreen-horplus.md)

1. `d3d8_display.h/.cpp`: `AspectMode`, `DisplayDefaults.aspect`, `Aspect()`/`SetAspect()`; the window's aspect from W×480.
2. `d3d8_gl_targets`: the `wide` field, widening only the back buffer and depth stencil, a GL-width helper, the pending change, `RenderTargetWideFactor()`, widths for present, dumps, the sampling copy and MSAA.
3. Shaders: `u_wide` (both fixed-function branches, the vertex-program epilogue), its location in the program structs, set per draw.
4. The x scale of the `d3d8_draw` viewport and `D3DDevice_Clear` scissors.
5. The `Aspect` setting (reading the old `Widescreen`), the Graphics tab combo.
6. Verification: builds, regression tools, 4:3 frame identity, the middle region of 16:9 and 21:9 frames, switching while running.
7. Documents: work log, guide, `ARCHITECTURE.md`.
