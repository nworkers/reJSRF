# 설계: 안티에일리어싱과 텍스처 필터링 옵션 (issue #89)

관련: [i080 렌더링 배율](20261017-i080-render-scale.md), [i084 설정 오버레이](20261017-i084-settings-overlay.md)

## 1. 요구

설정 메뉴에서 고르는 그래픽 개선 옵션.

1. MSAA: 2x, 4x, 8x, 16x
2. SMAA: 1x (T2x, S2x, 4x는 빼기로 함, 2.3 참고)
3. 이방성 필터링(AF): 2x, 4x, 8x, 16x

게임이 보는 값(D3D 상태, 렌더 타깃 크기)은 바꾸지 않는다. 드라이버가 지원하지 않는 값은 메뉴에서 고를 수 없다.

## 2. 설계

### 2.1 이방성 필터링

* 샘플러 객체에 `GL_TEXTURE_MAX_ANISOTROPY`(0x84FE)를 건다. GL 4.6 코어이고, `GL_EXT/ARB_texture_filter_anisotropic`의 같은 값이다. 상한은 `GL_MAX_TEXTURE_MAX_ANISOTROPY`(0x84FF)로 읽는다.
* 선형 축소 필터(밉맵 포함)를 쓰는 샘플러에만 건다. 점 필터는 게임이 의도한 것이므로 두지 않는다.
* 샘플러 캐시 키에 이방성 값을 넣는다. 값이 바뀌면 새 샘플러가 만들어지고, 다음 바인딩에 쓰인다.

### 2.2 MSAA

* 색 타깃마다 멀티샘플 색 렌더버퍼를 둔다. 깊이 타깃은 멀티샘플 렌더버퍼로 만든다. FBO는 색 렌더버퍼와 깊이 렌더버퍼를 붙이고, 게임의 그리기와 Clear는 그쪽으로 간다.
* 단일 샘플 텍스처(지금의 `ColorTarget.texture`)는 **resolve 대상**이다. 텍스처를 붙인 resolve FBO로 `glBlitFramebuffer`(같은 크기, 반전 없음)해서 채운다. 멀티샘플 blit은 크기와 방향을 바꿀 수 없다. 그래서 상하 반전 사본은 resolve된 텍스처에서 만든다.
* resolve 시점은 텍스처를 읽기 직전이다. 샘플링 사본(`SampleRenderTarget`), present, 캡처·덤프 읽기, 배율 변경 시 내용 복사가 여기에 해당한다. 타깃이 묶인 뒤로 그려졌을 때만 다시 resolve한다(`bound_since_copy`와 같은 방식).
* 샘플 수가 바뀌면 배율과 같이 present 뒤에 적용한다. 타깃은 다음에 쓰일 때 다시 만든다. 멀티샘플로는 blit해 넣을 수 없어서 옛 내용은 이어지지 않는다(한 프레임).
* 상한은 `GL_MAX_SAMPLES`다. FBO가 불완전하면 MSAA를 끄고 한 번 보고한다.

### 2.3 SMAA

* 구현: 공식 SMAA(iryoku/smaa, MIT, submodule `third_party/smaa`)의 `SMAA.hlsl`을 `SMAA_GLSL_3`, `SMAA_PRESET_HIGH`로 컴파일한다. 빌드할 때 CMake가 파일을 문자열 헤더로 만든다. `AreaTex.h`(160×560 RG8)와 `SearchTex.h`(64×16 R8)는 텍스처로 올린다.
* 위치: present에서 보여 줄 백 버퍼를 resolve한 텍스처(렌더 배율 크기)에 세 단계를 건다. 결과는 같은 크기의 텍스처이고, present는 그것을 창으로 늘려(감마 램프 포함) 보인다.
  1. 색 엣지 검출 → RG8 엣지 텍스처. 카툰 렌더링 화면에는 밝기는 비슷하고 색상만 다른 면이 많아, 휘도가 아니라 RGB 차이로 찾는다.
  2. 블렌딩 가중치 계산 → RGBA8 가중치 텍스처
  3. 이웃 블렌딩 → RGBA8 결과
* 모드
  * **1x**: 위의 세 단계다.
  * **T2x, S2x, 4x는 넣지 않는다.** T2x와 4x는 이전 프레임을 재투영해 섞는데, 이 게임에는 속도 버퍼가 없어 굵은 외곽선에 잔상이 생기고 단색 면이 번진다. 카툰 렌더링에 맞지 않는다. S2x(2x MSAA + 샘플별 SMAA)의 효과는 메뉴에서 MSAA 2x와 SMAA 1x를 함께 고르는 것으로 대신한다.
* MSAA와 함께 쓰지 않는다. MSAA로 resolve한 화면은 경계가 이미 중간색이라 SMAA가 경계를 덜 찾고, 찾은 곳은 한 번 더 흐려진다. 비용만 늘어 메뉴에서 둘 중 하나만 고르게 한다(2.4).

### 2.4 설정

* 메뉴 Graphics 탭의 "Image quality" 절에 드롭다운 두 개를 둔다: Anti-aliasing(Off, MSAA 2x, 4x, 8x, 16x, SMAA 1x)과 Anisotropic filtering(Off, 2x, 4x, 8x, 16x). MSAA와 SMAA는 한 목록이라 동시에 켤 수 없다. 드라이버 상한을 넘는 값은 비활성이다.
* `rejsrf.ini`의 `Msaa`, `Smaa`, `Anisotropy`. 파일에 `Msaa`와 `Smaa`가 모두 켜져 있으면 MSAA를 쓰고 SMAA는 끈다. 공개 헤더 `d3d8_display.h`의 `DisplayDefaults`에 더하고 `Set/Get` 함수를 둔다.
* Info 탭의 VRAM 집계에 멀티샘플 렌더버퍼를 더한다.

