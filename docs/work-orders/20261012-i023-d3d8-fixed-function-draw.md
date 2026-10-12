# 작업 지시: D3D8 HLE 세 번째 묶음 — 고정 기능 그리기 (issue #23)

설계: [20261012-i023](../design/20261012-i023-d3d8-fixed-function-draw.md)

1. **라이브러리 전역**: `d3d8_globals`. `rexbox_xdk_variables[]`에서 `D3D_g_RenderState`, `D3D_g_DeferredTextureState`, `D3D_g_Stream`, `D3D_g_pDevice`를 찾고, 원본 setter lift 본문에서 `D3D__IndexData`·dirty 플래그의 상대 위치를 확인해 상수로 둔다. `d3d8_state.cpp`의 setter가 원본과 같은 전역 칸을 쓰도록 바꾸고 `PipelineState`의 렌더·텍스처 상태 사본을 없앤다.
2. **번호표**: `d3d8_render_state`. 라이브러리의 simple 상태 method 표를 lift 본문에서 찾아 확인하고, Xbox `D3DRS`·`D3DTSS` 번호표를 만든다(근거별 확인됨/추정). `docs/kb/xbox-d3d8-api.md`에 누적.
3. **`rejsrf_d3d8_check`**: setter의 전역 칸 비교 케이스(`SetIndices`, 렌더 상태 setter, `SetTextureState_*`).
4. **텍스처 포맷 측정**: 부팅에서 게임이 만드는 텍스처 포맷·차원 분포를 `REXBOX_D3D8_TRACE`로 세어 구현 범위를 확정한다.
5. **순수 함수**: `d3d8_fvf`(FVF 배치), `d3d8_texture_format`(스위즐 해제, 팔레트, DXT1/3/5, 16비트 포맷 → RGBA8), `d3d8_ff_shader`(고정 기능 스테이지 → GLSL, XYZRHW·XYZ 정점 셰이더).
6. **`rexbox_d3d8_probe`**(게임 데이터 불필요): 설계 §7 항목. CI 워크플로에 추가.
7. **GL 백엔드**: `d3d8_gl` 진입점 확장(셰이더·프로그램·버퍼·VAO·텍스처·샘플러·FBO·blit·상태), `d3d8_gl_buffers`, `d3d8_gl_textures`(캐시, Lock 후 재업로드, 해시), `d3d8_gl_programs`(키 캐시), `d3d8_gl_targets`(백버퍼·깊이 FBO, `SetRenderTarget`, `Swap` blit, frame dump).
8. **그리기**: `d3d8_draw`의 `DrawVertices`, `DrawIndexedVertices`, `DrawVerticesUP`, `DrawIndexedVerticesUP`, `Begin`/`End`/`SetVertexData*`를 파이프라인으로 연결. 고정 상태 적용(설계 §3.6). 프로그래머블 셰이더·조명은 한 번 기록하고 건너뜀.
9. **수직 동기**: `d3d8_vblank` 가상 60 Hz 시계, `BlockUntilVerticalBlank`·`Swap`·`GetRasterStatus` 연결. `SetVerticalBlankCallback` 콜백의 lift 본문 확인(실행은 범위 밖).
10. **표시 계층**: `REXBOX_DISPLAY=hidden`(보이지 않는 창 + GL 컨텍스트).
11. **검증**: Debug/Release 빌드, 회귀 도구, `check_hle_pops.py`, `REXBOX_DISPLAY=hidden` + `REXBOX_FRAME_DUMP` 부팅, 창 실행 관찰. 다음 frontier 기록.
12. **문서**: `docs/analysis/d3d8-adapter.md`(라이브러리 전역·번호 확인 결과), `docs/kb/xbox-d3d8-api.md`(번호표·포맷·FVF·프리미티브), `ARCHITECTURE.md`, `docs/analysis/boot-frontier.md`, 가이드(새 환경 변수와 기대 화면), `THIRD_PARTY_NOTICES.md`(가져온 코드가 있으면), 작업 로그.

