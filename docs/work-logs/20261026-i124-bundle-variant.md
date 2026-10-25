# 작업 로그: 한국 정발 콤보 디스크(SegaJSRF.xbe) 빌드 변형 지원 (issue #124)

설계: [20261026-i124](../design/20261026-i124-bundle-variant.md) · 작업 지시: [20261026-i124](../work-orders/20261026-i124-bundle-variant.md) · 분석: [jsrf-xbe-overview](../analysis/jsrf-xbe-overview.md)

## 한 일

1. **바이너리 분석**:
   * 대상: `roms\Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)\SegaJSRF.xbe`
   * MD5: `8bf3c450e9c1cc39730d4025ef987e2c`, 크기: 2,281,472 바이트 (US판보다 48바이트 작음).
   * 인증서 버전 7, 인증서 타임스탬프 `0x3E44325E` (2003-02-07 22:04:14), 이미지 타임스탬프 `0x3C44170F` (2002-01-15 11:27:11).
   * 섹션 및 가상 주소 이동:
     - `.text` 섹션 크기 `0x17BB00` (-0x30). 진입점 `0x00147FF3` (-0x30).
     - 이동 경계 분석: `va < 0x00074EC3`에서는 오프셋 차이 0, `0x00074EC3 <= va < 0x0018CB40`에서는 `-0x30`, `0x0018CB40` 이후 섹션들(D3D, DSOUND, MMATRIX, .rdata, .data 등)은 `-0x40`.
     - 언어 전역 변수: `0x001FC6E4` (-0x40), 초깃값 `1` (English). 한국 정발이지만 게임 본편 텍스트는 영문 구성임을 확인.
2. **지문 검증 및 변환 도구 (`variant_fingerprints.py`)**:
   * `translate_va(va, variant)` 구현: 번들 변형의 세그먼트별 주소 변환 규칙 반영.
   * US 판 기준으로 추출된 225개 이름 붙은 함수들의 지문이 `translate_va`를 거쳐 `SegaJSRF.xbe`에서 225/225 (100%) 정확히 일치함을 확인.
   * 프로파일 소스 내 매크로 선언 처리 및 함수 추출 정규식 갱신.
3. **리프터 파이프라인 확장 (`lifter.py`, `regenerate.py`, `variants.json`)**:
   * `variants.json`에 `jsrf_bundle_kor` 추가 (MD5, 디렉터리, 태그, 언어 등).
   * `lifter.py`에서 `jsrf_bundle_kor` 리프팅 시 시드 주소, `xdk_symbols_extra.json`, 그리고 `overrides.c`에 대한 주소 변환 적용.
   * `collect_overrides` 정규식을 `\b(?:sub_|JSRF_DECOMP_)([0-9A-Fa-f]{8})(?:_gen)?\b`로 개선하여 `_gen` 접미사가 붙은 래퍼 함수 선언도 정확히 캡처하도록 수정.
4. **디컴파일 함수 및 데이터 주소 다형화 (`include/rejsrf/jsrf_addresses.h`)**:
   * C 전처리기 매크로 연쇄 확장(cascade expansion, e.g. `sub_001BAA00` -> `sub_001BA9C0` -> `sub_001BA980` 충돌)을 방지하기 위해 각 함수 심볼을 `JSRF_DECOMP_XXXXXXXX` 고유 토큰으로 정의.
   * `src/game/jsrf/decomp/mmatrix.c`, `camera_frustum.c`, `adxm_lock.c`의 함수 선언을 매크로화하여 US판, JP판, Bundle Kor판 모두에서 고유 이름으로 선언/호출되도록 처리.
5. **타이틀 프로파일 및 앱 확장 (`jsrf_profile.cpp`, `main.cpp`)**:
   * `jsrf_profile.cpp`: 번들 변형 인증서, `kBuiltVariant = 2`, `JSRF_D3D_INITIALIZE_DEVICE_STATE` 주소 매핑 등록.
   * `main.cpp`: 게임 디렉터리만 인자로 주어졌을 때 `default.xbe` 외에 `SegaJSRF.xbe`도 자동으로 검색하여 로드하도록 지원.
6. **빌드 시스템 및 스크립트 (`CMakeLists.txt`, `build_win32_x86.ps1`)**:
   * CMake `REJSRF_VARIANT`에 `jsrf_bundle_kor` 추가 및 `REJSRF_VARIANT_BUNDLE_KOR` 정의 추가.
   * US 전용 검증 도구(`rejsrf_decomp_check`, `rejsrf_d3d8_check`)는 `jsrf_us` 빌드에서만 활성화되도록 가드 추가.
   * `build_win32_x86.ps1`에 `-Variant bundle_kor` (별칭 `kor`) 옵션 추가 (`build\win32_x86_bundle_kor`).

## 검증

