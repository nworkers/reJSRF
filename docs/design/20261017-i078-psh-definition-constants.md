# 설계: 픽셀 셰이더 정의 상수와 SetPixelShaderConstant (issue #78)

관련: [i069 렌더 타깃 샘플링, 픽셀 셰이더](20261016-i069-render-target-sampling.md)

## 1. 문제

스프레이한 그래피티가 벽에 보이지 않는다(xemu에서는 보임).

사용자 캡처(17586, 17697, 13755)와 `REXBOX_VSH_WATCH=0x03FD6601:96-127` 로그에서 확인한 것(**확인됨**):

* 그래피티 그리기는 있다. 정점 셰이더 `03FD6601`과 픽셀 셰이더 9를 쓰고, 텍스처는 `Z:\Media\Mark\DEFAULT\JSRF_TEXS*.JTX`에서 온 1024×1024 DXT 아틀라스다. 그런데 owner 맵에 이 그리기들의 픽셀이 없다.
* 정점 셰이더는 스프레이 진행값 `c[108+i].w`(스프레이 중 0에서 1.5로 오름)를 diffuse 채널에 넣는다.
* 픽셀 셰이더 9는 레이어마다 `dot(c0, v0)`으로 가중치를 고른다. 0·2·5번 스테이지의 c0은 정의의 PSConstant0에 녹색·빨강·파랑(`0000FF00`, `00FF0000`, `000000FF`)으로 들어 있다.
* 정의의 PSC0Mapping(`FF0FF2F1`)은 그 스테이지들을 SetPixelShaderConstant 레지스터 1, 2, 0에 연결한다.
* 지금 HLE(`psh::StageConstants`)는 매핑이 있으면 런타임 레지스터를 읽는다. 게임은 이 레지스터를 쓰지 않으므로 값이 0이다. 가중치가 모두 0이 되고, 알파 0이 알파 테스트(GREATER 0)에서 모두 버려진다.

원본 라이브러리의 동작(리프팅된 `D3DDevice_SetPixelShader` 0x00199BE0, `D3DDevice_SetPixelShaderConstant` 0x00199DB0, **확인됨**):

* `SetPixelShader`는 설정할 때마다 정의의 PSConstant0/1(메서드 0xA60부터 32워드)과 파이널 컴바이너 상수를 컴바이너 레지스터에 그대로 보낸다.
* `SetPixelShaderConstant`는 현재 셰이더의 매핑을 따라 해당 스테이지의 레지스터만 덮어쓴다.
* 따라서 마지막 `SetPixelShader` 뒤에 쓰이지 않은 레지스터는 정의의 값을 가진다.

## 2. 설계

* `d3d8_shader`: 런타임 레지스터마다 "마지막 SetPixelShader 뒤에 쓰였는가"를 비트 마스크로 둔다. `SetPixelShaderConstant`가 비트를 세우고, `SetPixelShader`가 지운다.
* `psh::StageConstants(..., written)`: 매핑 니블이 가리키는 레지스터가 쓰였을 때만 런타임 값을 쓰고, 아니면 정의의 값을 쓴다. 스테이지 상수와 파이널 상수 모두 같다.
* 같은 셰이더를 다시 설정해도 정의 값으로 돌아간다(원본이 매번 정의를 보내므로).

### 2.1 하지 않는 것

* 런타임 값을 D3DCOLOR 바이트로 양자화하는 것(원본은 0..1로 자르고 8비트로 바꾼다). 결과 차이가 보이면 다룬다.

## 3. 검증

* Release·Debug 빌드, `rexbox_d3d8_probe`(쓰인 레지스터와 안 쓰인 레지스터 검사 추가), `rejsrf_d3d8_check`, `rejsrf_decomp_check`.
* 데모 프레임 BMP 비교(데모에 같은 경우가 있으면 바뀔 수 있다).
* 사용자 플레이: 스프레이한 그래피티가 벽에 보이는지.

---

# Design: pixel shader definition constants and SetPixelShaderConstant (issue #78)

Related: [i069 render-target sampling, pixel shaders](20261016-i069-render-target-sampling.md)

## 1. Problem

Sprayed graffiti does not show on the wall (it does in xemu).

From the user's captures (17586, 17697, 13755) and a `REXBOX_VSH_WATCH=0x03FD6601:96-127` log (**confirmed**):

* The graffiti draws exist. They use vertex shader `03FD6601` and pixel shader 9, with the 1024×1024 DXT atlas from `Z:\Media\Mark\DEFAULT\JSRF_TEXS*.JTX`. Yet the owner maps hold no pixel from them.
* The vertex shader puts the spray progress `c[108+i].w` (0 rising to 1.5 while spraying) into diffuse channels.
* Pixel shader 9 picks each layer's weight with `dot(c0, v0)`. The c0 of stages 0, 2 and 5 is green, red and blue (`0000FF00`, `00FF0000`, `000000FF`) in the definition's PSConstant0.
* The definition's PSC0Mapping (`FF0FF2F1`) maps those stages to SetPixelShaderConstant registers 1, 2 and 0.
* The HLE (`psh::StageConstants`) reads the runtime register whenever a stage is mapped. The title never writes these registers, so they are 0. Every weight is 0, and the zero alpha fails the alpha test (GREATER 0) everywhere.

What the original library does (lifted `D3DDevice_SetPixelShader` 0x00199BE0 and `D3DDevice_SetPixelShaderConstant` 0x00199DB0, **confirmed**):

* Every `SetPixelShader` sends the definition's PSConstant0/1 (32 words from method 0xA60) and the final-combiner constants to the combiner registers as they are.
* `SetPixelShaderConstant` overwrites only the registers of the stages the current shader maps.
* So a register not written since the last `SetPixelShader` holds the definition's value.

## 2. Design

* `d3d8_shader`: a bit mask records which runtime registers were written since the last SetPixelShader. `SetPixelShaderConstant` sets bits, and `SetPixelShader` clears them.
* `psh::StageConstants(..., written)`: a mapped stage takes the runtime value only when its register was written; otherwise it takes the definition's value. Stage and final constants alike.
* Setting the same shader again returns to the definition's values, since the original sends the definition every time.

### 2.1 Not done

* Quantizing runtime values to D3DCOLOR bytes (the original clamps to 0..1 and converts to 8 bits). To be handled if a difference shows.

## 3. Verification

* Release and Debug builds, `rexbox_d3d8_probe` (with checks for written and unwritten registers), `rejsrf_d3d8_check`, `rejsrf_decomp_check`.
* Comparing demo frame BMPs (they may change if the demo has the same case).
* User play: sprayed graffiti shows on the wall.
