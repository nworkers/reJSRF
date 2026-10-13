# 설계: D3D8 HLE — 정점 셰이더 프로그램과 레지스터 컴바이너 픽셀 셰이더를 GLSL로 (issue #37)

선행: [i023 고정 기능 그리기](20261012-i023-d3d8-fixed-function-draw.md), [kb: Xbox D3D8 API](../kb/xbox-d3d8-api.md), [analysis: d3d8](../analysis/d3d8-adapter.md), [boot-frontier](../analysis/boot-frontier.md)

## 1. 요구사항과 측정

* 오프닝(로고 뒤 3D 어트랙트 데모)에 도시는 나오지만 캐릭터가 나오지 않는다.
* **확인됨**(Release, 숨김 창, 그리기마다 분류하는 임시 계측): 데모 구간에서 300프레임마다 고정 기능 그리기는 약 8만 번이다. 정점 셰이더 프로그램만 쓴 그리기는 약 5만 번, 프로그램과 픽셀 셰이더를 함께 쓴 그리기는 5천~1만 2천 번이다. 픽셀 셰이더만 쓴 그리기는 없다. HLE는 프로그램이나 픽셀 셰이더가 있는 그리기를 모두 건너뛰고 있었다.
* **확인됨**(`REXBOX_SHADER_DUMP`): 게임이 만드는 정점 셰이더는 154개다.
  * 모두 헤더 형식 `0x2078`이고 스트림 0 하나를 쓴다.
  * 레지스터 형식은 FLOAT1/2/3과 PBYTE4뿐이다.
  * 데모에서 쓰는 18종 가운데 17종이 `arl`과 a0 상대 상수(스키닝 행렬 팔레트)를 쓴다.
  * 모든 프로그램이 런타임의 화면 공간 epilogue로 끝난다(`mul oPos.xyz, R12, c[58]` + `rcc R1.x, R12.w`, 그다음 `mad oPos.xyz, R12, R1.x, c[59]`).
* **확인됨**: 픽셀 셰이더 11개의 기능은 이렇다.
  * 일반 컴바이너 3~8스테이지를 쓰고, 텍스처 모드는 대부분 `PROJECT2D`다.
  * 최종 컴바이너 입력은 모두 0이다(출력은 r0).
  * 상수는 스테이지별 매핑(`PSC0Mapping`)으로 `SetPixelShaderConstant`의 레지스터를 고른다.
  * 2개는 `BUMPENVMAP`을 쓴다.
* 캐릭터 셰이더 3종(`03FD9141`, `03FD9381`, `03FD9501`)은 픽셀 셰이더 1번과 4번(텍스처 여러 장을 r1.a로 섞는 레이어 합성)과 함께 쓰인다.

## 2. 정점 셰이더 프로그램: 마이크로코드 → GLSL

* `d3d8_vsh`(순수 함수, `rexbox_d3d8_probe`로 검사)는 세 가지를 한다.
  1. 128비트 명령을 MAC 연산, ILU 연산, 세 소스(A, B, C: 멀티플렉서·temp·swizzle·부정), 쓰기 마스크, 출력 주소로 디코드한다.
  2. xsasm 비슷한 문법으로 디스어셈블한다.
  3. GLSL 정점 셰이더로 번역한다.
* 필드 배치는 xboxrecomp `nv2a_vsh_interp.c`(MIT)가 실제 프로그램으로 검증해 문서로 남긴 표와 같다. 코드는 가져오지 않고 그 표를 지식으로 썼다([외부 참고](../kb/external-references.md)).
* 의미(**확인됨**: 게임 프로그램의 구조와 결과 화면):
  * 두 유닛은 쓰기 전에 소스를 읽는다.
  * MAC 연산과 짝지은 ILU는 temp 필드와 관계없이 R1에 쓴다.
  * R12를 읽으면 oPos다.
  * `ADD`는 A+C, `MAD`는 A·B+C, `ARL`은 `a0 = floor(A.x + 0.001)`이다.
  * 상대 상수는 `c[clamp(a0 + n, 0, 191)]`이다.
  * 쓰지 않은 색 출력은 (0,0,0,1), 텍스처 좌표는 (0,0,0,1)이다.
* GLSL 구조:
  * 입력 v0..v15는 attribute 0..15, 상수는 `uniform vec4 c[192]`다.
  * 출력은 고정 기능 프래그먼트와 같은 이름(`v_diffuse`, `v_specular`, `v_tex0..3`)을 쓴다. 그래서 픽셀 셰이더가 없는 그리기는 기존 텍스처 스테이지 프래그먼트를 그대로 쓴다.
