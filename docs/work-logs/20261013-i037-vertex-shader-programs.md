# Issue #37 작업 로그 — 오프닝의 캐릭터: 정점 셰이더 프로그램과 픽셀 셰이더

작업 지시: [20261013-i037](../work-orders/20261013-i037-vertex-shader-programs.md) · 설계: [20261013-i037](../design/20261013-i037-vertex-shader-programs.md)

## 요약

오프닝에 캐릭터가 나오지 않던 원인은, D3D8 HLE가 정점 셰이더 프로그램을 쓰는 그리기를 모두 건너뛴 것이었다. 이 그리기는 데모 구간 그리기의 약 40%를 차지한다. NV2A 정점 마이크로코드를 GLSL로 옮기는 번역기와, 캐릭터 몸체에 쓰이는 레지스터 컴바이너 픽셀 셰이더 → GLSL 생성기를 만들었다. 이제 오프닝 3D 데모에 캐릭터(앞쪽 스케이터, 행인)와 그동안 빠졌던 차량이 그려진다. 그리기가 늘어 데모 구간 속도는 Release에서 초당 8~10프레임이다(이전 약 20).

## 과정

1. 사용자가 `default.xbe`를 미국판으로 바꾼 뒤 lift 코드를 다시 생성했다(9,193 함수, 미해결 스텁 83). 처음 시도는 이전 `default.xbe`가 미국판이 아니어서 MD5 검사에서 멈췄다.
2. **측정**(임시 계측, 커밋하지 않음): 그리기를 고정 기능, 정점 프로그램, 픽셀 셰이더, 둘 다로 나눠 300프레임마다 셌다. Debug 150초는 데모까지 가지 못해 Release로 다시 쟀다. 데모 구간에서 정점 프로그램 그리기가 약 5만, 둘 다가 5천~1만 2천이었다(설계 §1).
3. **덤프**: `REXBOX_SHADER_DUMP`(새 진단)로 정점 셰이더 154개의 선언과 디스어셈블, 픽셀 셰이더 11개의 정의를 받았다.
   * 디코더 출력이 그럴듯한 프로그램을 이루었다(행렬 상수와의 `dp4`, 화면 공간 epilogue). 그래서 필드 배치와 "oPos를 화면 좌표에서 되돌리는" 방식을 정했다.
   * 쓰인 선언 형식은 네 가지뿐이었다. 데모 셰이더 18종 중 17종이 스키닝이었다.
4. **정점 프로그램**:
   * `d3d8_vsh`(디코드, 디스어셈블, GLSL, 선언, 뷰포트 상수)를 만들었다.
   * 마이크로코드를 생성 때 복사하게 했다.
   * `ApplyViewport`가 `c[58]`/`c[59]`를 쓰게 했다.
   * 그리기 경로에 선언으로 속성을 묶고 상수 192개를 올리는 분기를 더했다. 프래그먼트는 기존 텍스처 스테이지 셰이더를 그대로 쓴다.
5. 첫 프레임 덤프에서 작은 캐릭터들은 나왔지만, 앞쪽의 큰 캐릭터는 어두운 실루엣뿐이었다. 그 몸체 그리기는 픽셀 셰이더 1·4번을 함께 쓴다.
6. **픽셀 셰이더**:
   * 덤프의 60 DWORD를 알려진 배치로 읽어 필드를 확인했다. 그 과정에서 HLE가 정의를 0xBC바이트만 복사하던 것을 0xF0으로 고쳤다.
   * `d3d8_psh`를 `NV_register_combiners` 모델로 만들었다. 입력 매핑 8종, 곱·내적·합·mux, 출력 배율·바이어스, blue-to-alpha, 최종 컴바이너, 텍스처 모드, 스테이지 상수 매핑을 다룬다.
   * blue-to-alpha가 같은 레지스터의 알파 portion 쓰기보다 우선하도록, 알파 portion을 먼저 쓴다.
