# 작업 지시: lifter 파이프라인을 reJSRF로 (issue #117)

설계: [20261024-i117](../design/20261024-i117-lifter-pipeline.md)

1. 변경 전 기준을 만든다. 같은 xboxrecomp와 XBE로 미국판·일본판을 생성하고, `check_hle_pops.py --all`(두 판)과 `find_pointer_targets.py`(미국판) 출력을 저장한다.
2. `third_party/rexbox`를 rexbox issue #26 브랜치로 올린다.
3. `scripts/recomp/lifter.py`를 만든다. rexbox `regenerate.py`의 xboxrecomp 실행, 출력 읽기, 이름 규칙을 옮기고 rexbox의 `hle_overrides`, `xbe_variant`를 import한다.
4. `regenerate.py`는 `lifter.run`을, `check_hle_pops.py`와 `find_pointer_targets.py`는 `lifter.py`와 rexbox의 `hle_pops`, `pointer_targets`, `xbe_variant`를 쓰게 한다.
5. CMake: `REXBOX_XBOXRECOMP_DIR`를 지우고(체크아웃 존재 확인은 경로로 유지), `CMakeLists.txt`와 `tools/CMakeLists.txt`의 `rexbox_check_license` 호출을 지운다.
6. 미국판·일본판을 다시 생성해 1의 기준과 비교한다. 검사 스크립트 출력도 비교한다.
7. Win32 x86 Release 빌드. `rejsrf_recomp_abi` 경고, 링크, `rejsrf_decomp_check`, `rejsrf_d3d8_check`, 실행(`REXBOX_WATCHDOG`)을 확인한다.
8. 문서: 작업 로그, `AGENTS.md` 라이선스 규칙, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`, `docs/kb/external-references.md`.

---

# Work order: the lifter pipeline moves to reJSRF (issue #117)

Design: [20261024-i117](../design/20261024-i117-lifter-pipeline.md)

1. Make a baseline before the change: generate the US and Japanese variants with the same xboxrecomp and XBEs, and save the output of `check_hle_pops.py --all` (both builds) and `find_pointer_targets.py` (US).
2. Move `third_party/rexbox` to the rexbox issue #26 branch.
3. Write `scripts/recomp/lifter.py`: move the xboxrecomp runs, output readers and naming out of rexbox's `regenerate.py`; import rexbox's `hle_overrides` and `xbe_variant`.
4. Make `regenerate.py` call `lifter.run`, and `check_hle_pops.py` and `find_pointer_targets.py` use `lifter.py` with rexbox's `hle_pops`, `pointer_targets` and `xbe_variant`.
5. CMake: drop `REXBOX_XBOXRECOMP_DIR` (keep the checkout's existence check, by path) and the `rexbox_check_license` calls in `CMakeLists.txt` and `tools/CMakeLists.txt`.
6. Regenerate the US and Japanese variants and compare with the baseline from 1; compare the check scripts' output too.
7. Win32 x86 Release build: `rejsrf_recomp_abi` warnings, links, `rejsrf_decomp_check`, `rejsrf_d3d8_check`, a run (`REXBOX_WATCHDOG`).
8. Documents: work log, `AGENTS.md` license rule, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`, `docs/kb/external-references.md`.
