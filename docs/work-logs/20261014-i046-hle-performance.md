# 작업 로그: 저사양 호스트를 위한 HLE 비용 줄이기 (issue #46)

설계: [20261014-i046](../design/20261014-i046-hle-performance.md) · 작업 지시: [20261014-i046](../work-orders/20261014-i046-hle-performance.md)

환경: Ryzen 5 5600X(6코어 12스레드), RTX 4090, Win32 x86 Release(SSE2 XMM 끔), 숨긴 창, 오디오 없음.

## 한 일

1. **기준 측정**:
   * 프로필에 `compared` 열(비교한 바이트, 범위 수)을 더했다.
   * 임시 계측으로 범위 크기 분포를 쟀다(설계 §2 표).
   * 작은 벤치마크로 `memcmp`와 `GetWriteWatch`의 비용을 쟀다.
   * 기준 실행 파일을 보관했다.
2. **플랫폼 계약**: `AllocateVirtualMemory(..., track_writes)`와 `TakeWrittenPages`를 더했다(Win32 `MEM_WRITE_WATCH`, `GetWriteWatch`). `PageSize`는 한 번만 묻는다.
3. **런타임**: `GuestMemory`가 추적을 켜고, 페이지별 쓰기 버전과 `WriteVersion`/`TracksWrites`를 제공한다.
4. **D3D8**:
   * 8 KiB 이상 정점·인덱스 범위는 쓰기 버전이 같으면 비교하지 않는다.
   * 정점 셰이더 상수 버전으로 `SetArray4`의 비교를 건너뛴다.
   * `BindRenderTarget`가 마지막으로 찾은 쌍을 기억한다.
   * `FirstReport`에 스레드별 표를 둔다.
   * `SetGammaRamp`의 trace 문자열은 trace가 켜졌을 때만 만든다.
5. **recomp 브리지**: `FirstTimeAt`에 스레드별 64칸 표를 둔다.
6. **텍스처 표본 해시에 쓰기 버전**: 구현하고 쟀다. 조회 비용이 줄어든 해시 비용보다 커서 되돌렸다(설계 §3.2).

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드, Debug 빌드(바뀐 라이브러리와 probe) | 경고 0 |
| kernel / D3D8 / DSOUND probe | 56/0, 116/0, 48/0 (Debug에서도 kernel·D3D8 통과) |
| decomp check, d3d8 check | 6/0, 14,018/0 |
| 로고 구간 프레임 300~1,800(6장) | 기준과 바이트 단위로 같음 |
| 데모 프레임 2,800~3,400 | 같은 장면, 카메라 위치가 조금 다름(아래) |
| 프로필 `compared`(483 그리기 구간) | 4,315 KiB/962개 → 1,387 KiB/856개 |
| 프로필 upload 단계 | 0.93 → 0.35 ms |

**CPU 시간**: present 스레드 ms/프레임, 기준과 새 빌드를 번갈아 3회 실행했다.

| 구간(그리기 수) | 기준 평균 | 새 빌드 평균 |
|---|---|---|
| w10 (483) | 5.42 | 4.83 |
| w11 (372) | 5.19 | 4.65 |
| w12 (277) | 3.93 | 3.58 |
| w13 (321) | 4.85 | 4.13 |
| 합 | 19.38 | 17.19 (-11%) |

* **데모 프레임 덤프**:
  * 첫 구현(텍스처 쓰기 버전 포함)에서는 2,800~3,400이 기준과 바이트 단위로 같았다.
  * 최종 빌드에서는 같은 장면이지만 카메라가 조금 달랐다(택시 위치).
  * 기준끼리는 두 번 실행해도 같았다. 그래서 차이는 빌드 사이에서만 난다.
  * 로고 구간은 같다. 마지막 단계의 변경은 보고·trace 경로뿐이고 그리기에 영향이 없다.
  * 따라서 프레임이 빨라져 게임 진행량이 달라진 것으로 본다(**추정**, [frame-time §5](../analysis/frame-time.md)와 같은 현상).
* **샘플**(최종 빌드, 잠자기 제외): `Unchanged` 87 → 40~65, `ZwGetWriteWatch` 25. 샘플 간격이 약 2 ms라 함수 단위 비교는 흔들림이 크다. 결론은 번갈아 잰 CPU 시간으로 낸다.

