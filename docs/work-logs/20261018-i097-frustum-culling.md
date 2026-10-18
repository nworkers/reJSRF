# 작업 로그: Hor+ 와이드스크린의 절두체 컬링 (issue #97)

설계: [20261018-i097](../design/20261018-i097-frustum-culling.md) · 작업 지시: [20261018-i097](../work-orders/20261018-i097-frustum-culling.md)

## 한 일

1. 조사: `D3DDevice_SetTransform(PROJECTION)` 호출자에서 거꾸로 올라가 카메라 시야 설정(`sub_00153FE0` → `sub_00153A90` → `sub_00153EC0`)과 절두체 전역(`0x264E78`~`0x264E94`), 그것을 읽는 두 컬링 함수(`sub_00153B70` 구, `sub_00153D00` 점)를 찾았다. 투영 행렬은 이 전역과 무관하다(설계 2절).
2. `src/game/jsrf/decomp/camera_frustum.c`: `sub_00153A90` 대체. 원본(`_gen`)을 부른 뒤 와이드면 가로 tan에 W/640을 곱하고 sec을 다시 계산해 네 전역에 쓴다.
3. `include/rejsrf/jsrf_host.h`: 호스트 훅 `rejsrf_host_horizontal_view_scale`(널이면 1). 처음에는 디컴파일 C에서 HLE 헤더를 직접 넣었는데 C++ 헤더라 컴파일되지 않았고, 직접 `extern`으로 하면 HLE를 링크하지 않는 `rejsrf_decomp_check`가 링크되지 않아 훅으로 바꿨다.
4. `d3d8_display`: `rexbox_d3d8_horizontal_view_scale()`(C 링크); `main.cpp`가 훅에 넣는다.
5. 문서: 가이드, `ARCHITECTURE.md`, v0.0.40 릴리스 노트의 알려진 한계.

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / D3D8CHECK / 129 case 0 difference |
| 4:3 데모 프레임 1500·4650 | FOURBYTHREE |
| 21:9 데모 프레임 4650 (v0.0.40 대비) | WIDE |
| 사용자 확인 | 대기 |

---

# Work log: frustum culling for Hor+ widescreen (issue #97)

Design: [20261018-i097](../design/20261018-i097-frustum-culling.md) · Work order: [20261018-i097](../work-orders/20261018-i097-frustum-culling.md)

## Done

1. Investigation: walking back from the callers of `D3DDevice_SetTransform(PROJECTION)` to the camera's field-of-view setter (`sub_00153FE0` → `sub_00153A90` → `sub_00153EC0`), the frustum globals (`0x264E78`–`0x264E94`) and the two culling functions reading them (`sub_00153B70` spheres, `sub_00153D00` points). The projection matrix does not depend on these globals (design §2).
2. `src/game/jsrf/decomp/camera_frustum.c`: the `sub_00153A90` replacement. After the original (`_gen`), in wide mode it scales the horizontal tan by W/640, recomputes the secant and writes the four globals.
3. `include/rejsrf/jsrf_host.h`: the host hook `rejsrf_host_horizontal_view_scale` (null means 1). Including the HLE header from the decompiled C failed (a C++ header), and a direct `extern` left `rejsrf_decomp_check`, which links without the HLE, unresolved; hence the hook.
4. `d3d8_display`: `rexbox_d3d8_horizontal_view_scale()` (C linkage); `main.cpp` puts it in the hook.
5. Documents: guide, `ARCHITECTURE.md`, the known limit in release notes v0.0.40.

## Verification

| check | result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks 0 failures / D3D8CHECK / 129 cases 0 differences |
| 4:3 demo frames 1500 and 4650 | FOURBYTHREE_EN |
| 21:9 demo frame 4650 against v0.0.40 | WIDE_EN |
| User | pending |
