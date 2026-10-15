# Issue #62 작업 로그 — Begin/End 즉시 모드 그리기

작업 지시: [20261015-i062](../work-orders/20261015-i062-immediate-mode.md) · 설계: [20261015-i062](../design/20261015-i062-immediate-mode.md)

## 요약

`Begin`/`SetVertexData2f`/`4f`/`End`로 모은 정점을 현재 FVF(또는 선언)의 배치로 묶어 기존 그리기 경로로 그리게 했다. 데모에서 이 호출은 스텐실 그림자 칠하기였고, 이제 고가 구조물 아래 지면에 그림자가 나온다. 프로파일 보고에는 미구현 경로의 프레임당 호출 수를 더했다.

## 과정

1. **미구현 빈도**: `profile::CountMissing`을 `ReportOncePer`에서 부르게 하고, 보고에 `unimplemented per frame:` 줄을 더했다. 데모 구간(프레임 2,700 이후)의 프레임당 횟수는 다음과 같았다.

   | 미구현 | 프레임당 |
   |---|---|
   | 텍스처 좌표 생성 `0x10000`(카메라 공간 법선) + 텍스처 변환(스테이지 0) | 10~39 그리기 |
   | `Begin`/`End` | 2(`SetVertexData` 약 20) |
   | 고정 기능 조명 | 0~14 그리기 |

   안개, bump, `CopyRects`, 큐브 맵은 이 구간에서 나오지 않았다. 그래서 이번에는 `Begin`/`End`를 맡았다. 호출 수는 적지만 장면 전체에 보이는 결과가 있고, 텍스처 좌표 생성과 조명은 별도 작업으로 둔다.
2. **호출 추적**(임시 코드, 커밋하지 않음):
   * 프레임마다 FVF `0x1C4` 삼각형 스트립 두 개가 나온다. 첫째는 퇴화, 둘째는 640×480 전체에 diffuse `(0,0,0,0.498)`이다.
   * 처음에는 화면 전체를 어둡게 하는 오버레이로 보았다.
3. **구현**:
   * `d3d8_immediate.h/.cpp`(순수): `ImmediateBatch`, `PackFvfVertices`, `PackDeclarationVertices`
   * `VertexSource::immediate`, `PrepareHostVertexData`
   * `rexbox_d3d8_probe`에 19개 검사(116 → 135)
4. **화면 확인 중의 의문**:
   * 2,750·2,900 프레임에서 하늘이 밝게 남아(최대 채널 192~217) 오버레이가 덮지 않는 것처럼 보였다.
   * XYZRHW z 단위 때문에 깊이 테스트에서 걸러진다고 의심했다. 그래서 z를 0으로 바꾸는 실험을 했지만 밝기는 같았다(평균 80.9 대 79.7).
   * 오버레이 그리기의 상태를 추적하니 스텐실이 켜져 있었다(func `LEQUAL`, ref 1). 앞선 그리기들은 섀도 볼륨이었다. 깊이 `GEQUAL`에 앞면 `INCR`·뒷면 `DECR`인 depth-fail 방식이다.
   * 곧 이 사각형은 그림자 안만 어둡게 하는 칠하기다. 설계 문서의 문제 설명을 이에 맞게 고쳤다.
5. **결과**: 같은 장면의 이전 기준 프레임(`frames2`, 2,800)과 비교했다.
   * 오른쪽 지면과 왼쪽 계단 아래 등 구조물 그림자 자리가 어두워졌다. 하늘과 로고는 그대로다.
   * 3,200 프레임은 거리 대부분이 그림자 안이라 위쪽 영역 평균이 51에서 31로 내려갔다.

## 확인

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug / Release 빌드 | 우리 코드 경고 0. 생성 코드 대상의 명령줄 경고 D9025는 기존과 같다 |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 135/135, 48/48(Debug·Release) |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 14,018 케이스 차이 0, 6 케이스 차이 0(Debug·Release) |
| `check_hle_pops.py` | 불일치 0 |
| 데모(Release, hidden) | `unimplemented per frame:`에 `begin` 없음. 2,800 프레임에 구조물 그림자 |

