# 작업 지시: 흰 윤곽선 조사용 정점 셰이더 그리기 감시 (issue #70)

설계: [20261016-i070](../design/20261016-i070-outline-watch.md)

1. `src/rexbox/hle/d3d8/d3d8_draw_watch.h/.cpp`: `REXBOX_VSH_WATCH`를 해석하고, 내용 해시가 달라질 때 한 줄을 출력한다(핸들마다 최대 64줄).
2. `d3d8_draw.cpp`: 프로그램 그리기에서 부른다.
3. 검증: 빌드, 회귀 도구, 데모 두 번(변수 있음/없음).
4. 문서: 작업 로그, ARCHITECTURE.md(환경 변수), 가이드(보고 방법), `docs/analysis/`(윤곽선 셰이더).
5. 사용자 보고 뒤: 원인을 정하고 수정 설계를 이 issue에 이어 쓴다.

---

# Work order: watching vertex-shader draws to investigate white outlines (issue #70)

Design: [20261016-i070](../design/20261016-i070-outline-watch.md)

1. `src/rexbox/hle/d3d8/d3d8_draw_watch.h/.cpp`: parse `REXBOX_VSH_WATCH` and print a line when the content hash changes (at most 64 lines per handle).
2. `d3d8_draw.cpp`: call it from the program draw path.
3. Verification: builds, regression tools, two demo runs (with and without the variable).
4. Documents: work log, ARCHITECTURE.md (environment variable), the guide (how to report), `docs/analysis/` (outline shaders).
5. After the user's report: settle the cause and continue with a fix design in this issue.