7. 두 번째 덤프에서 앞쪽 캐릭터가 텍스처를 입고 나왔다(2,800, 3,000, 3,200, 3,400프레임).
8. **라이선스**: 정점 명령 필드는 xboxrecomp `nv2a_vsh_interp.c`(MIT)의 표를 지식으로 참고했다. 컴바이너는 공개 명세로 직접 구현했다. xboxrecomp `nv2a_combiner.c`는 xemu(LGPL) 생성기를 따랐다고 밝히고 있어 열어 보지 않았다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| `rexbox_d3d8_probe` (Debug, Release) | 116/116 (새 검사 35개) |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` | 56/56, 48/48 |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 6/6, 14,018/0 (Debug, Release) |
| `check_hle_pops.py` | 불일치 0 |
| 오프닝 프레임(숨김 창, Release) | 2,800~3,400프레임에 캐릭터·차량이 그려지고, 컴파일·링크 실패 보고가 없음 |
| 속도 | 데모 구간 300프레임에 29~37초(초당 8~10프레임) |

덤프 이미지는 게임 자산이라 커밋하지 않는다.

## 남은 일

* **성능**(다음 task 후보): 그리기마다 정점 업로드, 상태 재적용, 상수 192개 업로드, 키 문자열 생성을 한다.
* 가까운 캐릭터 몇이 청록 단색에 가까운 프레임(3,400)을 원본과 비교한다. 안개(아직 계산하지 않음)나 조명 상수일 수 있다.
* 안개, `BUMPENVMAP`·dot 텍스처 모드(픽셀 셰이더 10·11번), 다중 스트림, 고정 기능 조명.

---

# Issue #37 work log — characters in the opening: vertex-shader programs and pixel shaders

Work order: [20261013-i037](../work-orders/20261013-i037-vertex-shader-programs.md) · Design: [20261013-i037](../design/20261013-i037-vertex-shader-programs.md)

## Summary

The characters were missing from the opening because the D3D8 HLE skipped every draw that uses a vertex-shader program, about 40% of the demo's draws. A translator from NV2A vertex microcode to GLSL and a generator from register-combiner pixel shaders (used by the character bodies) to GLSL now exist. The opening's 3D demo draws the characters (the skater in front, pedestrians) and the vehicles that were missing too. With more drawing, the demo runs at 8–10 frames per second in Release (about 20 before).

## Process

1. After the user replaced `default.xbe` with the US build, the lifted code was regenerated (9,193 functions, 83 unresolved stubs). The first attempt stopped at the MD5 check because the previous `default.xbe` was not the US build.
2. **Measurement** (temporary instrumentation, not committed): draws were counted every 300 frames as fixed-function, vertex program, pixel shader, or both. A 150 s Debug run did not reach the demo, so Release was measured. The demo had about 50,000 vertex-program draws and 5,000–12,000 with both (design §1).
3. **Dumps**: `REXBOX_SHADER_DUMP` (a new diagnostic) gave the declarations and disassembly of the 154 vertex shaders and the definitions of the 11 pixel shaders.
   * The decoder's output formed sensible programs (`dp4` against matrix constants, the screen-space epilogue), which settled the field layout and the approach of mapping oPos back from screen coordinates.
   * Only four declaration types occur, and 17 of the demo's 18 shaders are skinned.
4. **Vertex programs**:
   * `d3d8_vsh` (decoding, disassembly, GLSL, declarations, viewport constants) was written.
   * The microcode is now copied at creation.
   * `ApplyViewport` writes `c[58]`/`c[59]`.
   * The draw path gained a branch that binds attributes from the declaration and uploads the 192 constants; the fragment side keeps the existing texture-stage shader.
5. The first frame dumps showed the small characters, but the large one in front was only a dark silhouette: its body draws use pixel shaders 1 and 4.
6. **Pixel shaders**:
   * The dumped 60 DWORDs fit the known layout, which also showed that the HLE copied only 0xBC bytes of the definition; it is now 0xF0.
   * `d3d8_psh` follows the `NV_register_combiners` model: the eight input mappings, products, dot products, sums and the mux, output scale and bias, blue-to-alpha, the final combiner, texture modes and stage constant mapping.
   * The alpha portion is written first, so blue-to-alpha wins over it for the same register.
7. The second dumps show the front character textured (frames 2,800, 3,000, 3,200 and 3,400).
8. **Licensing**: for the vertex instruction fields, the table in xboxrecomp's `nv2a_vsh_interp.c` (MIT) was consulted as knowledge. The combiners were implemented from the public specification. xboxrecomp's `nv2a_combiner.c`, which says it follows xemu's (LGPL) generator, was not opened.

## Verification

The table in the Korean section applies:
* **Builds:** Win32 Debug and Release with zero warnings on our targets.
* **Probes:** `rexbox_d3d8_probe` 116/116 (35 new checks) in both configurations; kernel probe 56/56; DSOUND probe 48/48.
* **Checks:** decomp check 6/6; d3d8 check 14,018/0 in Debug and Release; `check_hle_pops.py` 0 mismatches.
* **Opening frames:** hidden window, Release: characters and vehicles are drawn at frames 2,800–3,400, with no compile or link failures reported.
* **Speed:** 29–37 s per 300 demo frames (8–10 frames per second).

The frame dumps are game assets and are not committed.

## Remaining

* **Performance** (a next-task candidate): every draw uploads its vertices, re-applies all state, uploads 192 constants and builds key strings.
* Compare the frame (3,400) where a few close characters look nearly flat teal with the original; fog (not computed yet) or lighting constants are possible causes.
* Fog, the `BUMPENVMAP` and dot texture modes (pixel shaders 10 and 11), multiple streams, fixed-function lighting.
