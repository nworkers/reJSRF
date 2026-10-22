# 작업 지시: 공용 런타임 rexbox를 submodule로 쓰기 (issue #102)

설계: [20261022-i102](../design/20261022-i102-rexbox-submodule.md)

1. 기준 측정: 지금 빌드(v0.0.46)로 미국판·일본판 데모의 파일 열기 목록과 프레임 300~1800 BMP를 저장한다(비교 기준).
2. submodule
   * `third_party/rexbox`(reexec/rexbox `b2f2653`, v0.0.9)를 더한다.
   * reJSRF 자체의 SDL, imgui, smaa, XbSymbolDatabase submodule을 지운다.
   * `third_party/xboxrecomp`(fork)는 남긴다.
3. CMake
   * 루트: `REXBOX_XBOXRECOMP_DIR`, `REXBOX_BUILD_TOOLS`, `add_subdirectory(third_party/rexbox)`, `rexbox_apply_host_flags()`
   * `rexbox_add_recompiled_library`
   * 우리 타깃에 `rexbox_check_license`, `rexbox_strict_warnings`
   * `tools/CMakeLists.txt`는 타이틀 도구만 남긴다.
4. 사본 삭제
   * `src/rexbox/`, `src/platform/`, `include/rexbox/`, `cmake/embed_text.cmake`
   * rexbox 도구 다섯 개
   * `scripts/recomp/check_hle_pops.py`, `scripts/perf/`
5. 스크립트
   * `regenerate.py` wrapper: MD5로 판 판단, 미국판이 아니면 지문 확인, rexbox `run(...)` 호출
   * `find_pointer_targets.py` wrapper
   * `rexbox_xdk_symbols` 경로 후보
6. 앱: `SetApplicationIdentity("reJSRF", "reJSRF")`
7. 빌드 스크립트·CI: 출력 경로, `submodules: recursive`, probe 실행 경로
8. 검증(설계 §6)
   * 재생성 결과 비교
   * 미국판·일본판 빌드와 회귀 도구
   * 데모 비교와 차이 원인
   * 설정 폴더
   * CI
9. 문서
   * 작업 로그
   * `ARCHITECTURE.md`(런타임 설명을 rexbox로 넘김)
   * `AGENTS.md` 레이어 경로
   * 가이드
   * `THIRD_PARTY_NOTICES.md`
   * `README.md`

---

# Work order: use the shared runtime rexbox as a submodule (issue #102)

Design: [20261022-i102](../design/20261022-i102-rexbox-submodule.md)

1. Baseline: with today's build (v0.0.46), save the file-open list and the BMPs of frames 300-1800 of the US and Japanese demos (the comparison baseline).
2. Submodules
   * Add `third_party/rexbox` (reexec/rexbox `b2f2653`, v0.0.9).
   * Remove reJSRF's own SDL, imgui, smaa and XbSymbolDatabase submodules.
   * Keep `third_party/xboxrecomp` (the fork).
3. CMake
   * Root: `REXBOX_XBOXRECOMP_DIR`, `REXBOX_BUILD_TOOLS`, `add_subdirectory(third_party/rexbox)`, `rexbox_apply_host_flags()`.
   * `rexbox_add_recompiled_library`.
   * `rexbox_check_license` and `rexbox_strict_warnings` on our targets.
   * `tools/CMakeLists.txt` keeps the title tools only.
4. Delete the copy
   * `src/rexbox/`, `src/platform/`, `include/rexbox/`, `cmake/embed_text.cmake`
   * the five rexbox tools
   * `scripts/recomp/check_hle_pops.py`, `scripts/perf/`
5. Scripts
   * the `regenerate.py` wrapper: the build by MD5, the fingerprint check for builds other than the US one, then rexbox's `run(...)`
   * the `find_pointer_targets.py` wrapper
   * the `rexbox_xdk_symbols` path candidates
6. App: `SetApplicationIdentity("reJSRF", "reJSRF")`.
7. Build script and CI: output paths, `submodules: recursive`, probe paths.
8. Verification (design §6)
   * compare the regenerated code
   * the US and Japanese builds and the regression tools
   * the demo comparison and the cause of any difference
   * the settings folder
   * CI
9. Documents
   * work log
   * `ARCHITECTURE.md` (runtime description handed to rexbox)
   * `AGENTS.md` layer paths
   * guides
   * `THIRD_PARTY_NOTICES.md`
   * `README.md`
