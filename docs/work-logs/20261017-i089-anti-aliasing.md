# 작업 로그: MSAA, SMAA, 비등방성 필터링 (issue #89)

설계: [20261017-i089](../design/20261017-i089-anti-aliasing.md) · 작업 지시: [20261017-i089](../work-orders/20261017-i089-anti-aliasing.md)

## 한 일

1. `d3d8_gl`
   * `glRenderbufferStorageMultisample`을 선택적으로 불러온다.
   * `GL_MAX_SAMPLES`와 `GL_EXT_texture_filter_anisotropic`/`GL_ARB_texture_filter_anisotropic`의 최대 배율을 읽는다(`GlMaxSamples`, `GlMaxAnisotropy`).
2. 비등방성 필터링(`d3d8_gl_textures`)
   * 샘플러 캐시 키에 배율을 더했다(`(상태, 배율)` 쌍).
   * 축소 필터가 선형인 스테이지에만 `GL_TEXTURE_MAX_ANISOTROPY`를 붙인다. 점 필터 텍스처(글꼴·UI)는 그대로다.
3. MSAA(`d3d8_gl_targets`)
   * 색 타깃은 배율이 1보다 크면 멀티샘플 렌더버퍼에 그리고, 같은 크기의 단일 샘플 텍스처를 resolve 대상으로 둔다. 깊이 렌더버퍼도 같은 샘플 수다.
   * `ResolveColor`/`ReadableFramebuffer`: 텍스처로 샘플할 때, present 때, 덤프·`ReadPixels` 때 그 타깃이 그려진 뒤 처음이면 blit으로 resolve한다.
   * 샘플 수가 바뀌면 타깃을 다시 만든다(내용은 resolve한 뒤 옮긴다). 실행 중 변경은 다음 present에 적용되고 `[d3d8] MSAA Nx` 또는 `off`가 나온다.
   * FBO가 불완전하면 한 번 보고하고 MSAA를 끈다. VRAM 계산은 샘플 수를 곱한다.
4. SMAA 1x(`d3d8_smaa`, 새 파일)
   * `third_party/smaa`(iryoku/smaa, MIT) 서브모듈을 넣었다. `cmake/embed_text.cmake`가 `SMAA.hlsl`을 바이트 배열 헤더로 만든다.
   * 파일의 비 ASCII 바이트(주석 속 문자)는 실행 때 공백으로 바꾼다. MSVC C4828을 피하려고 문자열 리터럴이 아니라 바이트로 넣었다.
   * GLSL 3, 프리셋 High. 색 에지 검출(RG8, 카툰 렌더링의 같은 밝기·다른 색상 경계를 잡으려고 휘도 대신 RGB 차이) → 가중치 계산(RGBA8, AreaTex RG8과 SearchTex R8) → 이웃 혼합의 세 패스다. 화면 크기 텍스처는 크기가 바뀔 때 다시 만든다.
   * present에서 감마 램프·blit 앞에 건다. 셰이더 오류는 한 번 보고하고 SMAA 없이 그린다.
5. 설정(`d3d8_display`, 앱)
   * `DisplayDefaults`와 접근자 `MsaaSamples`/`Smaa`/`Anisotropy`, 그리고 각각의 `Set*`·최댓값 함수를 더했다.
   * `rejsrf.ini` 키 `Msaa`, `Smaa`, `Anisotropy`를 더했다.
   * Graphics 탭의 Image quality 구역에 콤보 두 개를 두었다: Anti-aliasing(Off, MSAA 2x~16x, SMAA 1x를 한 목록에 두어 MSAA와 SMAA는 동시에 켜지지 않는다)과 Anisotropic filtering. GPU 한계를 넘는 값은 비활성이고, 바꾸면 바로 저장된다. ini에 둘 다 켜져 있으면 MSAA를 쓴다.
6. 문서: `ARCHITECTURE.md`, 가이드, `THIRD_PARTY_NOTICES.md`(SMAA).

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 숨긴 창 데모, 전부 끔 | 프레임 1500, 3000, 4650 BMP가 변경 전(i088)과 바이트 단위로 같음 |
| 보이는 창, MSAA 4x + AF 16x | 난간·계단 경계의 계단 현상이 줄고, 비스듬한 바닥·벽 텍스처가 선명해졌다(눈으로 비교). 데모 타이밍이 실행마다 달라 픽셀 비교는 하지 않았다 |
| 보이는 창, SMAA 1x(배율 1) | `[d3d8] SMAA ready (preset high)`. 계단 경계가 부드러워졌다(확대 비교). 텍스처는 조금 부드러워진다(SMAA의 일반적인 특성) |
| 메뉴를 연 채 데모(보이는 창, MSAA 2x·배율 4 / SMAA 1x / 끔) | Info·Graphics 탭 모두 연속 6프레임에서 메뉴가 온전하다. 깜빡임은 재현되지 않았다 |
| 사용자 플레이 | 확인 대기 |

