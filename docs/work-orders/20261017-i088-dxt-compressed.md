# 작업 지시: DXT 압축 업로드 (issue #88)

설계: [20261017-i088](../design/20261017-i088-dxt-compressed.md)

1. `d3d8_gl`: `glCompressedTexImage2D`, `glGetStringi`를 선택적으로 불러온다. 확장 목록에서 S3TC를 찾는다(`GlHasS3tc`). S3TC 내부 형식 상수.
2. `d3d8_gl_textures`: 2D DXT 레벨을 압축 업로드한다. 경로를 보고하고, `REXBOX_DXT=cpu`를 지원한다. 올린 바이트 수를 센다.
3. 검증: 빌드, 회귀 도구, 데모 BMP 차이 측정, `REXBOX_DXT=cpu` 바이트 비교, Textures VRAM.
4. 문서: 작업 로그, 가이드(`REXBOX_DXT`), `ARCHITECTURE.md`.

---

# Work order: compressed DXT upload (issue #88)

Design: [20261017-i088](../design/20261017-i088-dxt-compressed.md)

1. `d3d8_gl`: load `glCompressedTexImage2D` and `glGetStringi` as optional; find S3TC in the extension list (`GlHasS3tc`); S3TC internal-format constants.
2. `d3d8_gl_textures`: upload 2D DXT levels compressed; report the path; support `REXBOX_DXT=cpu`; count the uploaded bytes.
3. Verification: builds, regression tools, demo BMP difference measured, `REXBOX_DXT=cpu` byte comparison, Textures VRAM.
4. Documents: work log, guide (`REXBOX_DXT`), `ARCHITECTURE.md`.