## 남은 것

* 저사양 기계에서 직접 재지 못했다. i5-7200U에서는 비교 비용이 1.3 ms였으므로(frame-time §2) 이득이 더 클 것으로 본다(**추정**).
* 남은 큰 몫은 lift 코드(약 41%)다. HLE 쪽에서 다음 후보는 다음과 같다.
  * 그리기마다의 GL 호출 수(정점 상수 uniform 전송, 상태)
  * 프로그램 키 조회
  * 작은 범위(8 KiB 미만, 약 1.3 MB/프레임)의 비교

---

# Work log: cheaper HLE work for low-end hosts (issue #46)

Design: [20261014-i046](../design/20261014-i046-hle-performance.md) · Work order: [20261014-i046](../work-orders/20261014-i046-hle-performance.md)

Environment: Ryzen 5 5600X (6 cores, 12 threads), RTX 4090, Win32 x86 Release (SSE2 XMM off), hidden window, no audio.

## Done

1. **Baseline**:
   * Added a `compared` column (bytes and ranges compared) to the profile.
   * Measured the range-size distribution with temporary instrumentation (design §2 table).
   * Measured `memcmp` and `GetWriteWatch` costs with a small benchmark.
   * Kept the baseline executable.
2. **Platform contract**: added `AllocateVirtualMemory(..., track_writes)` and `TakeWrittenPages` (Win32 `MEM_WRITE_WATCH`, `GetWriteWatch`). `PageSize` asks the OS once.
3. **Runtime**: `GuestMemory` turns tracking on and provides per-page write versions with `WriteVersion`/`TracksWrites`.
4. **D3D8**:
   * Vertex and index ranges of 8 KiB or more are not compared while their write version is unchanged.
   * A vertex shader constant version lets `SetArray4` skip its comparison.
   * `BindRenderTarget` remembers the pair it resolved last.
   * `FirstReport` has a per-thread table.
   * `SetGammaRamp` builds its trace strings only when tracing.
5. **Recomp bridge**: `FirstTimeAt` has a per-thread 64-slot table.
6. **Write versions for the texture sample hash**: implemented and measured. The queries cost more than the hashing they saved, so it was reverted (design §3.2).

## Verification

The tables in the Korean section apply:

* Release build, and Debug build of the changed libraries and the probes: zero warnings.
* Probes: kernel 56/0, D3D8 116/0, DSOUND 48/0 (kernel and D3D8 pass in Debug too).
* Checks: decomp 6/0, d3d8 14,018/0.
* Logo frames 300–1,800 (six): byte-identical to the baseline.
* Profile `compared` (483-draw window): 4,315 KiB in 962 ranges → 1,387 KiB in 856 ranges.
* Profile upload phase: 0.93 → 0.35 ms.
* **CPU time** (presenting-thread ms per frame, baseline and new build run alternately three times): the sum of the four windows fell from 19.38 to 17.19 ms (-11%); the 483-draw window from 5.42 to 4.83 ms.
* **Demo frame dumps**:
  * With the first implementation (texture write versions included), frames 2,800–3,400 were byte-identical to the baseline.
  * The final build shows the same scene with a slightly different camera (the taxi's position).
  * Two baseline runs match each other, so the difference appears only between builds.
  * The logo frames match, and the last changes touched only the report and trace paths, which do not affect drawing.
  * So a faster frame is taken to advance the game differently (**inferred**; the same effect as [frame-time §5](../analysis/frame-time.md)).
* **Samples** (final build, without sleeping): `Unchanged` 87 → 40–65, `ZwGetWriteWatch` 25. At one sample per about 2 ms, per-function comparisons are noisy; the conclusion rests on the alternating CPU measurement.

## Remaining

* Not measured on a low-end machine. On the i5-7200U the comparison cost 1.3 ms (frame-time §2), so the gain is expected to be larger there (**inferred**).
* The large remaining share is lifted code (about 41%). The next HLE candidates:
  * GL calls per draw (vertex constant uniform uploads, state)
  * program key lookups
  * comparing the small ranges (below 8 KiB, about 1.3 MB per frame)
