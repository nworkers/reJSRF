# 작업 로그: 일본판 XBE 빌드 변형 (issue #108)

설계: [20261020-i108](../design/20261020-i108-japanese-variant.md) · 작업 지시: [20261020-i108](../work-orders/20261020-i108-japanese-variant.md) · 분석: [language §10](../analysis/language.md)

## 한 일

1. 측정(`scripts/analysis/language/variant_addresses.py`): 손으로 적은 미국판 함수 주소 225개가 일본판의 같은 주소에 같은 코드로 있다. 디컴파일이 쓰는 자료 주소와 언어 인덱스는 `+0x55834`, `-0x20`, `-0x18`, `0`만큼 옮겨졌다.
2. 범위 변경(사용자 결정): 실행 파일 하나가 아니라 빌드 시점 선택. 판은 파일 이름(`_eng`, `_jpn`)이 아니라 내용으로 판단한다.
3. 스크립트
   * `xbe_variant.py`: `identify_xbe`(MD5로 판 판단).
   * `regenerate.py`: `--xbe`(기본 `default.xbe`)의 판을 MD5로 정하고 `gen/<판>/`에 쓴다. `--variant`는 생략 가능하며, 주면 판단 결과와 같아야 한다.
   * `variant_fingerprints.py`(새 도구): 이름 붙은 함수 225개의 지문(가린 명령 바이트의 SHA-256)을 `function_fingerprints.json`에 쓰고(`--write`, US판만), 다른 판에서 확인한다. `regenerate.py`가 US판이 아닐 때 이 확인을 돌린다.
   * `find_pointer_targets.py`: seed 파일은 US판 기준이라 US판에서만 돈다.
4. `variants.json`에 `jsrf_jp`를 더했다.
5. `include/rejsrf/jsrf_addresses.h`(새 파일): 판별 자료 주소. `mmatrix.c`, `camera_frustum.c`, `jsrf_profile.cpp`가 이 헤더를 쓴다.
6. CMake: `REJSRF_VARIANT`(값 검사, 다른 판으로 다시 configure하면 생성 디렉터리 캐시를 바로잡음), `REJSRF_RECOMP_GEN_DIR` 기본값 `gen/<판>/gen`, `rejsrf_game`에 `REJSRF_VARIANT_JP` 공개 정의.
7. 프로파일
   * `XbeVariant`에 `id`·`tag`를 더했다.
   * `PrimaryVariant()`를 `BuiltVariant()`로 바꿨다.
   * `NativeLanguage()`를 더하고 언어 설정 기본값으로 쓴다.
8. 앱·도구
   * 판이 맞지 않으면 `-DREJSRF_VARIANT=…`를 알리고 종료 코드 1로 멈춘다(`rejsrf_d3d8_check`도 같다).
   * 빌드 식별(창 제목, Info 탭)에 `US`/`JP`를 붙였다.
9. 빌드 스크립트 `-Variant us|jp`. 일본판은 `build\win32_x86_jp`에 빌드한다.
10. CI: Release 작업에 일본판 앱 빌드와 스모크 단계를 더했다.
11. 문서: 설계, 가이드, `ARCHITECTURE.md`, `CODING_STYLE.md`(자료 주소는 헤더로, 지문 갱신), 분석 §10.

## 검증

| 검증 | 결과 |
|---|---|
| 판 판단 | `default.xbe`(US) → `jsrf_us`, `default_jpn.xbe` → `jsrf_jp`, `default.tbn` → 지원하지 않는 파일. 파일 이름과 상관없이 MD5로 정해진다 |
| 지문 | US판으로 225개 작성, 일본판 225/225 일치, US판 자기 확인 225/225. 일본판으로 `--write`하면 거부 |
| 일본판 재생성 | 지문 확인 통과. 함수 9,526개, ABI 9,481개, HLE 오버라이드 197개, 미해결 스텁 28개로 US판과 같다(스텁 목록 동일). 디스패치 표의 차이는 옮겨진 함수 몇 곳(`0x694C0`/`0x69690`, `0xD43A0`/`0xD4350` …)뿐 |
| 미국판 Release 빌드 | 경고 0. probe 56/48/163 실패 0, `rejsrf_decomp_check` 129 차이 0, `rejsrf_d3d8_check` 14,018 차이 0 |
| 일본판 Release 빌드(`build\win32_x86_jp`) | 경고 0. probe 56/48/163 실패 0, `rejsrf_decomp_check` 129 차이 0, `rejsrf_d3d8_check`(일본판 XBE) 14,018 차이 0 |
| 판 불일치 | 미국판 빌드 + 일본판 XBE, 일본판 빌드 + 미국판 XBE 모두 안내와 종료 코드 1. `rejsrf_d3d8_check`도 거부 |
| 미국판 기본 실행 | 파일 열기 127개, 프레임 300~1800이 이전 빌드와 같다 |
| 일본판 기본 실행 | 언어 Japanese(원래 언어). 파일 열기와 프레임 300~1800이 미국판 + `REJSRF_LANGUAGE=Japanese`와 바이트 단위로 같다 |
| 일본판 + `REJSRF_LANGUAGE=English` | 파일 열기와 프레임 300~2100이 미국판 English 실행과 같다 |
| 첫 미국판 비교(재생성과 동시 실행) | 프레임 1500·1800이 없었다. 부하로 40초 안에 그 프레임에 닿지 못한 것이며, 부하 없는 재실행에서는 같았다 |
| CI 일본판 단계 | PR에서 확인 |