### 2.5 하지 않는 것

* SMAA T2x/S2x/4x, FXAA, 텍스처 업스케일링.

## 3. 검증

* Release·Debug 빌드, 회귀 도구.
* 모두 끈 상태: 숨긴 창 데모 프레임 BMP가 변경 전과 같다.
* 각 옵션: 데모 프레임 덤프의 가장자리 계단, 텍스처 선명도를 눈으로 비교한다. MSAA는 F9 캡처·프레임 덤프가 resolve된 그림인지, 부스트 블러(렌더 타깃 샘플링)가 깨지지 않는지 본다.
* 사용자 플레이: 실행 중 변경, 성능.

---

# Design: anti-aliasing and texture filtering options (issue #89)

Related: [i080 render scale](20261017-i080-render-scale.md), [i084 settings overlay](20261017-i084-settings-overlay.md)

## 1. Requirement

Graphics options chosen in the settings menu:

1. MSAA: 2x, 4x, 8x, 16x
2. SMAA: 1x (T2x, S2x and 4x dropped, see 2.3)
3. Anisotropic filtering (AF): 2x, 4x, 8x, 16x

What the title sees (D3D state, render-target sizes) does not change, and values the driver does not support cannot be chosen.

## 2. Design

### 2.1 Anisotropic filtering

* `GL_TEXTURE_MAX_ANISOTROPY` (0x84FE) is set on sampler objects: core in GL 4.6 and the same value in `GL_EXT/ARB_texture_filter_anisotropic`. The limit is read from `GL_MAX_TEXTURE_MAX_ANISOTROPY` (0x84FF).
* Only samplers with a linear minification filter (mipmapped or not) get it; point filtering is the title's intent and stays.
* The anisotropy is part of the sampler cache key, so a change makes new samplers that the next binding uses.

### 2.2 MSAA

* Each color target gets a multisample color renderbuffer, and depth targets are made as multisample renderbuffers. The FBO attaches those, and the title's draws and clears go there.
* The single-sample texture (today's `ColorTarget.texture`) becomes the **resolve target**, filled by `glBlitFramebuffer` (same size, no flip) into a resolve FBO with the texture attached. A multisample blit cannot change size or direction, so flipped sampling copies are made from the resolved texture.
* It resolves just before the texture is read: sampling copies (`SampleRenderTarget`), present, capture and dump reads, and the content copy on a scale change. It resolves again only if the target was bound since (as with `bound_since_copy`).
* A sample-count change applies after a present, like the scale; targets are remade when next used. Nothing can be blitted into a multisample buffer, so the old contents do not carry over (one frame).
* The limit is `GL_MAX_SAMPLES`. If an FBO is incomplete, MSAA turns off and is reported once.

### 2.3 SMAA

* Implementation: the official SMAA (iryoku/smaa, MIT, submodule `third_party/smaa`); `SMAA.hlsl` compiled with `SMAA_GLSL_3` and `SMAA_PRESET_HIGH`, made into a string header by CMake at build time. `AreaTex.h` (160×560 RG8) and `SearchTex.h` (64×16 R8) are uploaded as textures.
* Where: in present, on the resolved texture of the back buffer being shown (at the render-scale size), three passes. The result is a texture of the same size, which present stretches to the window (with the gamma ramp).
  1. Color edge detection → RG8 edges texture. The cel-shaded picture has many flat areas of similar brightness and different hue, so edges are found by RGB difference rather than luma.
  2. Blending weight calculation → RGBA8 weights texture
  3. Neighborhood blending → RGBA8 result
* Modes
  * **1x**: the three passes above.
  * **T2x, S2x and 4x are not included.** T2x and 4x blend in the reprojected previous frame; the title has no velocity buffer, so the thick outlines would ghost and flat areas smear, which does not suit cel shading. S2x (2x MSAA plus per-sample SMAA) is approximated by picking MSAA 2x together with SMAA 1x in the menu.
* It is not combined with MSAA. A picture resolved from MSAA already has in-between colors at its edges, so SMAA finds fewer edges and softens the ones it finds once more; the cost grows for little gain, so the menu offers one or the other (2.4).

### 2.4 Settings

* An "Image quality" section in the menu's Graphics tab with two dropdowns: Anti-aliasing (Off, MSAA 2x, 4x, 8x, 16x, SMAA 1x) and Anisotropic filtering (Off, 2x, 4x, 8x, 16x). MSAA and SMAA share one list, so they are never on together. Values over the driver's limits are disabled.
* `Msaa`, `Smaa`, `Anisotropy` in `rejsrf.ini` (with both `Msaa` and `Smaa` on in the file, MSAA is used and SMAA is off); added to `DisplayDefaults` in the public `d3d8_display.h`, with `Set/Get` functions.
* The Info tab's VRAM figures include the multisample renderbuffers.

### 2.5 Not done

* SMAA T2x/S2x/4x, FXAA, texture upscaling.

## 3. Verification

* Release and Debug builds, regression tools.
* All off: hidden-window demo frame BMPs identical to before.
* Each option: compare edge stair-stepping and texture sharpness in demo frame dumps by eye. For MSAA, check that F9 captures and frame dumps are the resolved picture, and that the boost blur (render-target sampling) stays intact.
* User play: changing them while running, performance.
