# 작업 로그: lifter 파이프라인을 reJSRF로 (issue #117)

설계: [20261024-i117](../design/20261024-i117-lifter-pipeline.md) · 작업 지시: [20261024-i117](../work-orders/20261024-i117-lifter-pipeline.md)

## 한 일

1. 기준: 변경 전 코드로 미국판·일본판을 생성하고 검사 스크립트 출력을 저장했다(같은 xboxrecomp, 같은 XBE).
2. `third_party/rexbox`를 rexbox issue #26 브랜치(`bac8e80`)로 올렸다.
3. `scripts/recomp/lifter.py`(새 파일)
   * xboxrecomp 다섯 단계(parse, disasm, func_id, abi, lift)를 `third_party/xboxrecomp`(fork `rejsrf`)로 실행한다.
   * 출력 읽기: `disasm/functions.json` → 함수 시작, `/* ret N */` → `lifted_rets`, `recomp_dispatch.c` → `lifted_addresses`.
   * 이름: `entry_name` `sub_%08X`, `lifted_name` `sub_%08X_gen`.
   * rexbox의 `hle_overrides`, `xbe_variant`를 import한다.
4. `regenerate.py`는 `lifter.run`을 부른다. `check_hle_pops.py`와 `find_pointer_targets.py`는 `lifter.py`와 rexbox의 `hle_pops`, `pointer_targets`, `xbe_variant`를 쓴다.
5. CMake: `REXBOX_XBOXRECOMP_DIR`를 지웠다(`third_party/xboxrecomp` 존재 확인은 경로로 남김). `CMakeLists.txt`와 `tools/CMakeLists.txt`의 `rexbox_check_license` 호출을 지웠다.
6. 라이선스 가드(결정): reJSRF에 가드 코드를 두지 않는다. LGPL 파일을 어떤 소스 목록에도 넣지 않는다는 규칙을 `AGENTS.md`에 적었다.
7. 문서: `AGENTS.md`, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`, `docs/kb/external-references.md`.

## 검증

| 검증 | 결과 |
|---|---|
| 재생성(미국판, 일본판) | 판마다 66개 파일 중 61개가 기준과 바이트 단위로 같다. 다른 5개는 예상한 차이뿐이다: `hle_overrides.c`와 그 사본 `gen/rexbox_hle_overrides.c`의 머리 주석 두 줄, `overrides.c`의 머리와 그 안의 경로, `recomp-summary/summary.json`의 출력 루트 경로, `disasm/.disasm_cache.json`의 시각·소요 시간. lift된 C는 바이트 단위로 같다 |
| `check_hle_pops.py --all`(미국판, 일본판) | 기준과 출력이 같다. 불일치 0 |
| `find_pointer_targets.py`(미국판) | 기준과 출력이 같다. 후보 0 |
| Win32 x86 Release 빌드 | `recomp_abi.c`가 `rejsrf_recomp_abi`에서 fork의 헤더로 경고 없이 컴파일된다. `rejsrf.exe`, `rejsrf_decomp_check`, `rejsrf_d3d8_check` 링크 |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 129 / 14,018 케이스, 차이 0 |
| 실행(`rejsrf.exe`, 숨은 창, `REXBOX_WATCHDOG=40`) | 부팅(프레임 2000 덤프에 경고 화면), 프로그램 그리기와 함께 프레임 루프가 계속 돌고 watchdog로 끝남(exit 4). 멈춤·fault 없음 |
| 게임플레이 | 확인하지 않음(입력을 넣는 스크립트 없음) |

## 남은 것

* 사용자 재생성은 필요 없다(생성 코드와 ABI가 같다).
* rexbox #26 머지·태그 뒤 `third_party/rexbox`를 태그 커밋으로 맞추고 버전 표기(`ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`)를 고친다.
* CI: PR에서 확인.

---

# Work log: the lifter pipeline moves to reJSRF (issue #117)

Design: [20261024-i117](../design/20261024-i117-lifter-pipeline.md) · Work order: [20261024-i117](../work-orders/20261024-i117-lifter-pipeline.md)

## Done

1. Baseline: the US and Japanese variants generated with the code before the change, and the check scripts' output saved (same xboxrecomp, same XBEs).
2. `third_party/rexbox` moved to the rexbox issue #26 branch (`bac8e80`).
3. `scripts/recomp/lifter.py` (new)
   * runs xboxrecomp's five steps (parse, disasm, func_id, abi, lift) from `third_party/xboxrecomp` (fork `rejsrf`).
   * reads the output: `disasm/functions.json` → function starts, `/* ret N */` → `lifted_rets`, `recomp_dispatch.c` → `lifted_addresses`.
   * names: `entry_name` `sub_%08X`, `lifted_name` `sub_%08X_gen`.
   * imports rexbox's `hle_overrides` and `xbe_variant`.
4. `regenerate.py` calls `lifter.run`; `check_hle_pops.py` and `find_pointer_targets.py` use `lifter.py` with rexbox's `hle_pops`, `pointer_targets` and `xbe_variant`.
5. CMake: `REXBOX_XBOXRECOMP_DIR` removed (the `third_party/xboxrecomp` existence check stays, by path); the `rexbox_check_license` calls removed from `CMakeLists.txt` and `tools/CMakeLists.txt`.
6. License guard (decision): no guard code in reJSRF; `AGENTS.md` states the rule that the LGPL files never go into a source list.
7. Documents: `AGENTS.md`, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`, `docs/kb/external-references.md`.

## Verification

| Check | Result |
|---|---|
| Regeneration (US, Japanese) | per build, 61 of 66 files equal the baseline byte for byte; the other 5 differ only as expected: the two header comment lines of `hle_overrides.c` and its copy `gen/rexbox_hle_overrides.c`, the header of `overrides.c` and the paths in it, the output-root paths in `recomp-summary/summary.json`, the time and elapsed fields of `disasm/.disasm_cache.json`. The lifted C is byte-identical |
| `check_hle_pops.py --all` (US, Japanese) | output equals the baseline; 0 mismatches |
| `find_pointer_targets.py` (US) | output equals the baseline; 0 candidates |
| Win32 x86 Release build | `recomp_abi.c` compiles in `rejsrf_recomp_abi` against the fork's header with no warnings; `rejsrf.exe`, `rejsrf_decomp_check` and `rejsrf_d3d8_check` link |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 129 / 14,018 cases, 0 differences |
| Run (`rejsrf.exe`, hidden window, `REXBOX_WATCHDOG=40`) | boots (the warning screen in the frame-2000 dump); the frame loop keeps running with program draws until the watchdog ends it (exit 4); no stop or fault |
| Gameplay | not checked (no input-driving script) |

## Open

* Users need not regenerate (the generated code and the ABI are unchanged).
* After rexbox #26 is merged and tagged, move `third_party/rexbox` to the tag commit and fix the version notes (`ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`).
* CI: checked on the pull request.