## 남은 일

* SMAA T2x, S2x, 4x는 넣지 않기로 했다. T2x·4x는 속도 버퍼 없이 재투영해 카툰 렌더링의 외곽선에 잔상이 생긴다. S2x는 MSAA 샘플별 SMAA가 필요하고, MSAA 뒤에 SMAA를 그냥 거는 것은 효과가 작아 MSAA와 SMAA를 상호 배타로 했다(설계 2.3, 2.4).

---

# Work log: MSAA, SMAA, anisotropic filtering (issue #89)

Design: [20261017-i089](../design/20261017-i089-anti-aliasing.md) · Work order: [20261017-i089](../work-orders/20261017-i089-anti-aliasing.md)

## Done

1. `d3d8_gl`
   * `glRenderbufferStorageMultisample` is loaded as optional.
   * `GL_MAX_SAMPLES` and the largest ratio of `GL_EXT_texture_filter_anisotropic`/`GL_ARB_texture_filter_anisotropic` are read (`GlMaxSamples`, `GlMaxAnisotropy`).
2. Anisotropic filtering (`d3d8_gl_textures`)
   * The sampler cache key now carries the ratio (a `(state, ratio)` pair).
   * `GL_TEXTURE_MAX_ANISOTROPY` is set only on stages whose minification filter is linear. Point-filtered textures (fonts, UI) are left as they were.
3. MSAA (`d3d8_gl_targets`)
   * With more than one sample, a color target draws into a multisample renderbuffer and keeps a single-sample texture of the same size to resolve into. The depth renderbuffer gets the same sample count.
   * `ResolveColor`/`ReadableFramebuffer`: when a target is sampled as a texture, presented, dumped or read with `ReadPixels` for the first time since it was drawn, a blit resolves it.
   * A changed sample count remakes the target (its contents are resolved and then copied over). A change while running applies at the next present and prints `[d3d8] MSAA Nx` or `off`.
   * An incomplete FBO is reported once and turns MSAA off. The VRAM count multiplies by the sample count.
4. SMAA 1x (`d3d8_smaa`, new)
   * The `third_party/smaa` submodule (iryoku/smaa, MIT) is added. `cmake/embed_text.cmake` turns `SMAA.hlsl` into a byte-array header.
   * The file's non-ASCII bytes (characters in comments) become spaces at run time. Bytes rather than a string literal avoid MSVC C4828.
   * GLSL 3, preset High, in three passes: color edge detection (RG8; RGB difference rather than luma, to catch the cel-shaded borders between areas of equal brightness and different hue), then weight calculation (RGBA8, with AreaTex RG8 and SearchTex R8), then neighborhood blending. The screen-sized textures are remade when the size changes.
   * It runs at present, before the gamma ramp and blit. A shader error is reported once and the frame is drawn without SMAA.
5. Settings (`d3d8_display`, app)
   * `DisplayDefaults` and the accessors `MsaaSamples`/`Smaa`/`Anisotropy`, with a `Set*` and a maximum function for each.
   * `rejsrf.ini` keys `Msaa`, `Smaa`, `Anisotropy`.
   * Two combos in the Graphics tab's Image quality section: Anti-aliasing (Off, MSAA 2x-16x and SMAA 1x in one list, so MSAA and SMAA are never on together) and Anisotropic filtering. Values over the GPU's limits are disabled, and a change is saved at once. With both on in the ini, MSAA is used.
6. Documents: `ARCHITECTURE.md`, the guide, `THIRD_PARTY_NOTICES.md` (SMAA).

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Hidden-window demo, all off | frames 1500, 3000 and 4650 byte-identical to before (i088) |
| Visible window, MSAA 4x + AF 16x | less stair-stepping on railing and stair edges, and sharper floor and wall textures at grazing angles (by eye). Demo timing differs between runs, so no pixel comparison |
| Visible window, SMAA 1x (scale 1) | `[d3d8] SMAA ready (preset high)`. Stair edges are smoother (zoomed comparison). Textures soften slightly, as SMAA usually does |
| Demo with the menu open (visible window, MSAA 2x at scale 4 / SMAA 1x / off) | the menu is whole in six frames in a row on both the Info and Graphics tabs; no flicker reproduced |
| User play | waiting for the user's check |

## Remaining

* SMAA T2x, S2x and 4x are dropped. T2x and 4x reproject without a velocity buffer and would ghost the cel-shaded outlines. S2x needs SMAA per MSAA sample, and plain SMAA after MSAA gains little, so MSAA and SMAA exclude each other (design 2.3, 2.4).
