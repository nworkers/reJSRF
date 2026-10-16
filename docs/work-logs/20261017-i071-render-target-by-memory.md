# 작업 로그: 렌더 타깃을 표면 메모리로 식별 (issue #71)

설계: [20261017-i071](../design/20261017-i071-render-target-by-memory.md) · 작업 지시: [20261017-i071](../work-orders/20261017-i071-render-target-by-memory.md)

## 분석

* 사용자 캡처 13개(10847~11003, v0.0.31 빌드): 부스트 중 장면이 왼쪽 위 1/4에 축소돼 보이고 나머지는 회색이었다.
* 헤더 `03FDBFE0`은 프레임마다 512×512@`004EC000` 또는 128×128@`0066C000`을 가리킨다. 합성은 매 프레임 두 메모리를 모두 읽는다(설계 §1).
* stderr의 `texture 03FDB080 samples render target 03F80460 (512x512)`: 같은 메모리 `004EC000`에 남은 옛 헤더의 사본을 읽고 있었다.

## 한 일

1. `d3d8_gl_targets`
   * `g_colors`, `g_depths`의 키를 헤더 주소에서 물리 데이터 주소(`SurfaceMemory`)로 바꿨다. `g_framebuffers`는 메모리 쌍으로 찾는다. 바인딩된 타깃도 메모리로 기억한다.
   * `DropIfResized`를 없앴다. `Color`·`Depth`가 같은 메모리에 크기가 다른 표면이 오면 그 메모리의 GL 객체와 그것을 쓰는 FBO만 다시 만든다(`DropFramebuffers`).
   * `BindRenderTarget`의 빠른 경로는 헤더 주소와 헤더의 형식·크기·데이터 세 워드를 비교한다. 헤더가 다른 메모리를 가리키면 다시 찾는다.
   * `SampleRenderTarget`은 메모리로 바로 찾는다(순회 없음). `has_data`/`data`는 `sampleable`로 줄였다. `ColorTarget.header`는 보고에 쓴다.
   * 보고: `render target <헤더> at <메모리> (WxH) ...`, 메모리·크기마다 한 번.
2. `d3d8_gl_textures`: 보고가 `texture X samples render target Y at M (WxH)`가 됐다.
3. `ARCHITECTURE.md`: 렌더 타깃 설명에 메모리 키를 적었다.

## 검증

| 검증 | 결과 |
|---|---|
| Release, Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 162 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 데모 95초(hidden, 오디오 끔) | 프레임 1500, 3000, 4650 BMP가 변경 전과 바이트 단위로 같음. stderr도 파일 경로 말고는 같음 |
| 사용자 플레이(부스트 대시) | 미검증. 확인할 것: 장면이 화면 전체에 보이는지, 회색 영역이 없는지, `samples render target ... at 004EC000`/`at 0066C000` 줄 |

---

# Work log: identify render targets by surface memory (issue #71)

Design: [20261017-i071](../design/20261017-i071-render-target-by-memory.md) · Work order: [20261017-i071](../work-orders/20261017-i071-render-target-by-memory.md)

## Analysis

* 13 user captures (10847 to 11003, v0.0.31 build): during the boost the scene showed shrunk into the top-left quarter, and the rest was grey.
* Header `03FDBFE0` points at 512×512@`004EC000` or 128×128@`0066C000`, depending on the frame. The composite reads both memories every frame (design §1).
* stderr's `texture 03FDB080 samples render target 03F80460 (512x512)`: it was reading the copy of an old header left over the same memory, `004EC000`.

## Done

1. `d3d8_gl_targets`
   * `g_colors` and `g_depths` are keyed by physical data address (`SurfaceMemory`) instead of header address; `g_framebuffers` by the memory pair. The bound target is remembered by memory too.
   * `DropIfResized` is gone. When a surface of another size comes to the same memory, `Color` and `Depth` remake only that memory's GL objects and the FBOs that use them (`DropFramebuffers`).
   * `BindRenderTarget`'s fast path compares the header addresses and the headers' format, size and data words, so a header pointed at other memory resolves again.
   * `SampleRenderTarget` finds the target by memory directly (no loop). `has_data`/`data` became `sampleable`; `ColorTarget.header` is kept for reports.
   * Report: `render target <header> at <memory> (WxH) ...`, once per memory and size.
2. `d3d8_gl_textures`: the report reads `texture X samples render target Y at M (WxH)`.
3. `ARCHITECTURE.md`: the render-target description names the memory key.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 162 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Demo, 95 s (hidden, audio off) | frames 1500, 3000 and 4650 are byte-identical to before; stderr matches apart from file paths |
| User play (boost dash) | not verified. To check: the scene fills the screen, no grey area, and `samples render target ... at 004EC000` / `at 0066C000` lines |