* **화면 공간 epilogue 되돌리기**:
  * 프로그램이 oPos에 남기는 것은 화면 좌표 `clip.xyz / w × c[58] + c[59]`와 clip w다.
  * GLSL 끝에서 `clip.xyz = (oPos.xyz − c[59]) / c[58] × oPos.w`로 되돌린다. D3D clip z(0..w)는 GL(−w..w)로 옮긴다.
  * `c[58]`/`c[59]`는 HLE가 뷰포트를 정할 때(`ApplyViewport`: SetViewport, SetRenderTarget, 디바이스 생성) 쓴다: `(w/2, −h/2, 1, 1)`, `(x + w/2, y + h/2, 0, 0)`.
  * 하드웨어 값은 깊이 범위와 깊이 버퍼 정밀도도 담지만, GL이 그 부분을 하므로 HLE 고유 값으로 충분하다(**추정**). 게임이 이 상수를 직접 쓰면 그 값이 이긴다(원본과 같은 마지막 쓰기 규칙). 되돌리기는 상수에 무엇이 있든 성립한다.
* **선언**(`D3DVSD`): `STREAM`, `REG(reg, type)`, `SKIP`, `END`를 읽는다. 형식은 float, 정규화 short, short, 정규화 byte, D3DCOLOR(B, G, R, A → GLSL에서 swizzle)이다. 스트림 0이 아닌 선언, `NORMPACKED3`, `CONST`·`TESSELLATOR` 토큰은 보고하고 그리지 않는다(게임이 쓰지 않음).
* 마이크로코드는 `CreateVertexShader` 때 복사한다(원본 라이브러리도 객체에 복사한다). 프로그램 캐시 키는 마이크로코드와 D3DCOLOR 입력의 해시에 프래그먼트 키를 더한 것이다.

## 3. 레지스터 컴바이너 픽셀 셰이더 → GLSL

* `d3d8_psh`(순수 함수)는 `D3DPIXELSHADERDEF` 60 DWORD를 GLSL 프래그먼트 셰이더로 만든다. 모델은 OpenGL `NV_register_combiners` 명세(공개)이고, 인코딩은 XDK 상수 지식이다(kb §13). xboxrecomp의 `nv2a_combiner.c`는 xemu(LGPL)의 생성기를 따른다고 적혀 있어 참고하지 않았다.
* 정의 크기를 0xBC에서 **0xF0**으로 고쳤다(60 DWORD, 덤프의 필드 배치가 맞아떨어짐).
* 스테이지마다 RGB와 알파 portion을 계산한다.
  1. 입력 A, B, C, D를 고른다. 레지스터, 알파 채널 비트, 입력 매핑 8종을 반영한다.
  2. `A·B`(또는 내적)와 `C·D`를 구하고, 합 또는 mux(r0.a의 MSB/LSB)를 고른다.
  3. 출력 배율과 바이어스를 적용하고 [−1, 1]로 자른다.
  4. 목적지(v0, v1, t0..3, r0, r1)에 쓰고, blue-to-alpha를 반영한다. 두 portion은 쓰기 전에 읽는다.
* 초기값은 `r0.a = t0.a`다. 최종 컴바이너는 ABCD·EFG 워드 중 하나라도 0이 아니면 `A·B + (1−A)·C + D`와 `G`(V1+R0 합, E·F 곱 포함)를 쓰고, 둘 다 0이면 r0을 출력한다(**추정**: 게임 셰이더 11개 모두 0이고 결과 화면이 맞다).
* 텍스처 모드: `PROJECT2D`(q로 나눔), `PROJECT3D`, `CUBEMAP`, `PASSTHRU`, `NONE`. 나머지(`BUMPENVMAP` 등 dot/bump 모드)와 `PSCompareMode`는 2D로 근사하고 한 번 보고한다.
* 상수: 스테이지의 c0/c1은 `PSC0/PSC1Mapping` nibble이 가리키는 `SetPixelShaderConstant` 레지스터를 쓰고(0xF는 없음), 없으면 정의의 `PSConstant0/1`을 쓴다. 이때 `PSCombinerCount`의 UNIQUE 플래그가 없으면 스테이지 0의 값을 쓴다. 최종 상수도 같다.
* 안개는 아직 계산하지 않는다(fog 레지스터의 계수 1). 알파 테스트는 고정 기능과 같다.
* 픽셀 셰이더와 고정 기능 정점 처리를 함께 쓰는 그리기는 게임에 없어 보고만 한다.

## 4. 진단

