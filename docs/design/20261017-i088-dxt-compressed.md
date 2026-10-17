# 설계: DXT 텍스처를 압축된 채로 올리기 (issue #88)

관련: [i023 고정 기능 그리기](20261012-i023-d3d8-fixed-function-draw.md)(텍스처 디코드)

## 1. 문제

DXT1/DXT3/DXT5(Xbox 형식 0x0C/0x0E/0x0F) 텍스처를 올릴 때 CPU가 4×4 블록을 RGBA8로 푼다(`DecodeTextureLevel`). 그리고 `glTexImage2D`로 올린다.

* VRAM: DXT1은 4배, DXT3/5는 2배 더 쓴다(RGBA8은 텍셀당 4바이트, DXT1은 0.5, DXT3/5는 1). 설정 메뉴로 보면 이 게임의 텍스처 대부분이 DXT다(데모에서 텍스처 115개 70.7 MiB).
* 시간: 텍스처를 처음 올리거나 내용이 바뀔 때마다 디코드한다.
* 정확도: 원본 NV2A는 하드웨어로 DXT를 푼다. 호스트 GPU의 S3TC 디코드가 그쪽에 더 가깝다.

## 2. 설계

* **조건**: `GL_EXT_texture_compression_s3tc`가 있고(GL 3.3은 `glGetStringi`로 확장을 센다), 대상이 2D 텍스처일 때 DXT 레벨을 압축된 채로 올린다.
  * 볼륨(3D) DXT는 호스트 지원이 드물어 지금처럼 CPU에서 푼다.
* **업로드**: 레벨마다 `glCompressedTexImage2D(target, level, internal, width, height, 0, bytes, guest_pointer)`.
  * Xbox의 DXT 데이터는 스위즐 없이 블록이 행 순서로 놓여 있다. 그래서 게스트 메모리를 그대로 넘긴다.
  * `bytes`는 블록 단위로 올림한 크기(`LevelBytes`)다. 4×4보다 작은 밉 레벨도 블록 하나다(확장이 허용한다).
* **내부 형식**: DXT1 → `COMPRESSED_RGBA_S3TC_DXT1_EXT`(0x83F1). c0 ≤ c1이면 인덱스 3이 투명한 검정이고, Xbox D3D와 같다. DXT3 → `0x83F2`, DXT5 → `0x83F3`. DXT2/DXT4(미리 곱한 알파)는 Xbox에서 DXT3/DXT5와 같은 형식 값을 쓰고 블렌딩이 다룬다. 따라서 따로 처리하지 않는다.
* **대체 경로**: 확장이 없거나(일부 GLES, WebGL), 3D이거나, `REXBOX_DXT=cpu`이면 지금의 CPU 디코드를 쓴다. 처음 한 번 어느 경로인지 보고한다(`[d3d8] DXT textures: uploaded compressed (S3TC)` 또는 `decoded on the CPU (...)`).
* **메모리 집계**: 압축 레벨은 올린 바이트 수를 센다(설정 메뉴의 Textures 값이 실제와 맞는다).
* 샘플링, 필터, 해시 비교, 렌더 타깃 경로는 바뀌지 않는다.

### 2.1 하지 않는 것

* 큐브 맵(아직 올리지 않는다), 볼륨 DXT의 압축 업로드.
* GLES·WebGL용 다른 확장 이름(`WEBGL_compressed_texture_s3tc` 등). 그 플랫폼을 시작할 때 다룬다.

## 3. 검증

* Release·Debug 빌드, 회귀 도구.
* 데모: 프레임 BMP는 바이트 단위로 같지 않을 수 있다(GPU와 우리 디코더의 반올림 차이). 차이의 크기(최대·평균 채널 차)를 재고 눈으로 비교한다. `REXBOX_DXT=cpu`는 변경 전과 바이트 단위로 같아야 한다.
* 설정 메뉴의 Textures VRAM이 줄었는지.
* 사용자 플레이: 텍스처가 깨지거나 투명도(DXT1 구멍, DXT3/5 알파)가 달라지지 않았는지.

---

# Design: uploading DXT textures compressed (issue #88)

Related: [i023 fixed-function draw](20261012-i023-d3d8-fixed-function-draw.md) (texture decoding)

## 1. Problem

DXT1/DXT3/DXT5 textures (Xbox formats 0x0C/0x0E/0x0F) are decoded from 4×4 blocks to RGBA8 on the CPU at upload (`DecodeTextureLevel`) and then uploaded with `glTexImage2D`.

* Video memory: 4x more for DXT1 and 2x more for DXT3/5 (RGBA8 is 4 bytes per texel, DXT1 0.5 and DXT3/5 1). The settings menu shows most of this title's textures are DXT (115 textures, 70.7 MiB in the demo).
* Time: every first upload and every content change decodes.
* Accuracy: the NV2A decoded DXT in hardware; the host GPU's S3TC decode is closer to that.

## 2. Design

* **When**: with `GL_EXT_texture_compression_s3tc` present (GL 3.3 counts extensions through `glGetStringi`) and a 2D target, DXT levels go up compressed.
  * Volume (3D) DXT stays on the CPU decode, since hosts rarely support it.
* **Upload**: per level, `glCompressedTexImage2D(target, level, internal, width, height, 0, bytes, guest_pointer)`.
  * Xbox DXT data is unswizzled, blocks in row order, so guest memory is passed as it is.
  * `bytes` is the size rounded up to whole blocks (`LevelBytes`); mip levels smaller than 4×4 are one block (the extension allows it).
* **Internal formats**: DXT1 → `COMPRESSED_RGBA_S3TC_DXT1_EXT` (0x83F1): index 3 is transparent black when c0 ≤ c1, as in Xbox D3D. DXT3 → `0x83F2`, DXT5 → `0x83F3`. DXT2/DXT4 (premultiplied alpha) share the DXT3/DXT5 format values on the Xbox and are a matter of blending, so they need nothing extra.
* **Fallback**: without the extension (some GLES, WebGL), for 3D, or with `REXBOX_DXT=cpu`, the current CPU decode is used. Which path is reported once (`[d3d8] DXT textures: uploaded compressed (S3TC)` or `decoded on the CPU (...)`).
* **Memory accounting**: compressed levels count their uploaded bytes (the settings menu's Textures figure stays true).
* Sampling, filters, hash checks and the render-target paths do not change.

### 2.1 Not done

* Cube maps (not uploaded yet) and compressed upload of volume DXT.
* Other extension names for GLES and WebGL (`WEBGL_compressed_texture_s3tc` and so on); handled when those platforms start.

## 3. Verification

* Release and Debug builds, regression tools.
* Demo: frame BMPs may not be byte-identical (rounding differs between the GPU and our decoder); measure the difference (largest and mean channel difference) and compare by eye. `REXBOX_DXT=cpu` must be byte-identical to before.
* The settings menu's Textures VRAM goes down.
* User play: no broken textures, and no change in transparency (DXT1 holes, DXT3/5 alpha).
