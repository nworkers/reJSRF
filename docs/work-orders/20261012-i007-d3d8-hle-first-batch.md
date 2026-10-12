# 작업 지시: D3D8 HLE 첫 묶음 (issue #7)

설계: [20261012-i007](../design/20261012-i007-d3d8-hle-first-batch.md)

1. submodule `third_party/SDL`(release-3.4.18, zlib) 추가. 루트 CMake에서 정적 라이브러리만, 불필요한 하위 시스템 끔. `THIRD_PARTY_NOTICES.md`, `docs/kb/external-references.md` 갱신.
2. 플랫폼 계약 `include/rexbox/platform/display.h`와 구현 `src/platform/sdl/display_sdl.cpp`. `src/platform/CMakeLists.txt`가 OS별 소스 목록에 넣고 SDL3를 링크. AGENTS.md·CODING_STYLE.md에 `src/platform/<library>/` 규칙 추가.
3. D3D8 HLE 모듈 분할: `d3d8_internal.h`, `d3d8_adapter.cpp`, `d3d8_caps.cpp`, `d3d8_device.cpp`, `d3d8_gl.h/.cpp`, `d3d8_hle.cpp`(등록). export 8개 + `Direct3D_CreateDevice`.
4. 도구 `rejsrf_d3d8_check`(`--dump`, 비교 모드). 먼저 `--dump`로 원본의 모드 표·Caps·포맷 판정을 얻어 HLE 상수 표를 쓰고, 비교 모드로 확인.
5. 빌드(Debug/Release), `rexbox_kernel_probe`, `rejsrf_decomp_check`, `rejsrf_d3d8_check`, 부팅. 다음 frontier 기록.
6. 문서: `ARCHITECTURE.md`, `docs/kb/xbox-d3d8-api.md`(+색인), `docs/analysis/boot-frontier.md`, `docs/analysis/xdk-symbols.md`(+색인), 가이드, README 플랫폼 상태, 작업 로그.

---

# Work order: D3D8 HLE, first batch (issue #7)

Design: [20261012-i007](../design/20261012-i007-d3d8-hle-first-batch.md)

(1) Add the `third_party/SDL` submodule (release-3.4.18, zlib); build the static library only from the root CMake with unneeded subsystems off; update `THIRD_PARTY_NOTICES.md` and `docs/kb/external-references.md`. (2) Platform contract `include/rexbox/platform/display.h` and implementation `src/platform/sdl/display_sdl.cpp`; `src/platform/CMakeLists.txt` adds it to the per-OS source list and links SDL3; add the `src/platform/<library>/` rule to AGENTS.md and CODING_STYLE.md. (3) Split the D3D8 HLE module: `d3d8_internal.h`, `d3d8_adapter.cpp`, `d3d8_caps.cpp`, `d3d8_device.cpp`, `d3d8_gl.h/.cpp`, `d3d8_hle.cpp` (registration); the eight exports plus `Direct3D_CreateDevice`. (4) Tool `rejsrf_d3d8_check` (`--dump` and comparison mode): first obtain the original's mode table, caps and format verdicts with `--dump`, author the HLE constant tables, then confirm in comparison mode. (5) Build Debug/Release, run `rexbox_kernel_probe`, `rejsrf_decomp_check`, `rejsrf_d3d8_check` and boot; record the next frontier. (6) Documents: `ARCHITECTURE.md`, `docs/kb/xbox-d3d8-api.md` (and index), `docs/analysis/boot-frontier.md`, `docs/analysis/xdk-symbols.md` (and index), the guide, README platform status, the work log.
