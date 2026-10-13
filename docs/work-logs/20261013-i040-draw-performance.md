# Issue #40 작업 로그 — D3D8 그리기 경로의 비용 줄이기

작업 지시: [20261013-i040](../work-orders/20261013-i040-draw-performance.md) · 설계: [20261013-i040](../design/20261013-i040-draw-performance.md)

## 요약

오프닝 3D 데모의 프레임 시간을 단계별로 재는 진단(`REXBOX_D3D8_PROFILE`)을 더했다. 시간의 대부분은 그리기마다 같은 GL 상태, 상수, 정점·인덱스를 다시 보내는 데 쓰이고 있었다.

다음을 더했다.

* GL 상태 캐시: 프로그램, 유니폼, 샘플러·텍스처, 고정 상태
* 구조체 프로그램 키
* 게스트 바이트 비교로 정점·인덱스를 다시 쓰는 버퍼 캐시

결과: 그리기 483개 구간이 프레임당 120.8 ms에서 35.3 ms로 줄었다(초당 8.3 → 28.3프레임). 화면은 v0.0.15와 픽셀 단위로 같다.

## 과정

1. **계측**: `d3d8_profile`을 만들고 그리기 경로에 단계 표시를 넣었다. 처음에는 bind 한 단계가 32~58 ms로 가장 컸다. 이를 target, attributes, constants, textures로 나눴다.
2. **기준선**(설계 §1): 2,701~3,000프레임에서 constants 26.2 ms, textures 31.0 ms, upload 15.5 ms, setup 13.0 ms였다. GPU는 Intel HD Graphics 620(OpenGL 3.3)이고, GL 호출 하나하나가 비싸다.
3. **GL 상태 캐시**:
   * `InvalidateGlStateCache`/`GlStateEpoch`를 만들었다.
   * 프로그램을 바꿀 때만 `glUseProgram`을 부른다.
   * 유니폼은 프로그램별 `UniformCache`에 두고, `c[]`는 바뀐 원소 구간만 보낸다.
   * 샘플러는 매개변수 조합마다 객체를 두고, 유닛마다 바인딩을 기억한다.
   * 고정 상태는 바뀐 묶음만 보낸다.
   * 무효화 지점: 디바이스 생성, `Clear`, present, 렌더 타깃 텍스처 생성, 링크, 텍스처 업로드·삭제.
   * 결과: constants 26.2 → 1.6 ms, textures 31.0 → 0.5 ms, state 3.7 → 0.5 ms. 프레임 시간은 120.8 → 66.5 ms.
4. **프로그램 키**: 문자열 키를 필드별 `==`와 64비트 해시로 바꿨다. 그리기마다 만들던 trace 문자열은 trace가 켜졌을 때만 만든다. setup이 11.5 → 3.5 ms로 줄었다.
5. **스트리밍 링 실험**(측정으로 결정):
   * `glMapBufferRange(UNSYNCHRONIZED)`: upload가 24 → 62 ms로 늘어 버렸다.
   * `glBufferSubData` 링: 그리기마다 `glBufferData`와 비슷했다(약 23 ms).
   * 프레임당 약 3.8 MB를 보내는 데 드라이버 동기화가 시간을 쓰고 있었다. 그래서 보내는 양 자체를 줄이기로 했다.
6. **정점 캐시**(`d3d8_gl_buffers`):
   * 정점 버퍼 범위마다 GL 버퍼와 게스트 바이트 사본을 두고, 그리기마다 `memcmp`한다.
   * 업로드가 프레임당 수 KiB로 줄었다. 프레임 시간은 57.6 → 45.0 ms.
7. **인덱스 캐시**:
   * `DrawIndexedVertices`의 인덱스를 같은 방식으로 캐시하고, 변환 결과도 다시 쓴다.
   * 인덱스 없는 그리기는 `glDrawArrays`로 그린다. UP 데이터와 만든 인덱스는 스트리밍 버퍼에 담는다.
   * indices가 11.0 → 0.9 ms로 줄었다. 프레임 시간은 45.0 → 35.3 ms.
   * `IndexData`라는 이름은 라이브러리 전역 접근 함수와 겹쳐서 `DrawIndices`로 지었다.
8. **화면 확인**: 숨김 창에서 2,800, 3,000, 3,200, 3,400프레임을 덤프했다. v0.0.15 때의 덤프와 차이가 나는 픽셀이 0개였다. 데모는 프레임 번호로 결정되므로 같은 번호끼리 비교할 수 있다.

## 측정 (같은 프레임 구간, Release, 숨김 창, 오디오 없음)

| 프레임 | 그리기 | 이전 | 이후 |
|---|---|---|---|
| 2,401~2,700 | 459 | 125.3 ms (8.0 fps) | 46.7 ms (21.4 fps) |
| 2,701~3,000 | 483 | 120.8 ms (8.3 fps) | 35.3 ms (28.3 fps) |
| 3,001~3,300 | 372 | 96.9 ms (10.3 fps) | 34.5 ms (28.9 fps) |
| 3,301~3,600 | 277 | 73.7 ms (13.6 fps) | 25.6 ms (39.1 fps) |
| 3,601~3,900 | 321 | 86.5 ms (11.6 fps) | 32.5 ms (30.8 fps) |

* 2,401~2,700 구간의 setup(14.5 ms)에는 처음 쓰이는 셰이더의 컴파일이 들어 있다.
* 2,701~3,000 구간의 이후 내역(ms/프레임): indices 0.91, setup 3.62, target 0.63, upload 2.12, attributes 1.44, constants 1.57, textures 0.45, state 0.49, draw 0.26, present 11.15, other 12.69.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| 프레임 2,800 / 3,000 / 3,200 / 3,400 덤프 | v0.0.15와 다른 픽셀 0개 |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 116/116, 48/48 |
| `rejsrf_d3d8_check` | 14,018/0 |

