# 설계: Hor+ 와이드스크린의 절두체 컬링 (issue #97)

## 1. 목표

* Hor+([설계 i085](20261018-i085-widescreen-horplus.md))에서 게임의 절두체 컬링은 4:3 기준이라 양옆에서 물체가 늦게 나타난다. 와이드 모드일 때 컬링의 **가로 시야만** 넓혀 넓어진 화면에 보이는 것을 모두 그리게 한다.
* 투영 행렬은 건드리지 않는다(HLE가 Hor+를 계속 처리한다). 4:3에서는 아무것도 달라지지 않는다.

## 2. 조사(**확인됨**, 리프트된 코드와 `REXBOX_PROJ_TRACE` 임시 추적)

* 카메라의 시야 설정 `sub_00153FE0(this, angle16 fov, …)`는 `sub_00153A90(this)`로 절두체 전역을 만들고, `sub_00153EC0`이 투영 행렬 전역 `0x22E6B8`을 캐시 `0x264EF8`에 복사해 `D3DDevice_SetTransform(PROJECTION)`을 부른다. 장치 래퍼 vtable(.rdata `0x1E0F40`…)의 슬롯이 이 함수들이다.
* `sub_00153A90`은 sin/cos 표(`[0x264BA4]`, `sub_0014C640` = cos, `sub_0014C660` = tan)로 다음 전역을 쓴다.

  | 주소 | 값 |
  |---|---|
  | `0x264E80`, `0x264E88` | 1/cos(가로 반각) |
  | `0x264E84` | −tan(가로 반각) |
  | `0x264E8C` | tan(가로 반각) |
  | `0x264E78`, `0x264E90` | 1/cos(세로 반각) |
  | `0x264E7C`, `0x264E94` | ±tan(세로 반각) |

* 이 전역을 읽는 함수는 둘뿐이다. `sub_00153B70(ecx = 구 x,y,z,r)`은 `x`를 `z·tan ± r/cos`와, `y`를 세로 값과 비교하고 깊이를 `0x22E6AC`와 비교한다. `sub_00153D00(ecx = 점)`은 같은 검사를 반지름 없이 한다. 호출자는 `sub_00153C50`/`sub_00153CA0`, `sub_00153DB0`/`sub_00153DF0`(vtable 슬롯)다.
* 투영 행렬은 이 전역에서 만들지 않는다. 따라서 전역만 바꾸면 그림은 그대로이고 컬링만 달라진다.

## 3. 설계

### 3.1 원리

가로 시야가 `W/640`배 넓어지면 가로 반각의 탄젠트도 `W/640`배다(Hor+는 화면 평면에서 x를 그만큼 더 보인다). 그래서 `sub_00153A90`이 끝난 뒤

* `tan' = tan × W/640`, `sec' = sqrt(1 + tan'^2)`
* `0x264E8C = tan'`, `0x264E84 = −tan'`, `0x264E80 = 0x264E88 = sec'`

로 고쳐 쓴다. 세로 값과 깊이는 그대로다. 4:3(`W/640 = 1`)에서는 쓰지 않는다.

### 3.2 디컴파일 대체(`src/game/jsrf/decomp/camera_frustum.c`)

* `void sub_00153A90(void)`를 정의한다. `scripts/recomp/regenerate.py`가 이것을 보고 원본을 `sub_00153A90_gen`으로 낸다(`adxm_lock.c`와 같은 방식). 대체는 `_gen`을 먼저 부르고(ecx 그대로, thiscall) 전역을 고친다. 디컴파일이 아니라 호스트 어댑터다.
* `rejsrf_decomp_check`는 D3D8 HLE 없이 `rejsrf_game`만 링크하므로, 게임 코드가 HLE 함수를 직접 참조하면 안 된다. `include/rejsrf/jsrf_host.h`에 C 훅 `float (*rejsrf_host_horizontal_view_scale)(void)`을 두고(널이면 1), 앱(`main.cpp`)이 `rexbox_d3d8_horizontal_view_scale`로 채운다.

### 3.3 HLE(`d3d8_display`)

* `extern "C" float rexbox_d3d8_horizontal_view_scale(void)`: `WideWidth(TargetAspect(), 백버퍼 높이) / 백버퍼 너비`, 4:3이면 1. `TargetAspect`는 present 끝에 적용되는 값이라 다음 프레임의 그림과 맞는다.

### 3.4 적용 시점