* `REXBOX_SHADER_DUMP=1`: 만들어지는 정점 셰이더마다 선언 토큰과 디스어셈블을, 픽셀 셰이더마다 정의 60 DWORD를 출력한다.

## 5. 검증

| 검증 | 기대 |
|---|---|
| `rexbox_d3d8_probe` | 새 검사: 독립 인코더로 만든 명령의 디코드·디스어셈블, 짝지은 ILU의 R1 쓰기, 상대 상수, epilogue 되돌리기 왕복, 선언 오프셋·D3DCOLOR·SKIP·거부, 컴바이너 스테이지·상수 매핑·최종 컴바이너·bump 보고 |
| 회귀 | kernel/dsound probe, decomp check, d3d8 check, `check_hle_pops.py` |
| 오프닝 프레임 | 캐릭터(앞쪽 스케이터, 행인)와 차량이 나온다 |

## 6. 범위 밖

* **성능**: 데모 구간 Release 초당 8~10프레임(이전 약 20). 그리기마다 정점 업로드, 상태 재적용, 192개 상수 업로드를 한다. 다음 task 후보.
* 안개, `BUMPENVMAP`·dot 텍스처 모드, 다중 스트림, 상수 쓰기 프로그램(`0x7778`), 상태 셰이더(`0x7378`), 고정 기능 조명.

---

# Design: D3D8 HLE — vertex-shader programs and register-combiner pixel shaders as GLSL (issue #37)

Prerequisites: [i023 fixed-function drawing](20261012-i023-d3d8-fixed-function-draw.md), [kb: Xbox D3D8 API](../kb/xbox-d3d8-api.md), [analysis: d3d8](../analysis/d3d8-adapter.md), [boot-frontier](../analysis/boot-frontier.md)

## 1. Requirements and measurements

* The opening (the 3D attract demo after the logos) shows the city but not the characters.
* **Confirmed** (Release, hidden window, temporary per-draw classification): every 300 frames of the demo hold about 80,000 fixed-function draws, about 50,000 draws with a vertex-shader program alone and 5,000–12,000 with a program and a pixel shader; none with a pixel shader alone. The HLE skipped every draw with a program or a pixel shader.
* **Confirmed** (`REXBOX_SHADER_DUMP`): the game creates 154 vertex shaders.
  * All have header type `0x2078` and use stream 0 only.
  * The only register types are FLOAT1/2/3 and PBYTE4.
  * 17 of the 18 the demo uses run `arl` with a0-relative constants (skinning matrix palettes).
  * Every program ends in the runtime's screen-space epilogue (`mul oPos.xyz, R12, c[58]` + `rcc R1.x, R12.w`, then `mad oPos.xyz, R12, R1.x, c[59]`).
* **Confirmed**: the 11 pixel shaders use these features.
  * 3–8 general combiner stages, texture mode mostly `PROJECT2D`.
  * All-zero final-combiner inputs (the output is r0).
  * Per-stage constant mappings (`PSC0Mapping`) onto `SetPixelShaderConstant` registers.
  * Two of them use `BUMPENVMAP`.
* The three character shaders (`03FD9141`, `03FD9381`, `03FD9501`) pair with pixel shaders 1 and 4: layered textures mixed by r1.a.

## 2. Vertex-shader programs: microcode to GLSL

* `d3d8_vsh` (pure functions, checked by `rexbox_d3d8_probe`) does three things.
  1. It decodes the 128-bit instructions: the MAC op, the ILU op, three sources (A, B, C: mux, temp, swizzle, negate), write masks and the output address.
  2. It disassembles them in xsasm-like syntax.
  3. It translates them to a GLSL vertex shader.
