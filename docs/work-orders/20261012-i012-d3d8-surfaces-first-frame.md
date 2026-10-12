# 작업 지시: D3D8 HLE 두 번째 묶음 — 리소스 모델과 첫 프레임 (issue #12)

설계: [20261012-i012](../design/20261012-i012-d3d8-surfaces-first-frame.md)

1. 리소스 모델: `d3d8_resource.h/.cpp` — Xbox `D3DResource` 헤더 레이아웃 상수, 헤더 생성(GuestHeap), refcount 규칙(`AddRef`/`Release`/`GetType`), `ResourceTable`(헤더 주소 → 호스트 상태). XGRAPHC `Format`/`Size` 인코딩 해석·작성.
2. 디바이스: `Direct3D_CreateDevice`에서 백버퍼 2개·깊이 1개 헤더와 픽셀 메모리 생성. `GetBackBuffer`, `GetDepthStencilSurface`, `SetRenderTarget`, `Reset`, `SetViewport`.
3. 프레임: `D3DDevice_Clear`(scissor 사각형 포함), `D3DDevice_Swap`(present, 이벤트, 프레임 카운터), vblank·감마·필터 setter는 상태 저장.
4. `D3DSurface_GetDesc`, 그리고 부팅이 닿는 순서대로 `D3D_AllocContiguousMemory`/`Free`, `D3DResource_Register`, `GetTile`/`SetTile`, `CreateTexture` 계열(헤더 + 메모리 + 표 등록까지).
5. `rejsrf_d3d8_check`에 헤더 기반 케이스 추가(AddRef/Release/GetType/GetDesc vs 원본).
6. 빌드·회귀·부팅 검증, frontier 기록, `docs/kb/xbox-d3d8-api.md`(리소스 헤더, XGRAPHC 비트), `ARCHITECTURE.md`, 작업 로그.

---

# Work order: D3D8 HLE, second batch — resource model and the first frame (issue #12)

Design: [20261012-i012](../design/20261012-i012-d3d8-surfaces-first-frame.md)

(1) Resource model `d3d8_resource.h/.cpp`: Xbox `D3DResource` header layout constants, header creation (GuestHeap), refcount rules (`AddRef`/`Release`/`GetType`), `ResourceTable` (header address → host state), reading and writing the XGRAPHC `Format`/`Size` encoding. (2) Device: create the two back-buffer and one depth headers with pixel memory in `Direct3D_CreateDevice`; `GetBackBuffer`, `GetDepthStencilSurface`, `SetRenderTarget`, `Reset`, `SetViewport`. (3) Frame: `D3DDevice_Clear` (with scissored rectangles), `D3DDevice_Swap` (present, events, frame counter); vblank, gamma and filter setters store state. (4) `D3DSurface_GetDesc`, then, as boot reaches them, `D3D_AllocContiguousMemory`/`Free`, `D3DResource_Register`, `GetTile`/`SetTile` and the `CreateTexture` family (header + memory + table entry). (5) Header-based cases in `rejsrf_d3d8_check` (AddRef/Release/GetType/GetDesc against the originals). (6) Build, regression and boot verification, frontier record, `docs/kb/xbox-d3d8-api.md` (resource header, XGRAPHC bits), `ARCHITECTURE.md`, work log.
