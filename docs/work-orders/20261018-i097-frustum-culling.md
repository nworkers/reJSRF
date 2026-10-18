# 작업 지시: Hor+ 와이드스크린의 절두체 컬링 (issue #97)

설계: [20261018-i097](../design/20261018-i097-frustum-culling.md)

1. `d3d8_display`: `rexbox_d3d8_horizontal_view_scale()`(C 링크, W/640).
2. `include/rejsrf/jsrf_host.h`: 호스트 훅 `rejsrf_host_horizontal_view_scale`; `main.cpp`가 채움.
3. `src/game/jsrf/decomp/camera_frustum.c`: `sub_00153A90` 대체(원본 호출 후 가로 tan/sec 보정); `regenerate.py`.
4. 검증: 빌드, 회귀 도구, 4:3 프레임 동일성, 21:9 프레임의 가운데 동일성과 양옆 추가.
5. 문서: 작업 로그, 가이드, `ARCHITECTURE.md`, 릴리스 노트 v0.0.40의 알려진 한계.

---

# Work order: frustum culling for Hor+ widescreen (issue #97)

Design: [20261018-i097](../design/20261018-i097-frustum-culling.md)

1. `d3d8_display`: `rexbox_d3d8_horizontal_view_scale()` (C linkage, W/640).
2. `include/rejsrf/jsrf_host.h`: the host hook `rejsrf_host_horizontal_view_scale`, filled by `main.cpp`.
3. `src/game/jsrf/decomp/camera_frustum.c`: the `sub_00153A90` replacement (original, then the horizontal tan/sec correction); `regenerate.py`.
4. Verification: builds, regression tools, 4:3 frame identity, the 21:9 frames' identical middle and added sides.
5. Documents: work log, guide, `ARCHITECTURE.md`, the known limit in release notes v0.0.40.
