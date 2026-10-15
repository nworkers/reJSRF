# 작업 지시: 렌더 타깃 샘플링, 픽셀 셰이더와 고정 기능 정점 처리 (issue #69)

설계: [20261016-i069](../design/20261016-i069-render-target-sampling.md)

1. `d3d8_gl_targets`: `SampleRenderTarget(data, &width, &height)`. 데이터 주소로 타깃을 찾고, 상하 반전 사본을 유지한다. 묶인 적이 있으면 다시 복사한다.
2. `d3d8_gl_textures`: `ResolveTexture`가 먼저 렌더 타깃을 찾는다. 처음 한 번 보고한다.
3. `d3d8_draw`: `BuildKey`의 픽셀 셰이더 모드, 픽셀 셰이더 + 고정 기능 경로, uniform 설정.
4. `d3d8_gl_programs`: `GetFixedFunctionPixelProgram`.
5. 검증: 빌드, 회귀 도구, 데모 실행.
6. 문서: 작업 로그, ARCHITECTURE.md, `docs/analysis/d3d8-adapter.md` 또는 관련 분석.

---

# Work order: render-target sampling; pixel shaders with fixed-function vertex processing (issue #69)

Design: [20261016-i069](../design/20261016-i069-render-target-sampling.md)

1. `d3d8_gl_targets`: `SampleRenderTarget(data, &width, &height)` finds a target by data address and keeps a vertically flipped copy, copied again when the target was bound since.
2. `d3d8_gl_textures`: `ResolveTexture` looks for a render target first and reports the first time.
3. `d3d8_draw`: `BuildKey`'s pixel-shader mode, the pixel shader with fixed-function path, and setting uniforms.
4. `d3d8_gl_programs`: `GetFixedFunctionPixelProgram`.
5. Verification: builds, regression tools, a demo run.
6. Documents: work log, ARCHITECTURE.md, and the related analysis.