| 검증 항목 | 결과 |
|---|---|
| XBE 자동 식별 | `SegaJSRF.xbe` → `jsrf_bundle_kor` (MD5: `8bf3c450e9c1cc39730d4025ef987e2c`) 정확히 감지 |
| 함수 지문 일치율 | 225 / 225 (100%) 일치 (`scripts/recomp/variant_fingerprints.py`) |
| 리프터 C 코드 생성 | `gen/jsrf_bundle_kor/gen/` 아래 38개 소스 파일 및 헤더 정상 생성 완료 |
| 컴파일 및 링크 (Release) | 경고 0, 에러 0으로 `build\win32_x86_bundle_kor\src\app\Release\rejsrf.exe` 빌드 성공 |
| 기존 US 빌드 회귀 검증 | `build_win32_x86.ps1 -Variant us` 빌드 성공, 도구 및 디컴파일 검증 통과 |
| 부팅 및 렌더링 검증 | `rejsrf.exe ".\roms\Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)"` 실행 확인: 디렉터리 인자 자동 감지, D3D8 디바이스 생성, 640x480 OpenGL 3.3 초기화, 첫 프레임 표시, 오디오(DSOUND) 및 스레드 정상 초기화, PRESS 화면 텍스처 로드, 5초 watchdog 정상 통과 확인 |

---

# Work Log: Korean Combo Disc (SegaJSRF.xbe) Build Variant Support (issue #124)

Design: [20261026-i124](../design/20261026-i124-bundle-variant.md) · Work order: [20261026-i124](../work-orders/20261026-i124-bundle-variant.md) · Analysis: [jsrf-xbe-overview](../analysis/jsrf-xbe-overview.md)

## Done

1. **Binary Analysis**:
   * Target: `roms\Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)\SegaJSRF.xbe`
   * MD5: `8bf3c450e9c1cc39730d4025ef987e2c`, Size: 2,281,472 bytes (48 bytes smaller than US build).
   * Certificate version 7, Cert timestamp `0x3E44325E` (2003-02-07 22:04:14), Image timestamp `0x3C44170F` (2002-01-15 11:27:11).
   * Section & VA shift rules:
     - `.text` section size `0x17BB00` (-0x30). Entry point `0x00147FF3` (-0x30).
     - Shift boundaries: offset diff 0 for `va < 0x00074EC3`, `-0x30` for `0x00074EC3 <= va < 0x0018CB40`, `-0x40` for subsequent sections starting at `0x0018CB40` (D3D, DSOUND, MMATRIX, .rdata, .data, etc.).
     - Language global variable: `0x001FC6E4` (-0x40), initial value `1` (English). Confirmed that although sold in Korea, the in-game text is configured in English.
2. **Fingerprint Verification & Address Mapping Tool (`variant_fingerprints.py`)**:
   * Implemented `translate_va(va, variant)` handling segmented address shifting.
   * Verified all 225 named functions extracted from the US build match 225/225 (100%) against `SegaJSRF.xbe`.
   * Updated regex and profile macro address handling.
3. **Lifter Pipeline Expansion (`lifter.py`, `regenerate.py`, `variants.json`)**:
   * Added `jsrf_bundle_kor` to `variants.json`.
   * Applied address translation to seed addresses, `xdk_symbols_extra.json`, and `overrides.c` in `lifter.py`.
   * Enhanced `collect_overrides` regex to `\b(?:sub_|JSRF_DECOMP_)([0-9A-Fa-f]{8})(?:_gen)?\b` to accurately capture wrapper functions ending in `_gen`.
4. **Decomp Function & Data Address Mapping (`include/rejsrf/jsrf_addresses.h`)**:
   * Defined unique `JSRF_DECOMP_XXXXXXXX` tokens for decompiled functions to avoid C preprocessor cascading macro expansion collisions (e.g. `sub_001BAA00` -> `sub_001BA9C0` -> `sub_001BA980`).
   * Updated `src/game/jsrf/decomp/mmatrix.c`, `camera_frustum.c`, and `adxm_lock.c` function declarations to use these macro tokens.
5. **Title Profile & App Updates (`jsrf_profile.cpp`, `main.cpp`)**:
   * `jsrf_profile.cpp`: Added certificate info, `kBuiltVariant = 2`, and `JSRF_D3D_INITIALIZE_DEVICE_STATE` address mapping.
   * `main.cpp`: Added automatic resolution of `SegaJSRF.xbe` alongside `default.xbe` when passing a game directory.
6. **Build System & Scripts (`CMakeLists.txt`, `build_win32_x86.ps1`)**:
   * Added `jsrf_bundle_kor` option to CMake `REJSRF_VARIANT` and defined `REJSRF_VARIANT_BUNDLE_KOR`.
   * Guarded US-only tools (`rejsrf_decomp_check`, `rejsrf_d3d8_check`) to build only for `jsrf_us`.
   * Added `-Variant bundle_kor` (alias `kor`) to `build_win32_x86.ps1` targeting `build\win32_x86_bundle_kor`.

## Verification

| Verification | Result |
|---|---|
| XBE Auto-identification | `SegaJSRF.xbe` → `jsrf_bundle_kor` (MD5: `8bf3c450e9c1cc39730d4025ef987e2c`) correctly identified |
| Function Fingerprints | 225 / 225 (100%) match (`scripts/recomp/variant_fingerprints.py`) |
| Lifter Code Generation | 38 C source files and headers generated cleanly in `gen/jsrf_bundle_kor/gen/` |
| Compile & Link (Release) | 0 warnings, 0 errors; `build\win32_x86_bundle_kor\src\app\Release\rejsrf.exe` created |
| US Build Regression | `build_win32_x86.ps1 -Variant us` succeeded; decomp & tools tests passed |
| Boot & Render Verification | Ran `rejsrf.exe ".\roms\Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)"`: directory auto-detection succeeded, D3D8 device created at 640x480 OpenGL 3.3, first frame presented, DSOUND and worker threads initialized, PRESS screen textures loaded, passed 5-second watchdog cleanly |
