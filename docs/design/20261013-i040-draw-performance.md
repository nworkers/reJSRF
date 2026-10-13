# 설계: D3D8 그리기 경로의 비용 줄이기 (issue #40)

선행: [i023 고정 기능 그리기](20261012-i023-d3d8-fixed-function-draw.md), [i037 정점 셰이더 프로그램](20261013-i037-vertex-shader-programs.md)

## 1. 요구사항과 측정

* 정점 셰이더 프로그램을 그리게 된 뒤(#37) 오프닝 3D 데모가 Release에서 초당 8~10프레임이다.
* 한 프레임의 시간이 어디에 쓰이는지 재고, 큰 비용을 없앤 뒤 같은 프레임에서 전후를 비교한다.
* **측정 도구**: `REXBOX_D3D8_PROFILE=<n>`(진단)을 더한다. n번째 present마다 프레임당 밀리초를 한 줄로 낸다.
  * 그리기 단계: indices(인덱스 읽기·변환·업로드), setup(정점 형식·프로그램·텍스처 확인, 캐시 키), target(렌더 타깃), upload(정점 데이터), attributes, constants(프로그램과 유니폼), textures, state(고정 상태와 뷰포트), draw.
  * present: 프레임 덤프, vblank 맞춤 대기, 화면 내기, 창 이벤트.
  * other: 나머지. 게임 자신의 코드와 그리기가 아닌 HLE 호출이다.
  * 그리기 수와 GL 버퍼로 보낸 바이트도 센다.
  * GL은 비동기라서, 한 단계의 시간은 그 단계의 GL 호출에서 CPU가 기다린 시간도 포함한다.
* **기준선**(Release, 숨김 창, 오디오 없음; Intel HD Graphics 620, OpenGL 3.3, 드라이버 26.20.100.7325): 2,701~3,000프레임, 그리기 483개(그중 프로그램 205개).

| 단계 | ms/프레임 |
|---|---|
| indices | 3.85 |
| setup | 13.04 |
| target | 0.93 |
| upload | 15.54 |
| attributes | 1.43 |
| constants | 26.19 |
| textures | 30.97 |
| state | 3.74 |
| draw | 0.41 |
| present | 10.91 |
| other | 13.82 |
| **합계** | **120.8 (8.3 fps)** |

* **확인됨**(측정): 비용은 GL 호출 수에 비례한다. 매 그리기마다 다음을 다시 보냈다.
  * 정점 상수 192개(`glUniform4fv`)
  * 공유 샘플러 하나의 매개변수 5개(스테이지마다)
  * 고정 상태 전부
  * 정점 범위 전체(프레임당 약 3.8 MB)와 인덱스
  * 프로그램 캐시 키는 문자열이었다. 픽셀 셰이더 키는 약 500자다.

## 2. 설계

### 2.1 GL 상태 캐시

* 그리기 경로는 자기가 설정한 GL 상태를 기억하고, 같은 값이면 GL을 부르지 않는다.
* **무효화**: 그 상태를 그리기 경로 밖에서 바꾸는 코드는 `InvalidateGlStateCache()`를 부른다. 각 캐시는 자기가 채워진 시점의 `GlStateEpoch()`와 지금 값을 비교한다.
  * 부르는 곳: 디바이스 생성, `Clear`, present 뒤, 렌더 타깃 텍스처 생성, 프로그램 링크.
  * 텍스처 업로드·삭제는 텍스처 유닛의 기록만 지운다.
* **프로그램**: 이미 쓰는 프로그램이면 `glUseProgram`을 생략한다.
* **유니폼**(`UniformCache`): 유니폼은 프로그램의 상태라서 프로그램마다 마지막 값을 둔다. 프로그램을 바꿔도 기록은 유효하다.
  * 정점 상수 `c[]`는 원소마다 location을 링크 때 구해 둔다.
  * 바뀐 원소 중 location이 연속인 구간만 한 번에 보낸다.
* **텍스처**: 공유 샘플러의 매개변수를 매번 바꾸는 대신, 매개변수 조합(wrap S/T/R, mag, min)마다 샘플러 객체를 하나씩 둔다. 유닛마다 묶인 텍스처와 샘플러를 기억한다.
* **고정 상태**: 깊이, 블렌드, 컬링, 스텐실, 색 마스크, 폴리곤 오프셋, 채우기, 뷰포트, 깊이 범위를 구조체 하나로 만든다. 바뀐 묶음만 보낸다.

### 2.2 프로그램 키

* 고정 기능 키와 픽셀 셰이더 키는 필드별 `==`와 64비트 해시로 캐시를 찾는다. `Serialize()` 문자열은 로그와 덤프에만 쓴다.
* `REXBOX_D3D8_TRACE`가 꺼져 있으면 그리기마다 만들던 trace 문자열을 만들지 않는다.

### 2.3 정점과 인덱스 데이터 (`d3d8_gl_buffers`)

* **정점 범위**: 정점 버퍼에서 온 범위는 (주소, 바이트 수)마다 GL 버퍼와, 마지막 업로드 때의 게스트 바이트 사본을 둔다.
  * 그리기마다 게스트 바이트를 사본과 `memcmp`한다. 같으면 GL 버퍼를 그대로 쓴다.
  * 다르면 `glBufferData`로 저장소를 다시 지정한다. 이미 제출된 그리기는 이전 내용을 쓴다.
  * 표본 해시가 아니라 전체를 비교하므로 어떤 쓰기도 놓치지 않는다. 대가는 비교 시간이다(데모에서 프레임당 약 3.8 MB, 2 ms).
* **인덱스**: `DrawIndexedVertices`의 인덱스는 (주소, 개수, base, QUADLIST 여부)마다 같은 방식으로 둔다. 원본 16비트 인덱스가 같으면 변환 결과(최소 정점, 개수, 16/32비트)와 GL 버퍼를 다시 쓴다.
* **스트리밍 버퍼**: 그리기와 함께 넘어온 데이터(UP 호출)와 만든 인덱스(인덱스 없는 QUADLIST)를 담는다.
  * 앞 데이터 뒤에 `glBufferSubData`로 붙인다.
  * 가득 차면 버퍼를 orphan하고 0부터 다시 채운다(처음 4 MiB).
* 인덱스 없는 그리기(QUADLIST 제외)는 인덱스 없이 `glDrawArrays`로 그린다.
* 600 present 동안 쓰이지 않은 캐시 범위는 지운다.
* **버린 대안**(측정):
  * `glMapBufferRange(UNSYNCHRONIZED)` 링: 이 드라이버에서 upload가 24 ms에서 62 ms로 늘었다.
  * 그리기마다 `glBufferData`: 링과 비슷했다(약 24 ms).
  * persistent mapping: GL 4.4/확장이라 GL 3.3·GLES 3.0 계약 밖이다.

### 2.4 한계

* 캐시 사본만큼 메모리를 더 쓴다. 쓰인 정점·인덱스 바이트와 같고, 데모에서 수 MB다.
* 그리기마다 다시 쓰이는 범위는 지금처럼 매번 업로드한다.
* 남은 프레임 시간은 그리기 경로 밖에 있다.
  * other: 게임 코드와 다른 HLE 호출.
  * present: 늦은 프레임은 다음 vblank에 나가므로 vblank 두세 번 단위가 된다. 하드웨어와 같은 동작이다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 같은 프레임 구간의 전후 측정 | 데모 구간 프레임 시간이 크게 줄어듦 |
| 프레임 2,800 / 3,000 / 3,200 / 3,400 덤프 | v0.0.15와 픽셀 단위로 같음 |
| Debug/Release, 회귀 도구 | 경고 0, 회귀 없음 |

---

# Design: cutting the cost of the D3D8 draw path (issue #40)

Prerequisites: [i023 fixed-function drawing](20261012-i023-d3d8-fixed-function-draw.md), [i037 vertex-shader programs](20261013-i037-vertex-shader-programs.md)

## 1. Requirements and measurement

* Since vertex-shader programs are drawn (#37), the opening's 3D demo runs at 8–10 frames per second in Release.
* Measure where a frame's time goes, remove the largest costs, and compare before and after on the same frames.
* **Tool**: `REXBOX_D3D8_PROFILE=<n>` (diagnostic) prints milliseconds per frame every n presents.
  * Draw phases: indices (reading, converting and uploading indices), setup (vertex format, program and texture checks, cache keys), target (render target), upload (vertex data), attributes, constants (program and uniforms), textures, state (fixed state and viewport), draw.
  * present: frame dump, the wait for the vblank, presenting, window events.
  * other: the rest, which is the title's own code and the HLE calls that do not draw.
  * It also counts draws and the bytes sent to GL buffers.
  * GL is asynchronous, so a phase's time includes the CPU waiting inside its GL calls.
* **Baseline** (Release, hidden window, no audio; Intel HD Graphics 620, OpenGL 3.3, driver 26.20.100.7325): frames 2,701–3,000, 483 draws (205 with a program), 120.8 ms per frame (8.3 fps). The phase table in the Korean section applies: constants 26.19 ms, textures 30.97, upload 15.54, setup 13.04, other 13.82, present 10.91.
* **Confirmed** (measurement): the cost follows the number of GL calls. Every draw sent these again:
  * 192 vertex constants (`glUniform4fv`)
  * five parameters of one shared sampler per stage
  * all fixed state
  * the whole vertex range (about 3.8 MB per frame) and the indices
  * Program cache keys were strings; a pixel-shader key is about 500 characters.

## 2. Design

### 2.1 GL state cache

* The draw path remembers the GL state it set and makes no GL call for an unchanged value.
* **Invalidation**: code that changes that state outside the draw path calls `InvalidateGlStateCache()`, and each cache compares the `GlStateEpoch()` it was filled in.
  * Callers: device creation, `Clear`, after a present, render-target texture creation, program links.
  * Texture uploads and deletes forget only the texture-unit bindings.
* **Program**: `glUseProgram` is skipped for the program in use.
* **Uniforms** (`UniformCache`): uniforms are program state, so each program keeps the values it was last given, and they stay valid across program switches.
  * The vertex constants `c[]` have one location per element, queried at link time.
  * Runs of changed elements at consecutive locations go in one call.
* **Textures**: instead of re-setting a shared sampler's parameters per draw, each parameter combination (wrap S/T/R, mag, min) has its own sampler object. Each unit remembers its texture and sampler.
* **Fixed state**: depth, blend, cull, stencil, color mask, polygon offset, fill mode, viewport and depth range form one struct, and only the groups that changed are sent.

### 2.2 Program keys

* The fixed-function and pixel-shader keys find their programs by field-wise `==` and a 64-bit hash; the `Serialize()` strings remain for logs and dumps only.
* The per-draw trace strings are not built while `REXBOX_D3D8_TRACE` is off.

### 2.3 Vertex and index data (`d3d8_gl_buffers`)

* **Vertex ranges**: a range from a vertex buffer keeps, per (address, bytes), a GL buffer and a copy of the guest bytes at its last upload.
  * Every draw compares the guest bytes with the copy (`memcmp`); when equal, the GL buffer is reused.
  * Otherwise `glBufferData` respecifies the storage, and draws already submitted keep the old contents.
  * The whole range is compared, not a sampled hash, so no write is missed; the price is the comparison (about 3.8 MB and 2 ms per frame in the demo).
* **Indices**: the indices of `DrawIndexedVertices` are kept the same way per (address, count, base, quad list). When the guest's 16-bit indices are unchanged, the conversion result (lowest vertex, count, 16 or 32 bits) and its GL buffer are reused.
* **Streaming buffers** hold data passed with the draw (the UP calls) and generated indices (a quad list without indices).
  * Data is appended behind the previous data with `glBufferSubData`.
  * A full buffer is orphaned and filling restarts at 0 (4 MiB to begin with).
* A draw without indices (except a quad list) uses `glDrawArrays`.
* Cached ranges unused for 600 presents are deleted.
* **Rejected** (measured):
  * a `glMapBufferRange(UNSYNCHRONIZED)` ring: upload went from 24 to 62 ms on this driver.
  * `glBufferData` per draw: about the same as the `glBufferSubData` ring (about 24 ms).
  * persistent mapping: GL 4.4 or an extension, outside the GL 3.3 and GLES 3.0 contract.

### 2.4 Limits

* The cached copies cost memory equal to the vertex and index bytes in use, a few MB in the demo.
* A range rewritten for every draw is uploaded every time, as before.
* The remaining frame time lies outside the draw path.
  * other: the title's code and the other HLE calls.
  * present: a late frame is shown at the next vblank, so frames take two or three vblanks, as on the hardware.

## 3. Verification

The table above applies: before and after on the same frames, with the demo's frame time clearly lower; dumps of frames 2,800, 3,000, 3,200 and 3,400 identical to v0.0.15 pixel for pixel; Debug and Release with zero warnings and no regressions in the tools.
