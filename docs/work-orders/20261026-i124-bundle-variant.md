# 작업 지시: 한국 정발 번들 SegaJSRF.xbe 지원 (issue #124)

상태: **준비됨**(사용자 승인 대기). 설계: [20261026-i124](../design/20261026-i124-bundle-variant.md)

## 1. 목적

한국에서 판매된 "Sega GT 2002 + JSRF - Jet Set Radio Future" 번들 디스크의 `SegaJSRF.xbe`를 새 빌드 변형(`jsrf_bundle_kor`)으로 추가하고 실행할 수 있도록 한다.

## 2. 작업 단계

### 단계 1: 분석 및 변형 등록
* `src/game/jsrf/recomp/variants.json`에 `jsrf_bundle_kor` (MD5 `8bf3c450e9c1cc39730d4025ef987e2c`) 추가.
* `scripts/recomp/variant_fingerprints.py`에 `jsrf_bundle_kor` 전용 주소 변환 규칙(`0x351cd` 이전 0, `.text` 내부 -0x30, XDK 섹션 -0x40) 반영 및 검증.

### 단계 2: 주소 매핑 및 리프팅 지원
* `scripts/recomp/regenerate.py` 및 `lifter.py`가 `jsrf_bundle_kor` 변형에 대해 seed 주소와 extra symbols를 적절히 매핑하여 lift할 수 있도록 연동.
* `include/rejsrf/jsrf_addresses.h`에 `REJSRF_VARIANT_BUNDLE_KOR` 매크로 분기 추가:
  * `JSRF_LANGUAGE_INDEX`: `0x001FC6E4`
  * `JSRF_MMATRIX_WORK_VECTOR` 및 기타 데이터 주소: `-0x40` 반영.

### 단계 3: 타이틀 프로파일 및 앱 로딩 지원
* `src/game/jsrf/jsrf_profile.cpp`:
  * `kVariants`에 `jsrf_bundle_kor` 인증서 정보 추가.
  * 기본 언어: English.
* `src/app/main.cpp`:
  * 폴더를 인자로 주었을 때 `default.xbe` 외에 `SegaJSRF.xbe`도 탐색하여 실행할 수 있도록 보완.
* CMake 및 빌드 스크립트(`scripts/build_win32_x86.ps1` 등):
  * `REJSRF_VARIANT` 옵션 값에 `jsrf_bundle_kor` 허용.

### 단계 4: 코드 생성 및 빌드 검증
* `python scripts/recomp/regenerate.py --xbe "roms/Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)/SegaJSRF.xbe"` 실행하여 C 코드 생성.
* `-DREJSRF_VARIANT=jsrf_bundle_kor`로 CMake 빌드 수행.
* 실행 테스트: 부팅, 사운드, 그래픽, 영어 텍스트 출력 검증.

---

# Work Order: Support SegaJSRF.xbe from Korean Bundle (issue #124)

Status: **Ready** (awaiting user approval). Design: [20261026-i124](../design/20261026-i124-bundle-variant.md)

## 1. Goal

Add and support `SegaJSRF.xbe` from the Korean release of "Sega GT 2002 + JSRF - Jet Set Radio Future" bundle disc as a new build variant (`jsrf_bundle_kor`).

## 2. Steps

### Step 1: Analysis and Variant Registration
* Add `jsrf_bundle_kor` (MD5 `8bf3c450e9c1cc39730d4025ef987e2c`) to `src/game/jsrf/recomp/variants.json`.
* Update `scripts/recomp/variant_fingerprints.py` with the address translation rules for `jsrf_bundle_kor` (no shift before `0x351cd`, `-0x30` inside `.text`, `-0x40` in XDK sections) and verify fingerprints pass.

### Step 2: Address Mapping and Lifter Support
* Update `scripts/recomp/regenerate.py` and `lifter.py` to map seed addresses and extra symbols for `jsrf_bundle_kor`.
* Add `REJSRF_VARIANT_BUNDLE_KOR` definitions in `include/rejsrf/jsrf_addresses.h`:
  * `JSRF_LANGUAGE_INDEX`: `0x001FC6E4`
  * `JSRF_MMATRIX_WORK_VECTOR` and other data addresses shifted by `-0x40`.

### Step 3: Title Profile and App Loading
* Update `src/game/jsrf/jsrf_profile.cpp`:
  * Add `jsrf_bundle_kor` certificate entry to `kVariants`.
  * Set native language to English.
* Update `src/app/main.cpp`:
  * Support detecting `SegaJSRF.xbe` alongside `default.xbe` when passing a directory.
* Update CMake and build scripts to allow `jsrf_bundle_kor` variant.

### Step 4: Code Generation and Build Verification
* Run `regenerate.py` to lift C code from `SegaJSRF.xbe`.
* Build with `-DREJSRF_VARIANT=jsrf_bundle_kor`.
* Verify boot, rendering, audio, and English text output.