순서: 1–3(상태 정합성, 그리기 없이 검증 가능) → 4–6(순수 함수, probe) → 7–8(GL 그리기) → 9–10 → 11–12. 1–3, 4–6, 7–10을 각각 하나의 커밋 단위로 남긴다.

---

# Work order: D3D8 HLE, third batch — fixed-function drawing (issue #23)

Design: [20261012-i023](../design/20261012-i023-d3d8-fixed-function-draw.md)

1. **Library globals**: `d3d8_globals`. Look up `D3D_g_RenderState`, `D3D_g_DeferredTextureState`, `D3D_g_Stream` and `D3D_g_pDevice` in `rexbox_xdk_variables[]`. Confirm the relative locations of `D3D__IndexData` and the dirty flags from the lifted original setters and keep them as constants. Make the setters in `d3d8_state.cpp` write the same global slots as the originals, and remove the render- and texture-state copies from `PipelineState`.
2. **Numbering**: `d3d8_render_state`. Find and confirm the library's simple-state method table in the lifted bodies, and build the Xbox `D3DRS` and `D3DTSS` maps (confirmed/inferred per source). Accumulate them in `docs/kb/xbox-d3d8-api.md`.
3. **`rejsrf_d3d8_check`**: global-slot comparison cases for the setters (`SetIndices`, the render-state setters, `SetTextureState_*`).
4. **Texture format census**: count the texture formats and dimensions the game creates during boot with `REXBOX_D3D8_TRACE`, which fixes the implementation scope.
5. **Pure functions**: `d3d8_fvf` (FVF layouts), `d3d8_texture_format` (unswizzling, palettes, DXT1/3/5, 16-bit formats → RGBA8) and `d3d8_ff_shader` (fixed-function stages → GLSL, plus the XYZRHW and XYZ vertex shaders).
6. **`rexbox_d3d8_probe`** (no game data): the items of design §7, added to the CI workflow.
7. **GL backend**: extend the `d3d8_gl` entry points (shaders, programs, buffers, VAOs, textures, samplers, FBOs, blit, state); `d3d8_gl_buffers`; `d3d8_gl_textures` (cache, re-upload after a lock, hash); `d3d8_gl_programs` (keyed cache); `d3d8_gl_targets` (back-buffer and depth FBOs, `SetRenderTarget`, the `Swap` blit, frame dumps).
8. **Drawing**: connect `DrawVertices`, `DrawIndexedVertices`, `DrawVerticesUP`, `DrawIndexedVerticesUP` and `Begin`/`End`/`SetVertexData*` in `d3d8_draw` to the pipeline, and apply fixed state (design §3.6). Programmable shaders and lighting are logged once and skipped.
9. **Vertical blank**: the virtual 60 Hz clock in `d3d8_vblank`, wired into `BlockUntilVerticalBlank`, `Swap` and `GetRasterStatus`. Check the lifted body of the `SetVerticalBlankCallback` callback (running it is out of scope).
10. **Display layer**: `REXBOX_DISPLAY=hidden` (an invisible window with a GL context).
11. **Verification**: Debug/Release builds, the regression tools, `check_hle_pops.py`, a boot with `REXBOX_DISPLAY=hidden` + `REXBOX_FRAME_DUMP`, and an observed windowed run. Record the next frontier.
12. **Documents**: `docs/analysis/d3d8-adapter.md` (library globals, numbering results), `docs/kb/xbox-d3d8-api.md` (numbering, formats, FVF, primitives), `ARCHITECTURE.md`, `docs/analysis/boot-frontier.md`, the guide (new environment variables and the expected screen), `THIRD_PARTY_NOTICES.md` (if code is taken), and the work log.

Order: 1–3 (state consistency, verifiable without drawing) → 4–6 (pure functions, probe) → 7–8 (GL drawing) → 9–10 → 11–12. Steps 1–3, 4–6 and 7–10 each become one commit unit.
