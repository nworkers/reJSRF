# 작업 지시: D3D8 그리기 경로의 비용 줄이기 (issue #40)

설계: [20261013-i040](../design/20261013-i040-draw-performance.md)

1. `REXBOX_D3D8_PROFILE=<n>`(`d3d8_profile`): 그리기 단계, present, 나머지의 프레임당 시간과 그리기 수, 업로드 바이트.
2. 기준선 측정: Release, 숨김 창, 오디오 없음, 300 present 단위.
3. GL 상태 캐시: `InvalidateGlStateCache`/`GlStateEpoch`, 프로그램, 프로그램별 유니폼(`UniformCache`), 샘플러 객체와 유닛 바인딩, 고정 상태.
4. 프로그램 키를 구조체 비교와 해시로 바꾼다. trace 문자열은 trace가 켜졌을 때만 만든다.
5. `d3d8_gl_buffers`: 정점 범위·인덱스 캐시(게스트 바이트 비교), 스트리밍 버퍼, 인덱스 없는 그리기는 `glDrawArrays`.
6. 같은 프레임 구간에서 전후를 측정하고, 데모 프레임 덤프를 v0.0.15와 비교한다.
7. Debug/Release 빌드, 회귀 도구.
8. 문서: `ARCHITECTURE.md`(모듈, 환경 변수), 가이드, boot-frontier, d3d8-adapter 분석, 작업 로그.

---

# Work order: cutting the cost of the D3D8 draw path (issue #40)

Design: [20261013-i040](../design/20261013-i040-draw-performance.md)

(1) `REXBOX_D3D8_PROFILE=<n>` (`d3d8_profile`): per-frame time of the draw phases, present and the rest, with draw counts and uploaded bytes. (2) Baseline: Release, hidden window, no audio, windows of 300 presents. (3) GL state cache: `InvalidateGlStateCache`/`GlStateEpoch`, the program, per-program uniforms (`UniformCache`), sampler objects and unit bindings, fixed state. (4) Program keys compared as structs with a hash; trace strings built only while tracing. (5) `d3d8_gl_buffers`: vertex-range and index caches (guest bytes compared), streaming buffers, `glDrawArrays` for draws without indices. (6) Measure before and after on the same frame windows; compare demo frame dumps with v0.0.15. (7) Debug and Release builds, regression tools. (8) Documents: `ARCHITECTURE.md` (modules, environment variables), the guide, boot-frontier, the d3d8-adapter analysis, the work log.
