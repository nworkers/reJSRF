# 작업 로그: 렌더 타깃 샘플링, 픽셀 셰이더와 고정 기능 정점 처리 (issue #69)

설계: [20261016-i069](../design/20261016-i069-render-target-sampling.md) · 작업 지시: [20261016-i069](../work-orders/20261016-i069-render-target-sampling.md)

## 한 일

1. `d3d8_gl_targets`
   * `ColorTarget`에 샘플링용 사본(텍스처와 FBO), "마지막 복사 뒤 묶였는가" 표시, 묶을 때 읽은 데이터 주소를 더했다.
   * `SampleRenderTarget`은 데이터 주소로 타깃을 찾는다. 필요할 때 `glBlitFramebuffer`로 상하를 뒤집어 복사한다. 지금 묶인 타깃이면 다음에도 다시 복사한다.
   * `ForgetRenderTargetMemory`: 그 메모리가 해제되면(`DestroyResource`) 더는 그 타깃을 샘플링하지 않는다. 해제된 메모리를 새 텍스처가 받아도 옛 타깃을 읽지 않게 하기 위해서다.
   * 오프스크린 타깃 보고 문구를 "sampled through a copy"로 바꿨다.
2. `d3d8_gl_textures`: `ResolveTexture`가 먼저 렌더 타깃을 찾는다. 처음 한 번 `texture X samples render target Y (WxH)`를 보고한다. 선형 헤더는 텍셀 좌표로 샘플링한다.
3. `d3d8_draw`
   * `BuildKey(..., pixel_shader)`: 픽셀 셰이더 모드에서는 네 스테이지 모두 `SELECTARG1`로 활성이다. 좌표 선택·생성·변환은 그대로 읽고, 스테이지 연산과 알파 테스트는 키에 넣지 않는다.
   * 픽셀 셰이더 + 고정 기능 경로를 그린다. 조합기 상수 설정은 `SetPixelShaderUniforms`로 묶어 두 경로가 함께 쓴다.
4. `d3d8_gl_programs`: `GetFixedFunctionPixelProgram`(고정 기능 정점 + 조합기 조각), `CombinerLocations`.
5. `rexbox_d3d8_probe`: 픽셀 셰이더 모드 키의 정점 셰이더가 조합기 조각 셰이더의 입력을 모두 쓰는지 검사(158 → 159).

## 검증

| 검증 | 결과 |
|---|---|
| Release, Debug 빌드 | 새 경고 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 159 check 0 failure / 14018 case 0 difference / 6 case 0 difference |
| 데모 95초(hidden, 오디오 끔) | 새 보고 없음. 60 fps. 프레임 4650 BMP가 변경 전과 바이트 단위로 같음 |
| 사용자 플레이(부스트 대시) | 미검증. 데모는 오프스크린 타깃도, 픽셀 셰이더 + 고정 기능 그리기도 쓰지 않는다. 확인할 것: 화면이 흐려지는지, `samples render target` 줄, 셰이더 컴파일·링크 오류가 없는지 |

## 추가: BUMPENVMAP (사용자 확인 뒤, 설계 §2.4)

* 사용자 확인: 멈춤과 검은 화면은 해결됐다. 블러는 보이지만 어색하다. 로그에 픽셀 셰이더 10의 1번 스테이지 모드 6이 2D로 근사됐다는 보고가 있다.
* `psh::GenerateFragmentShader`: BUMPENVMAP과 휘도형을 그린다. `u_bump_mat[4]`, `u_bump_lum[4]`, `psh_signed`를 쓴다. 키에 `bump_source_argb`, `BoundTexture`에 `format`을 더했다.
* 그리기: 두 픽셀 셰이더 경로가 스테이지마다 bump 상태를 uniform으로 보낸다.
* 검증: Debug 빌드 경고 없음. `rexbox_d3d8_probe` 160 check 0 failure(기존 "mode 6 보고" 검사를 "그림" 검사로 바꿈).
  * Release 링크는 사용자 게임이 실행 중이라 이 시점에 하지 못했다.
  * 실제 장면 확인은 사용자 플레이가 필요하다.

## 추가: bump 좌표의 투영

* 사용자 확인(BUMPENVMAP 뒤): 부스트 때 화면 사본이 왼쪽 위 1/4에 두 배 크기로 나왔다. 나머지는 회색이었다.
* bump 경로가 `v_tex.xy`를 `w`로 나누지 않은 탓이다. 이전 2D 경로처럼 나눈 좌표에 bump 오프셋을 더하게 바꿨다. probe 검사도 이 식을 본다.
* 실제 장면 확인은 사용자 플레이가 필요하다.

## 추가: 크기가 바뀐 렌더 타깃 다시 만들기

* 사용자 캡처(10306~10370, 소유 지도와 정점 포함):
  * 부스트 중 장면은 `03FDBFE0`에 뷰포트 128×128로 그려진다.
  * 그 헤더의 크기 필드는 `S0307F07F`, 곧 선형 128×128, pitch 256이다.
  * 합성 사각형(`ps 0A`, FVF 0x202)의 1번 텍스처 좌표는 텍셀 단위 (0..128, 16..112)다(**확인됨**).