* The field layout is the table xboxrecomp's `nv2a_vsh_interp.c` (MIT) documents and checked against real programs; it was used as knowledge, no code was taken ([external references](../kb/external-references.md)).
* Semantics (**confirmed** by the game's program structure and the resulting frames):
  * Both units read their sources before writing.
  * An ILU paired with a MAC op writes R1 whatever the temp field says.
  * Reading R12 reads oPos.
  * `ADD` is A+C, `MAD` A·B+C, `ARL` `a0 = floor(A.x + 0.001)`.
  * A relative constant is `c[clamp(a0 + n, 0, 191)]`.
  * Unwritten colour outputs are (0,0,0,1), and so are unwritten texture coordinates.
* GLSL layout:
  * Inputs v0..v15 are attributes 0..15, and the constants are `uniform vec4 c[192]`.
  * Outputs use the fixed-function fragment stage's names (`v_diffuse`, `v_specular`, `v_tex0..3`), so a draw without a pixel shader keeps the existing texture-stage fragment shader.
* **Undoing the screen-space epilogue**:
  * The program leaves screen coordinates `clip.xyz / w × c[58] + c[59]` and clip w in oPos.
  * The GLSL ends with `clip.xyz = (oPos.xyz − c[59]) / c[58] × oPos.w` and maps D3D clip z (0..w) to GL's (−w..w).
  * The HLE writes `c[58]`/`c[59]` whenever it sets the viewport (`ApplyViewport`: SetViewport, SetRenderTarget, device creation): `(w/2, −h/2, 1, 1)` and `(x + w/2, y + h/2, 0, 0)`.
  * The hardware values also fold in the depth range and the depth buffer's precision, which GL applies itself, so the HLE's own values suffice (**inferred**). A title writing these constants itself wins (last write, as on the original), and the undoing holds whatever they contain.
* **Declarations** (`D3DVSD`): `STREAM`, `REG(reg, type)`, `SKIP` and `END` are read. Types are float, normalized short, short, normalized byte and D3DCOLOR (B, G, R, A, swizzled in GLSL). Declarations with streams other than 0, `NORMPACKED3`, and `CONST` or `TESSELLATOR` tokens are reported and not drawn (the game uses none).
* The microcode is copied at `CreateVertexShader`, as the library copies it into the object. The program cache key is a hash of the microcode and the D3DCOLOR inputs, plus the fragment key.

## 3. Register-combiner pixel shaders to GLSL

* `d3d8_psh` (pure functions) turns the 60 DWORDs of a `D3DPIXELSHADERDEF` into a GLSL fragment shader. The model is OpenGL's public `NV_register_combiners` specification and the encoding is XDK constant knowledge (kb §13). xboxrecomp's `nv2a_combiner.c` says it follows xemu's (LGPL) generator, so it was not consulted.
* The definition size is now **0xF0** (60 DWORDs; was 0xBC); the dumps' field layout fits.
* Each stage computes an RGB and an alpha portion.
  1. Pick inputs A, B, C, D: register, alpha-channel bit, one of eight input mappings.
  2. Form `A·B` (or a dot product) and `C·D`, then their sum or a mux (r0.a's MSB/LSB).
  3. Apply the output scale or bias, then clamp to [−1, 1].
  4. Write the destinations (v0, v1, t0..3, r0, r1), with blue-to-alpha. Both portions read before either writes.
* r0.a starts as t0.a. The final combiner computes `A·B + (1−A)·C + D` and `G` (with the V1+R0 sum and the E·F product) when its ABCD or EFG word is non-zero; with both zero the output is r0 (**inferred**: all 11 game shaders have zeros and the frames come out right).
* Texture modes: `PROJECT2D` (divided by q), `PROJECT3D`, `CUBEMAP`, `PASSTHRU`, `NONE`. The others (dot and bump modes such as `BUMPENVMAP`) and `PSCompareMode` are approximated as 2D and reported once.
* Constants: a stage's c0/c1 is the `SetPixelShaderConstant` register its `PSC0/PSC1Mapping` nibble names (0xF: none). Otherwise it is the definition's `PSConstant0/1`, stage 0's unless `PSCombinerCount` marks them unique. The final constants work the same way.
* Fog is not computed yet (the fog register's factor is 1); the alpha test is the fixed-function one.
* A pixel shader with fixed-function vertex processing does not occur in the game and is only reported.

## 4. Diagnostics

`REXBOX_SHADER_DUMP=1` prints each created vertex shader's declaration tokens and disassembly, and each pixel shader's 60 definition DWORDs.

## 5. Verification

The table above applies:
* **`rexbox_d3d8_probe`:** new checks on instructions built by an independent encoder (decode, disassembly, the paired ILU's R1 write, relative constants), the epilogue round trip, declaration offsets with D3DCOLOR, SKIP and refusals, and combiner stages, constant mapping, the final combiner and the bump report.
* **Regressions:** none in the kernel and DSOUND probes, the decomp and d3d8 checks, and `check_hle_pops.py`.
* **Opening frames:** characters (the skater in front, pedestrians) and vehicles appear.

## 6. Out of scope

* **Performance**: the demo runs at 8–10 frames per second in Release (about 20 before). Every draw uploads its vertices, re-applies all state and uploads 192 constants. A candidate for the next task.
* Fog, the `BUMPENVMAP` and dot texture modes, multiple streams, constant-writing programs (`0x7778`), state shaders (`0x7378`) and fixed-function lighting.
