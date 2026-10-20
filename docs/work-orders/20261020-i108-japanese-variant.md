# 작업 지시: 일본판 XBE 빌드 변형 (issue #108)

설계: [20261020-i108](../design/20261020-i108-japanese-variant.md)

1. 재생성: `variants.json`에 `jsrf_jp`. `variant_addresses.py`의 함수 비교를 `scripts/recomp/` 모듈로 옮기고, 미국판에서 만든 지문을 `src/game/jsrf/recomp/function_fingerprints.json`에 커밋(해시만). `regenerate.py`가 미국판이 아닌 변형에서 지문을 확인한다.
2. 자료 주소: `include/rejsrf/jsrf_addresses.h`. `mmatrix.c`, `camera_frustum.c`, `jsrf_profile.cpp`가 쓴다.
3. CMake: `REJSRF_VARIANT`(검사 포함), 생성 디렉터리 기본값, `REJSRF_VARIANT_JP` 정의. 빌드 스크립트 `-Variant jp`와 `build/win32_x86_jp`.
4. 프로파일과 앱: `BuiltVariant()`, 변형별 언어 인덱스와 `NativeLanguage()`, 불일치 안내(앱, `rejsrf_d3d8_check`), 빌드 식별에 변형 표시.
5. CI: 일본판 configure와 생성 코드 없는 빌드.
6. 검증(설계 §6): 미국판 회귀, 일본판 재생성·빌드·회귀 도구·데모 비교, 불일치 안내.
7. 문서: 작업 로그, 가이드(일본판 빌드 방법), `ARCHITECTURE.md`, `CODING_STYLE.md`(자료 주소는 헤더로), `docs/analysis/language.md`.

---

# Work order: the Japanese XBE build variant (issue #108)

Design: [20261020-i108](../design/20261020-i108-japanese-variant.md)

1. Regeneration: `jsrf_jp` in `variants.json`. Move `variant_addresses.py`'s function comparison into a `scripts/recomp/` module and commit the fingerprints made from the US build to `src/game/jsrf/recomp/function_fingerprints.json` (hashes only). `regenerate.py` checks them for variants other than the US one.
2. Data addresses: `include/rejsrf/jsrf_addresses.h`, used by `mmatrix.c`, `camera_frustum.c` and `jsrf_profile.cpp`.
3. CMake: `REJSRF_VARIANT` (validated), the generated-directory default, the `REJSRF_VARIANT_JP` definition. Build script `-Variant jp` and `build/win32_x86_jp`.
4. Profile and app: `BuiltVariant()`, the per-variant language index and `NativeLanguage()`, the mismatch message (app, `rejsrf_d3d8_check`), the variant in the build identity.
5. CI: a Japanese configure and a build without generated code.
6. Verification (design §6): US regression; Japanese regeneration, build, regression tools and demo comparison; the mismatch message.
7. Documents: work log, the guide (how to build the Japanese variant), `ARCHITECTURE.md`, `CODING_STYLE.md` (data addresses through the header), `docs/analysis/language.md`.