참고: 이 확인 중에 다른 세션의 빌드가 CPU를 100% 쓰고 있었다. 그래서 프레임 속도와 CPU 시간은 이번 작업에서 비교하지 않았다. 추가된 것은 프레임당 그리기 두 개(정점 8개)다.

## 남은 것

* XYZRHW z의 단위(0..1 깊이인지 깊이 버퍼 단위인지)는 확인하지 못했다(설계 §4).
* 다음 미구현 작업 후보: 텍스처 좌표 생성(카메라 공간 법선)과 텍스처 변환, 고정 기능 조명

---

# Issue #62 work log: Begin/End immediate-mode drawing

Work order: [20261015-i062](../work-orders/20261015-i062-immediate-mode.md) · Design: [20261015-i062](../design/20261015-i062-immediate-mode.md)

## Summary

Vertices gathered by `Begin`, `SetVertexData2f`/`4f` and `End` are now packed in the current FVF (or declaration) layout and drawn through the existing draw path. In the demo these calls are the stencil shadow fill, and shadows now show on the ground under the elevated structures. The profile report also gives calls per frame into unimplemented paths.

## Steps

1. **How often unimplemented paths run**: `ReportOncePer` now calls `profile::CountMissing`, and the report gained an `unimplemented per frame:` line. Per frame in the demo (frames from 2,700):

   | Unimplemented | Per frame |
   |---|---|
   | Texture coordinate generation `0x10000` (camera-space normal) with a texture transform (stage 0) | 10–39 draws |
   | `Begin`/`End` | 2 (about 20 `SetVertexData`) |
   | Fixed-function lighting | 0–14 draws |

   Fog, bump, `CopyRects` and cube maps did not appear in this window. `Begin`/`End` was taken first: few calls, but a result visible across the scene. Coordinate generation and lighting are separate tasks.
2. **Call trace** (temporary code, not committed):
   * Each frame draws two FVF `0x1C4` triangle strips: the first degenerate, the second covering 640×480 with diffuse `(0,0,0,0.498)`.
   * It was first read as a full-screen dimming overlay.
3. **Implementation**:
   * `d3d8_immediate.h/.cpp` (pure): `ImmediateBatch`, `PackFvfVertices`, `PackDeclarationVertices`
   * `VertexSource::immediate` and `PrepareHostVertexData`
   * 19 checks in `rexbox_d3d8_probe` (116 → 135)
4. **A doubt while checking the picture**:
   * At frames 2,750 and 2,900 the sky stayed bright (maximum channel 192–217), as if the overlay did not cover it.
   * The XYZRHW z unit was suspected of failing the depth test, so z was forced to 0 in an experiment. The brightness did not change (mean 80.9 against 79.7).
   * A trace of the overlay draw's state showed the stencil on (func `LEQUAL`, ref 1). The draws before it are shadow volumes in the depth-fail style: depth `GEQUAL`, `INCR` on front faces and `DECR` on back faces.
   * So the quad is a fill that darkens only inside the shadows. The design's problem statement was corrected to say so.
5. **Result**, compared with an earlier reference frame of the same scene (`frames2`, 2,800):
   * The places where structures cast shadows, such as the ground on the right and under the stairs on the left, are darker; the sky and the logo are unchanged.
   * At frame 3,200 most of the street is in shadow, and the upper region's mean fell from 51 to 31.

## Verified

| Item | Result |
|---|---|
| Win32 x86 Debug / Release builds | no warnings from our code; the generated-code target's command-line warning D9025 is as before |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 135/135, 48/48 (Debug and Release) |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 14,018 cases with 0 differences, 6 cases with 0 differences (Debug and Release) |
| `check_hle_pops.py` | 0 mismatches |
| Demo (Release, hidden) | no `begin` in `unimplemented per frame:`; structure shadows at frame 2,800 |

Note: another session's build held the CPU at 100% during these checks, so frame rate and CPU time were not compared in this task. The addition is two draws (8 vertices) per frame.

## Left open

* The unit of XYZRHW z (a 0..1 depth or depth-buffer units) is not confirmed (design §4).
* Next unimplemented work: texture coordinate generation (camera-space normal) with texture transforms, and fixed-function lighting.
