# 설계: 렌더 타깃을 헤더가 아니라 표면 메모리로 식별 (issue #71)

관련: [i069 렌더 타깃 샘플링](20261016-i069-render-target-sampling.md)

## 1. 문제

부스트 대시 중 화면이 원본과 다르다. 장면이 왼쪽 위 1/4에 축소돼 보이고 나머지는 회색이다.

사용자 캡처(10847~11003, 13개 프레임)에서 확인한 것(**확인됨**):

* 장면은 프레임마다 한 표면에만 그려진다. 둘 다 헤더 `03FDBFE0`을 쓴다.
  * 512×512, 데이터 `004EC000`, 뷰포트 512×512 (10847, 10939, 10949, 10973, 11003)
  * 128×128, 데이터 `0066C000`, 뷰포트 128×128 (10848, 10860, 10878, 10888, 10904, 10922, 10956, 10984)
* 매 프레임 마지막 합성은 두 메모리를 모두 읽는다.
  * 픽셀 셰이더 10 사각형: 128 표면(텍스처 헤더 `03FDB0A0`, `0066C000`)을 BUMPENVMAP으로 왜곡한다.
  * 그 위에 FVF 0x1C4 사각형: 512 표면(텍스처 헤더 `03FDB080`, `004EC000`)을 정점 알파 0x9F~0xA3으로 섞는다.
* 그 프레임에 그리지 않은 쪽은 앞 프레임의 결과다. 원본은 두 표면 메모리를 각각 그대로 둔다.

지금 구현의 잘못(**확인됨**, `d3d8_gl_targets.cpp`):

1. GL 색 텍스처·깊이 렌더버퍼·FBO를 헤더 주소로 찾는다. `DropIfResized`는 `03FDBFE0`의 크기가 바뀔 때마다 GL 텍스처를 지우고 새로 만든다. 그래서 다른 쪽 표면의 내용이 매번 사라진다(회색).
2. `SampleRenderTarget`은 데이터 주소가 같은 첫 타깃을 고른다. `004EC000`에는 예전에 쓰인 헤더 `03F80460`(512×512)도 남아 있다. stderr에 `texture 03FDB080 samples render target 03F80460`이 있다. 오래된 사본을 읽으니 엉뚱한 장면이 나온다.

## 2. 설계

Xbox에서 표면의 정체는 메모리다. 헤더는 그 메모리를 가리키는 서술자일 뿐이고, 타이틀은 헤더를 다른 메모리에 다시 쓴다. GL 쪽도 메모리로 식별한다.

* **키**: 색·깊이 타깃은 `(물리 데이터 주소)`로 찾는다. 묶을 때마다 헤더에서 데이터 주소와 크기를 읽는다.
  * 같은 메모리에 크기가 다른 표면이 오면 그 메모리의 GL 객체만 다시 만든다(지금 `DropIfResized`의 역할을 메모리 키로 옮긴다).
* **FBO**: `(색 메모리, 깊이 메모리)` 쌍으로 찾는다.
* **샘플링**: `SampleRenderTarget(data)`는 그 메모리의 타깃을 바로 찾는다. 같은 메모리를 가리키는 헤더가 여럿이어도 하나다.
* **해제**: `ForgetRenderTargetMemory`는 그 메모리의 타깃을 샘플링하지 않게 표시한다. 다시 렌더 타깃으로 묶이면 풀린다(지금과 같다).
* **헤더를 받는 함수**(`PresentRenderTarget`, `ReadRenderTarget`, `DumpRenderTarget`, `RenderTargetTexture`)는 헤더에서 메모리를 읽어 찾는다. 바깥 API는 바뀌지 않는다.
* **보고**: `render target <헤더> at <메모리> (WxH) ...`을 메모리마다 한 번 한다.

### 2.1 하지 않는 것