## 남은 것

* 일본판 빌드에서 Press Start 이후 화면(사용자 확인).

---

# Work log: the Japanese XBE build variant (issue #108)

Design: [20261020-i108](../design/20261020-i108-japanese-variant.md) · Work order: [20261020-i108](../work-orders/20261020-i108-japanese-variant.md) · Analysis: [language §10](../analysis/language.md)

## Done

1. Measurement (`scripts/analysis/language/variant_addresses.py`): all 225 hand-written US function addresses hold the same code at the same address in the Japanese build; the decomp's data addresses and the language index moved by `+0x55834`, `-0x20`, `-0x18` and `0`.
2. Scope change (the user's decisions): chosen at build time, not one executable; the build is told from content, never from file names (`_eng`, `_jpn`).
3. Scripts
   * `xbe_variant.py`: `identify_xbe` (the build by MD5).
   * `regenerate.py`: picks the build of `--xbe` (default `default.xbe`) by MD5 and writes `gen/<build>/`; `--variant` is optional and must agree when given.
   * `variant_fingerprints.py` (new): writes fingerprints (SHA-256 of the masked instruction bytes) of the 225 named functions to `function_fingerprints.json` (`--write`, US build only) and checks another build against them; `regenerate.py` runs the check for builds other than the US one.
   * `find_pointer_targets.py`: the seed file is kept against the US build, so it runs only there.
4. `variants.json` gained `jsrf_jp`.
5. `include/rejsrf/jsrf_addresses.h` (new): per-build data addresses, used by `mmatrix.c`, `camera_frustum.c` and `jsrf_profile.cpp`.
6. CMake: `REJSRF_VARIANT` (validated; reconfiguring a tree for the other build fixes the cached generated directory), `REJSRF_RECOMP_GEN_DIR` defaulting to `gen/<build>/gen`, the public definition `REJSRF_VARIANT_JP` on `rejsrf_game`.
7. Profile
   * `XbeVariant` gained `id` and `tag`.
   * `PrimaryVariant()` became `BuiltVariant()`.
   * `NativeLanguage()` was added and is the language setting's default.
8. App and tools
   * A mismatched build names `-DREJSRF_VARIANT=…` and exits with 1 (`rejsrf_d3d8_check` refuses too).
   * The build identity (window title, Info tab) carries `US`/`JP`.
9. Build script `-Variant us|jp`; the Japanese build goes to `build\win32_x86_jp`.
10. CI: a Japanese app build and smoke step in the Release job.
11. Documents: design, guide, `ARCHITECTURE.md`, `CODING_STYLE.md` (data addresses through the header, refreshing the fingerprints), analysis §10.

## Verification

| Check | Result |
|---|---|
| Build identification | `default.xbe` (US) → `jsrf_us`, `default_jpn.xbe` → `jsrf_jp`, `default.tbn` → unsupported; decided by MD5 whatever the file name |
| Fingerprints | 225 written from the US build; Japanese 225/225 match, US self-check 225/225; `--write` on the Japanese build refused |
| Japanese regeneration | fingerprint check passed; 9,526 functions, 9,481 ABI entries, 197 HLE overrides, 28 unresolved stubs, as for the US build (same stub list); the dispatch tables differ only at a few moved functions (`0x694C0`/`0x69690`, `0xD43A0`/`0xD4350` ...) |
| US Release build | zero warnings; probes 56/48/163 with 0 failures, `rejsrf_decomp_check` 129 with 0 differences, `rejsrf_d3d8_check` 14,018 with 0 differences |
| Japanese Release build (`build\win32_x86_jp`) | zero warnings; probes 56/48/163 with 0 failures, `rejsrf_decomp_check` 129 with 0 differences, `rejsrf_d3d8_check` (Japanese XBE) 14,018 with 0 differences |
| Build mismatch | US build + Japanese XBE and Japanese build + US XBE both explain and exit with 1; `rejsrf_d3d8_check` refuses too |
| US default run | 127 file opens and frames 300-1800 equal the previous build |
| Japanese default run | language Japanese (native); file opens and frames 300-1800 equal the US build with `REJSRF_LANGUAGE=Japanese` byte for byte |
| Japanese + `REJSRF_LANGUAGE=English` | file opens and frames 300-2100 equal the US English run |
| First US comparison (run during regeneration) | frames 1500 and 1800 were missing: under load the run did not reach them in 40 s; the unloaded rerun matched |
| CI Japanese step | checked on the pull request |

## Open

* Screens past Press Start in the Japanese build (user check).