## 남은 일

* 그리기 경로는 그리기 483개에 약 11.5 ms다. 남은 큰 몫은 그리기 경로 밖에 있다.
  * other 약 12.7 ms: 게임 코드와 다른 HLE 호출
  * present 약 11 ms: 대부분 다음 vblank까지의 대기
* 60프레임에 가려면 other를 따로 재야 한다. 게임 코드인지 상태 설정 HLE인지 나눠 봐야 한다. 별도 이슈로 다룬다.
* 처음 쓰이는 셰이더의 컴파일로 끊김이 생긴다(첫 데모 구간 setup 14.5 ms).
* 정점 범위 비교에 프레임당 약 2 ms가 든다.

---

# Issue #40 work log — cutting the cost of the D3D8 draw path

Work order: [20261013-i040](../work-orders/20261013-i040-draw-performance.md) · Design: [20261013-i040](../design/20261013-i040-draw-performance.md)

## Summary

A diagnostic (`REXBOX_D3D8_PROFILE`) now times the opening 3D demo's frames by phase. Most of the time went into sending the same GL state, constants, vertices and indices again for every draw.

The changes:

* a GL state cache for the program, uniforms, samplers and textures, and fixed state
* struct program keys
* buffer caches that reuse vertices and indices while the guest bytes stay the same

Result: the window with 483 draws went from 120.8 to 35.3 ms per frame (8.3 → 28.3 frames per second), and the picture is identical to v0.0.15 pixel for pixel.

## Process

1. **Instrumentation**: `d3d8_profile` and phase marks in the draw path. At first, one bind phase was the largest at 32–58 ms, so it was split into target, attributes, constants and textures.
2. **Baseline** (design §1): in frames 2,701–3,000, constants took 26.2 ms, textures 31.0, upload 15.5 and setup 13.0. The GPU is an Intel HD Graphics 620 (OpenGL 3.3), where every GL call is expensive.
3. **GL state cache**:
   * `InvalidateGlStateCache`/`GlStateEpoch`.
   * `glUseProgram` only on a change.
   * Per-program `UniformCache`; `c[]` sends only runs of changed elements.
   * One sampler object per parameter combination, and per-unit bindings.
   * Only changed groups of fixed state are sent.
   * Invalidation at device creation, `Clear`, present, render-target texture creation, links, and texture uploads and deletes.
   * Result: constants 26.2 → 1.6 ms, textures 31.0 → 0.5, state 3.7 → 0.5; the frame went from 120.8 to 66.5 ms.
4. **Program keys**: string keys became field-wise `==` with a 64-bit hash, and the per-draw trace strings are built only while tracing. Setup went from 11.5 to 3.5 ms.
5. **Streaming-ring experiments** (decided by measurement):
   * `glMapBufferRange(UNSYNCHRONIZED)` made upload worse (24 → 62 ms).
   * A `glBufferSubData` ring was about the same as `glBufferData` per draw (about 23 ms).
   * Sending about 3.8 MB per frame was spent in driver synchronization, so the amount sent had to shrink.
6. **Vertex cache** (`d3d8_gl_buffers`):
   * Each vertex-buffer range keeps a GL buffer and a copy of the guest bytes, compared with `memcmp` at every draw.
   * Uploads fell to a few KiB per frame, and the frame went from 57.6 to 45.0 ms.
7. **Index cache**:
   * The indices of `DrawIndexedVertices` are cached the same way, with their conversion result.
   * Draws without indices use `glDrawArrays`; UP data and generated indices go to streaming buffers.
   * Indices went from 11.0 to 0.9 ms, and the frame from 45.0 to 35.3 ms.
   * The struct is named `DrawIndices` because `IndexData` is already the accessor of a library global.
8. **Picture check**: frames 2,800, 3,000, 3,200 and 3,400 dumped from a hidden window differ from the v0.0.15 dumps in 0 pixels. The demo is determined by the frame number, so equal numbers can be compared.

## Measurements (same frame windows, Release, hidden window, no audio)

The table in the Korean section applies: frames 2,401–2,700 (459 draws) 125.3 → 46.7 ms; 2,701–3,000 (483) 120.8 → 35.3 ms; 3,001–3,300 (372) 96.9 → 34.5 ms; 3,301–3,600 (277) 73.7 → 25.6 ms; 3,601–3,900 (321) 86.5 → 32.5 ms.

* Setup in frames 2,401–2,700 (14.5 ms) includes compiling shaders on first use.
* Frames 2,701–3,000 now spend (ms per frame): indices 0.91, setup 3.62, target 0.63, upload 2.12, attributes 1.44, constants 1.57, textures 0.45, state 0.49, draw 0.26, present 11.15, other 12.69.

## Verification

The table in the Korean section applies: Debug and Release builds with zero warnings on our targets; frame dumps 2,800–3,400 with 0 pixels differing from v0.0.15; kernel probe 56/56, D3D8 probe 116/116, DSOUND probe 48/48; d3d8 check 14,018/0.

## Remaining

* The draw path takes about 11.5 ms for 483 draws. Most of what is left lies outside it.
  * other, about 12.7 ms: the title's code and the other HLE calls
  * present, about 11 ms: mostly the wait for the next vblank
* Reaching 60 frames per second needs other measured on its own, split into the title's code and the state-setting HLE. That is a separate issue.
* Compiling shaders on first use causes stutter (setup 14.5 ms in the first demo window).
* Comparing vertex ranges costs about 2 ms per frame.
