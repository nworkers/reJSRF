# 작업 지시: 정점 셰이더 프로그램과 레지스터 컴바이너 픽셀 셰이더 (issue #37)

설계: [20261013-i037](../design/20261013-i037-vertex-shader-programs.md)

1. 오프닝에서 건너뛰는 그리기를 분류해 잰다(임시 계측, 커밋하지 않음). `REXBOX_SHADER_DUMP`로 게임의 정점·픽셀 셰이더를 덤프한다.
2. `d3d8_vsh`: 마이크로코드 디코드·디스어셈블·GLSL 번역, 선언 해석, 뷰포트 상수.
3. `d3d8_shader.h`: 생성 시 마이크로코드 복사와 해석, 상수 접근, `ApplyViewport`에서 `c[58]`/`c[59]` 쓰기.
4. `d3d8_psh`: `D3DPIXELSHADERDEF`(0xF0) → GLSL, 스테이지 상수 매핑.
5. GL 프로그램 캐시(`GetVertexProgram`, `GetVertexPixelProgram`)와 그리기 경로(속성, 상수, 텍스처).
6. `rexbox_d3d8_probe` 검사, 회귀 도구, Debug/Release 빌드, 오프닝 프레임 덤프 비교.
7. 문서: kb(§12 정점 셰이더, §13 픽셀 셰이더), analysis(§10), boot-frontier, `ARCHITECTURE.md`, 가이드, 외부 참고, 작업 로그.

---

# Work order: vertex-shader programs and register-combiner pixel shaders (issue #37)

Design: [20261013-i037](../design/20261013-i037-vertex-shader-programs.md)

(1) Measure which draws the opening skips, by kind (temporary instrumentation, not committed), and dump the game's vertex and pixel shaders with `REXBOX_SHADER_DUMP`. (2) `d3d8_vsh`: microcode decoding, disassembly and GLSL translation, declaration parsing, viewport constants. (3) `d3d8_shader.h`: copy and parse the microcode at creation, constant access, `c[58]`/`c[59]` written by `ApplyViewport`. (4) `d3d8_psh`: `D3DPIXELSHADERDEF` (0xF0) to GLSL, stage constant mapping. (5) The GL program cache (`GetVertexProgram`, `GetVertexPixelProgram`) and the draw path (attributes, constants, textures). (6) `rexbox_d3d8_probe` checks, regression tools, Debug and Release builds, opening frame dumps compared. (7) Documents: kb (§12 vertex shaders, §13 pixel shaders), analysis (§10), boot-frontier, `ARCHITECTURE.md`, the guide, external references, the work log.