게임은 카메라 시야를 바꿀 때마다 `sub_00153A90`을 다시 부른다(장면 로드, 카메라 전환, 줌). 실행 중 종횡비를 바꾸면 다음에 시야가 설정될 때부터 컬링이 따라온다. 그때까지는 예전 기준이다(알려진 한계; 대부분 몇 초 안에 다시 설정된다).

## 4. 검증

* 빌드 경고 0, 회귀 도구 세 가지.
* 4:3 데모 프레임이 v0.0.40과 같다.
* 21:9 데모 프레임의 가운데 640x480이 v0.0.40의 21:9 프레임과 같고(래스터화 차이 안), 양옆에만 물체가 더 그려진다.

---

# Design: frustum culling for Hor+ widescreen (issue #97)

## 1. Goals

* With Hor+ ([design i085](20261018-i085-widescreen-horplus.md)) the game's frustum culling is still 4:3, so objects pop in at the sides. In wide mode widen **only the horizontal field of view of the culling**, so everything the widened picture shows is drawn.
* The projection stays as it is (the HLE keeps doing Hor+). Nothing changes at 4:3.

## 2. Findings (**confirmed**, from the lifted code and a temporary `REXBOX_PROJ_TRACE`)

* The camera's field-of-view setter `sub_00153FE0(this, angle16 fov, …)` builds the frustum globals through `sub_00153A90(this)`, then `sub_00153EC0` copies the projection global `0x22E6B8` into the cache `0x264EF8` and calls `D3DDevice_SetTransform(PROJECTION)`. These are slots of the device wrapper's vtable (.rdata `0x1E0F40`…).
* `sub_00153A90` writes, from the sin/cos table (`[0x264BA4]`, `sub_0014C640` = cos, `sub_0014C660` = tan):

  | address | value |
  |---|---|
  | `0x264E80`, `0x264E88` | 1/cos(half horizontal angle) |
  | `0x264E84` | −tan(half horizontal angle) |
  | `0x264E8C` | tan(half horizontal angle) |
  | `0x264E78`, `0x264E90` | 1/cos(half vertical angle) |
  | `0x264E7C`, `0x264E94` | ±tan(half vertical angle) |

* Only two functions read them. `sub_00153B70(ecx = sphere x,y,z,r)` compares `x` with `z·tan ± r/cos`, `y` with the vertical values, and the depth with `0x22E6AC`; `sub_00153D00(ecx = point)` does the same without a radius. Callers: `sub_00153C50`/`sub_00153CA0`, `sub_00153DB0`/`sub_00153DF0` (vtable slots).
* The projection matrix is not built from these globals, so changing them changes the culling and nothing else.

## 3. Design

### 3.1 Principle

A horizontal view `W/640` times wider means the tangent of the half horizontal angle is `W/640` times larger (Hor+ shows that much more x in the view plane). So after `sub_00153A90`:

* `tan' = tan × W/640`, `sec' = sqrt(1 + tan'^2)`
* `0x264E8C = tan'`, `0x264E84 = −tan'`, `0x264E80 = 0x264E88 = sec'`

The vertical values and the depth stay. Nothing is written at 4:3 (`W/640 = 1`).

### 3.2 Decompiled replacement (`src/game/jsrf/decomp/camera_frustum.c`)

* Defines `void sub_00153A90(void)`; `scripts/recomp/regenerate.py` sees it and emits the original as `sub_00153A90_gen` (as for `adxm_lock.c`). The replacement calls `_gen` first (ecx intact, thiscall) and then corrects the globals. A host adapter, not a decompilation.
* `rejsrf_decomp_check` links `rejsrf_game` without the D3D8 HLE, so game code must not reference HLE functions directly. `include/rejsrf/jsrf_host.h` holds the C hook `float (*rejsrf_host_horizontal_view_scale)(void)` (null means 1), and the application (`main.cpp`) fills it with `rexbox_d3d8_horizontal_view_scale`.

### 3.3 HLE (`d3d8_display`)

* `extern "C" float rexbox_d3d8_horizontal_view_scale(void)`: `WideWidth(TargetAspect(), back buffer height) / back buffer width`, 1 at 4:3. `TargetAspect` is the value applied at the end of the present, matching the next frame's picture.

### 3.4 When it applies

The game calls `sub_00153A90` whenever it sets the camera's field of view (scene loads, camera changes, zoom). After an aspect change while running the culling follows from the next field-of-view setting; until then the old bound holds (known limit; usually a few seconds).

## 4. Verification

* Zero build warnings; the three regression tools.
* 4:3 demo frames identical to v0.0.40.
* The middle 640x480 of the 21:9 demo frames equals v0.0.40's 21:9 frames (within rasterization differences), with objects added only at the sides.
