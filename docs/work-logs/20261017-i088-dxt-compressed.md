# 작업 로그: DXT 압축 업로드 (issue #88)

설계: [20261017-i088](../design/20261017-i088-dxt-compressed.md) · 작업 지시: [20261017-i088](../work-orders/20261017-i088-dxt-compressed.md)

## 한 일

1. `d3d8_gl`
   * `glCompressedTexImage2D`, `glGetStringi`를 선택적으로 불러온다.
   * 함수 표를 불러온 뒤 확장 목록에서 `GL_EXT_texture_compression_s3tc`를 찾는다(`GlHasS3tc`).
   * 상수: `GL_EXTENSIONS`, `GL_NUM_EXTENSIONS`, S3TC DXT1/3/5 RGBA 내부 형식.
2. `d3d8_gl_textures`
   * `DxtCompressedPath`: S3TC가 있고 `REXBOX_DXT=cpu`가 아니면 압축 경로다. 처음 한 번 어느 경로인지 보고한다.
   * 2D DXT 레벨은 `glCompressedTexImage2D`로 게스트 메모리를 그대로 올린다(블록 단위 바이트). 크기를 기록하고 디코드·잘라내기는 건너뛴다.
   * `DxtInternalFormat`: DXT1 → RGBA DXT1(c0 ≤ c1이면 인덱스 3이 투명), DXT3, DXT5.
   * `gl_bytes`는 압축 레벨이면 올린 바이트 수를 센다.
3. 문서: `ARCHITECTURE.md`, 가이드(`REXBOX_DXT`).

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 숨긴 창 데모, `REXBOX_DXT=cpu` | `decoded on the CPU (REXBOX_DXT=cpu)`. 프레임 1500, 3000, 4650 BMP가 변경 전과 바이트 단위로 같음 |
| 숨긴 창 데모, 기본 | `uploaded compressed (S3TC)`. 변경 전과의 차이(채널 최대 / 평균 / 바뀐 픽셀 / 8 초과): 1500은 2 / 0.027 / 2.6% / 0, 3000은 4 / 0.253 / 24.2% / 0, 4650은 5 / 0.287 / 26.5% / 0. GPU와 우리 디코더의 반올림 차이다 |
| 텍스처 VRAM(설정 메뉴, 첫 프레임 후 약 45초, 텍스처 115개) | 12.8 MiB. CPU 디코드 때 같은 115개가 70.7 MiB(i084 작업 로그)였으니 약 5.5배 줄었다 |
| 사용자 플레이(텍스처 모양, DXT1 구멍, 알파) | 사용자 확인: 잘 동작한다 |

---

# Work log: compressed DXT upload (issue #88)

Design: [20261017-i088](../design/20261017-i088-dxt-compressed.md) · Work order: [20261017-i088](../work-orders/20261017-i088-dxt-compressed.md)

## Done

1. `d3d8_gl`
   * `glCompressedTexImage2D` and `glGetStringi` are loaded as optional.
   * After the table is loaded, `GL_EXT_texture_compression_s3tc` is looked up in the extension list (`GlHasS3tc`).
   * Constants: `GL_EXTENSIONS`, `GL_NUM_EXTENSIONS`, the S3TC DXT1/3/5 RGBA internal formats.
2. `d3d8_gl_textures`
   * `DxtCompressedPath`: the compressed path when S3TC is present and `REXBOX_DXT=cpu` is not set; the path is reported once.
   * 2D DXT levels go up from guest memory as they are through `glCompressedTexImage2D` (bytes in whole blocks); the size is recorded and decoding and cropping are skipped.
   * `DxtInternalFormat`: DXT1 → RGBA DXT1 (index 3 transparent when c0 ≤ c1), DXT3, DXT5.
   * `gl_bytes` counts the uploaded bytes for compressed levels.
3. Documents: `ARCHITECTURE.md`, the guide (`REXBOX_DXT`).

## Verification

| Check | Result |
|---|---|
| Release build | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Hidden-window demo, `REXBOX_DXT=cpu` | `decoded on the CPU (REXBOX_DXT=cpu)`; frames 1500, 3000 and 4650 byte-identical to before |
| Hidden-window demo, default | `uploaded compressed (S3TC)`. Difference from before (largest channel / mean / pixels changed / over 8): 1500: 2 / 0.027 / 2.6% / 0; 3000: 4 / 0.253 / 24.2% / 0; 4650: 5 / 0.287 / 26.5% / 0. Rounding differs between the GPU and our decoder |
| Texture VRAM (settings menu, about 45 s after the first frame, 115 textures) | 12.8 MiB; the same 115 took 70.7 MiB with the CPU decode (i084 work log), about 5.5x less |
| User play (texture look, DXT1 holes, alpha) | the user's check: it works |
