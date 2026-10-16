# 작업 로그: 픽셀 셰이더 정의 상수 (issue #78)

설계: [20261017-i078](../design/20261017-i078-psh-definition-constants.md) · 작업 지시: [20261017-i078](../work-orders/20261017-i078-psh-definition-constants.md)

## 분석

* 캡처 17697(xemu와 같은 장소): 그래피티 그리기 #55~#58, #60(정점 셰이더 `03FD6601`, 픽셀 셰이더 9, 1024×1024 DXT 아틀라스)이 owner 맵 세 장 어디에도 픽셀을 남기지 않았다.
* `REXBOX_VSH_WATCH=0x03FD6601:96-127`: 스프레이 중 `c[108].w`, `c[109].w`가 0에서 1.5로 0.05씩 오른다. x·y·z는 0이다. 진행값은 diffuse 채널로만 가고, `oD0.w`는 0이다.
* 픽셀 셰이더 9 해석: 레이어 가중치가 `dot(c0, v0)`(0·2·5번 스테이지, c0 = 녹색·빨강·파랑)과 `v0.a`(4번 스테이지)에서 온다. 파이널 컴바이너는 없다(r0 출력).
* PSC0Mapping `FF0FF2F1` 때문에 HLE가 그 스테이지들의 c0을 쓰지 않은 런타임 레지스터 1, 2, 0(값 0)에서 읽었다. 그래서 알파가 0이 됐다.
* 리프팅된 `D3DDevice_SetPixelShader`는 정의의 상수를 매번 컴바이너에 올린다(설계 §1).

## 한 일

1. `d3d8_shader`: `g_pixel_constants_written`(마지막 SetPixelShader 뒤 쓰인 레지스터), `PixelShaderConstantsWritten()`, `ForgetPixelShaderConstants()`.
2. `d3d8_state`: `D3DDevice_SetPixelShader`가 마스크를 지운다.
3. `d3d8_psh`: `StageConstants(..., written, ...)`는 쓰인 레지스터만 런타임 값을 쓰고, 아니면 정의 값을 쓴다.
4. `d3d8_draw`: 마스크를 넘긴다.
5. `rexbox_d3d8_probe`: 쓰이지 않은 매핑 레지스터는 정의 값을 쓰는지 검사(162 → 163). 가이드의 기대값도 고쳤다.

## 검증

| 검증 | 결과 |
|---|---|
| Release, Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 데모 95초(hidden, 오디오 끔) | 프레임 1500, 3000, 4650 BMP가 v0.0.33과 바이트 단위로 같음 |
| 사용자 플레이(그래피티 스프레이) | 사용자 확인: 그래피티가 잘 나온다 |

---

# Work log: pixel shader definition constants (issue #78)

Design: [20261017-i078](../design/20261017-i078-psh-definition-constants.md) · Work order: [20261017-i078](../work-orders/20261017-i078-psh-definition-constants.md)

## Analysis

* Capture 17697 (the same place as the xemu shot): graffiti draws #55 to #58 and #60 (vertex shader `03FD6601`, pixel shader 9, the 1024×1024 DXT atlas) left no pixel in any of the three owner maps.
* `REXBOX_VSH_WATCH=0x03FD6601:96-127`: while spraying, `c[108].w` and `c[109].w` rise from 0 to 1.5 in steps of 0.05; x, y and z stay 0. The progress goes only to diffuse channels, and `oD0.w` is 0.
* Pixel shader 9 decoded: the layer weights come from `dot(c0, v0)` (stages 0, 2 and 5, with c0 green, red and blue) and from `v0.a` (stage 4). There is no final combiner (r0 is the output).
* Because of PSC0Mapping `FF0FF2F1`, the HLE read those stages' c0 from the unwritten runtime registers 1, 2 and 0 (value 0), so the alpha was 0.
* The lifted `D3DDevice_SetPixelShader` loads the definition's constants into the combiners every time (design §1).

## Done

1. `d3d8_shader`: `g_pixel_constants_written` (registers written since the last SetPixelShader), `PixelShaderConstantsWritten()`, `ForgetPixelShaderConstants()`.
2. `d3d8_state`: `D3DDevice_SetPixelShader` clears the mask.
3. `d3d8_psh`: `StageConstants(..., written, ...)` uses runtime values only for written registers, and the definition's values otherwise.
4. `d3d8_draw`: passes the mask.
5. `rexbox_d3d8_probe`: checks that an unwritten mapped register takes the definition's value (162 → 163). The guide's expected count is updated.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Demo, 95 s (hidden, audio off) | frames 1500, 3000 and 4650 are byte-identical to v0.0.33 |
| User play (spraying graffiti) | the user's check: the graffiti shows |
