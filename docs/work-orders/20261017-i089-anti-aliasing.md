# 작업 지시: 안티에일리어싱과 텍스처 필터링 (issue #89)

설계: [20261017-i089](../design/20261017-i089-anti-aliasing.md)

1. GL 함수·상수: `glRenderbufferStorageMultisample`, `GL_MAX_SAMPLES`, 이방성 상수. 확장 확인(이방성).
2. AF: 샘플러 캐시 키와 `GL_TEXTURE_MAX_ANISOTROPY`.
3. MSAA: 멀티샘플 색·깊이 렌더버퍼, resolve FBO와 resolve 시점, 샘플 수 변경 적용, VRAM 집계.
4. SMAA 1x: submodule `third_party/smaa`, CMake로 셰이더 문자열 헤더, 텍스처, 세 단계, present 연결.
5. 공개 API(`d3d8_display.h`), 설정 파일, 메뉴(Anti-aliasing 절).
6. 검증과 문서: 작업 로그, 가이드, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`.

---

# Work order: anti-aliasing and texture filtering (issue #89)

Design: [20261017-i089](../design/20261017-i089-anti-aliasing.md)

1. GL functions and constants: `glRenderbufferStorageMultisample`, `GL_MAX_SAMPLES`, anisotropy constants; extension check (anisotropy).
2. AF: the sampler cache key and `GL_TEXTURE_MAX_ANISOTROPY`.
3. MSAA: multisample color and depth renderbuffers, the resolve FBO and when to resolve, applying sample-count changes, VRAM figures.
4. SMAA 1x: submodule `third_party/smaa`, a shader string header made by CMake, textures, the three passes, the present hookup.
5. Public API (`d3d8_display.h`), settings file, menu (Anti-aliasing section).
6. Verification and documents: work log, guide, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`.