* 같은 메모리를 다른 형식이나 pitch로 보는 헤더. 크기만 비교한다.
* 한 메모리 안의 일부(오프셋이 다른 데이터 주소)를 보는 헤더. 다른 메모리로 취급한다.
* 합성의 다른 문제(bump 행렬, 부호)는 이 수정 뒤 화면을 보고 판단한다.

## 3. 검증

* Release·Debug 빌드, `rexbox_d3d8_probe`, `rejsrf_d3d8_check`, `rejsrf_decomp_check`.
* 데모 프레임 BMP가 변경 전과 같아야 한다(데모는 오프스크린 타깃을 쓰지 않는다).
* 사용자 플레이: 부스트 중 화면 전체에 장면이 보이고 회색 영역이 없는지, `samples render target`가 `03F80460`이 아니라 메모리 기준으로 보고되는지.

---

# Design: identify render targets by surface memory, not by header (issue #71)

Related: [i069 render-target sampling](20261016-i069-render-target-sampling.md)

## 1. Problem

During the boost dash the screen differs from the original: the scene shows shrunk into the top-left quarter, and the rest is grey.

From the user's captures (10847 to 11003, 13 frames) (**confirmed**):

* Each frame draws the scene into one surface only, and both surfaces use header `03FDBFE0`:
  * 512×512, data `004EC000`, viewport 512×512 (10847, 10939, 10949, 10973, 11003)
  * 128×128, data `0066C000`, viewport 128×128 (10848, 10860, 10878, 10888, 10904, 10922, 10956, 10984)
* The composite at the end of every frame reads both memories:
  * a pixel shader 10 quad distorts the 128 surface (texture header `03FDB0A0`, `0066C000`) with BUMPENVMAP;
  * an FVF 0x1C4 quad on top mixes in the 512 surface (texture header `03FDB080`, `004EC000`) with vertex alpha 0x9F to 0xA3.
* The side not drawn in a frame holds an earlier frame's result. The original leaves each surface's memory as it is.

What the current code gets wrong (**confirmed**, `d3d8_gl_targets.cpp`):

1. It looks up GL color textures, depth renderbuffers and FBOs by header address. `DropIfResized` deletes and remakes the GL texture each time `03FDBFE0` changes size, so the other surface's contents are lost every time (the grey).
2. `SampleRenderTarget` takes the first target with the same data address. `004EC000` still has an older header, `03F80460` (512×512); stderr shows `texture 03FDB080 samples render target 03F80460`. Reading its stale copy shows the wrong picture.

## 2. Design

On the Xbox a surface is its memory. A header only describes that memory, and the title points headers at other memory again. The GL side identifies surfaces by memory too.

* **Key**: color and depth targets are looked up by `(physical data address)`. Every bind reads the data address and size from the header.
  * When a surface of another size comes to the same memory, only that memory's GL objects are remade (what `DropIfResized` does now, moved to the memory key).
* **FBOs**: looked up by the `(color memory, depth memory)` pair.
* **Sampling**: `SampleRenderTarget(data)` finds the target at that memory directly. Several headers over the same memory still mean one target.
* **Freeing**: `ForgetRenderTargetMemory` marks the target at that memory as not sampled, until it is bound as a render target again (as now).
* **Functions taking a header** (`PresentRenderTarget`, `ReadRenderTarget`, `DumpRenderTarget`, `RenderTargetTexture`) read the memory from the header. The outside API does not change.
* **Report**: `render target <header> at <memory> (WxH) ...`, once per memory.

### 2.1 Not done

* Headers that view the same memory with another format or pitch; only the size is compared.
* Headers over part of a memory (a data address at an offset); treated as other memory.
* Other composite issues (the bump matrix, signedness) are judged from the screen after this fix.

## 3. Verification

* Release and Debug builds, `rexbox_d3d8_probe`, `rejsrf_d3d8_check`, `rejsrf_decomp_check`.
* The demo frame BMP must match the one from before the change (the demo uses no off-screen targets).
* User play: during the boost the scene fills the screen with no grey area, and `samples render target` is reported by memory rather than naming `03F80460`.