* 우리는 이 표면을 처음 본 크기(512×512)로 만든 GL 텍스처를 계속 썼다. 같은 헤더 주소가 다른 크기의 표면으로 다시 쓰인 것이다(**추정**). 그래서 좌표 배율이 어긋났다.
* `DropIfResized`: 렌더 타깃을 묶을 때마다 헤더 크기를 다시 읽는다. 다르면 색·깊이 GL 객체, 샘플링 사본, 그것을 쓰는 FBO를 지우고 새로 만든다. GL 표에 `glDeleteFramebuffers`, `glDeleteRenderbuffers`를 더했다.
* 검증: Release 경고 없음. 데모 프레임 4650 BMP가 변경 전과 같음. `rexbox_d3d8_probe` 160/0, `rejsrf_d3d8_check` 14018/0. 실제 부스트 확인은 사용자 플레이가 필요하다.

## 남은 것

* X8 형식 타깃의 알파, 헤더와 타깃 크기가 다른 경우(설계 §2.3).
* 실제 장면에서 블러가 원본과 다르게 보이면, 그때의 보고와 화면을 이 issue에 남긴다.

---

# Work log: render-target sampling; pixel shaders with fixed-function vertex processing (issue #69)

Design: [20261016-i069](../design/20261016-i069-render-target-sampling.md) · Work order: [20261016-i069](../work-orders/20261016-i069-render-target-sampling.md)

## Done

1. `d3d8_gl_targets`
   * `ColorTarget` gained a sampling copy (texture and FBO), a "bound since the last copy" flag, and the data address read when it is bound.
   * `SampleRenderTarget` finds a target by data address and, when needed, copies it flipped vertically with `glBlitFramebuffer`. A target that is still bound is copied again next time.
   * `ForgetRenderTargetMemory`: once that memory is freed (`DestroyResource`), the target is no longer sampled there, so a new texture in freed memory does not read an old target.
   * The off-screen target report now says "sampled through a copy".
2. `d3d8_gl_textures`: `ResolveTexture` looks for a render target first and reports `texture X samples render target Y (WxH)` once. Linear headers are sampled with texel coordinates.
3. `d3d8_draw`
   * `BuildKey(..., pixel_shader)`: in pixel-shader mode all four stages are active with `SELECTARG1`. Coordinate selection, generation and transforms are read as before; stage operations and the alpha test stay out of the key.
   * The pixel shader with fixed-function path draws. Setting the combiner constants moved into `SetPixelShaderUniforms`, which both paths use.
4. `d3d8_gl_programs`: `GetFixedFunctionPixelProgram` (fixed-function vertices with a combiner fragment stage) and `CombinerLocations`.
5. `rexbox_d3d8_probe`: checks that the vertex shader for a pixel-shader-mode key writes every input of the combiner fragment shader (158 → 159).

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no new warnings |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 159 checks, 0 failures / 14018 cases, 0 differences / 6 cases, 0 differences |
| Demo, 95 s (hidden, audio off) | no new reports; 60 fps; the frame 4650 BMP is byte-identical to before |
| User play (boost dash) | not verified. The demo uses neither off-screen targets nor pixel shaders with fixed-function draws. To check: the screen blurs; a `samples render target` line; no shader compile or link errors |

## Added: BUMPENVMAP (after the user's check, design §2.4)

* The user's check: the hang and the black screen are fixed; the blur shows but looks off. The log reports pixel shader 10's stage-1 mode 6 approximated as 2D.
* `psh::GenerateFragmentShader` draws BUMPENVMAP and its luminance form with `u_bump_mat[4]`, `u_bump_lum[4]` and `psh_signed`. The key gained `bump_source_argb`, and `BoundTexture` gained `format`.
* Draws: both pixel-shader paths send each stage's bump states as uniforms.
* Verification: no Debug warnings; `rexbox_d3d8_probe` 160 checks, 0 failures (the "mode 6 is reported" check became "it is drawn").
  * The Release link could not run at this point because the user's game was running.
  * Checking the real scene needs the user's play.

## Added: projecting bump coordinates

* The user's check (after BUMPENVMAP): during the boost the screen copy filled the top-left quarter at twice its size, with the rest grey.
* The bump path did not divide `v_tex.xy` by `w`. It now adds the bump offset to the divided coordinates, as the earlier 2D path did, and the probe checks this expression.
* Checking the real scene needs the user's play.

## Added: remaking render targets whose size changed

* The user's captures (10306 to 10370, with owner maps and vertices):
  * During the boost the scene is drawn into `03FDBFE0` with a 128×128 viewport.
  * That header's size field is `S0307F07F`: linear 128×128, pitch 256.
  * The composite quad's (`ps 0A`, FVF 0x202) stage-1 texture coordinates are texels (0..128, 16..112) (**confirmed**).
* We kept using a GL texture made at the size the surface first had (512×512). The same header address was set up again as a surface of another size (**inferred**), so the coordinate scale was wrong.
* `DropIfResized`: every render-target bind reads the header size again. On a change it deletes and remakes the color and depth GL objects, the sampling copy and the FBOs that use them. The GL table gained `glDeleteFramebuffers` and `glDeleteRenderbuffers`.
* Verification: no Release warnings; the demo's frame 4650 BMP is unchanged; `rexbox_d3d8_probe` 160/0, `rejsrf_d3d8_check` 14018/0. Checking the real boost needs the user's play.

## Left

* Alpha of X8-format targets, and headers whose size differs from the target's (design §2.3).
* If the blur looks different from the original in the real scene, record the reports and a screenshot in this issue.
