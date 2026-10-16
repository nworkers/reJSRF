# Issue #75 작업 로그 — HLE 호출마다 드는 고정 비용 줄이기

작업 지시: [20261016-i075](../work-orders/20261016-i075-hle-call-cost.md) · 설계: [20261016-i075](../design/20261016-i075-hle-call-cost.md)

## 요약

HLE 호출마다 정확한 시계를 읽던 것을 감시 스레드의 거친 시계로 바꿨다. 인자를 읽을 때마다 esp를 다시 읽던 것과, 레지스터 접근의 함수 한 단계도 없앴다. 데모 구간에서 HLE 경로가 대기를 뺀 CPU에서 차지하는 비중이 9.1%에서 4.5%로 줄었다.

## 과정

1. **측정**: v0.0.31 Release 샘플(프레임 2,700~3,300)에서 HLE 경로 항목을 모으니 469개, 대기를 뺀 CPU의 9.1%였다.
   * 이 중 `QueryPerformanceCounter` 쪽이 103개였다. `NoteHleEntry`가 HLE 호출마다 `steady_clock::now()`를 불렀다.
   * 호출자 추정에서 `RtlQueryPerformanceCounter ← rexbox_recomp_set_gpr`로 나온 것도 같은 경로로 본다(**추정**).
2. **1단계**: 거친 시계와 `HleCall`의 esp 캐시. 305개, 5.5%다. `QueryPerformanceCounter`는 8개만 남았다.
3. **2단계**: `rexbox_recomp_get/set_gpr`을 레지스터 직접 `switch`로 바꿨다. 167개, 4.5%다.
4. **정지 보고 확인**(임시 코드, 커밋하지 않음): 프레임 1,500의 `Swap`에서 3초 쉬게 하고 `REXBOX_STALL_REPORT=1.5`로 실행했다.
   * `no present for 1.9 s`가 보고됐다. present 스레드는 `last HLE D3DDevice_Swap 2.0 s ago`, 다른 스레드는 `0.1 s ago`처럼 0.1초 단위로 맞게 나왔다.
   * 재개 보고 `presents resumed after 3.9 s`도 나왔다.
   * 임계값 0.3초로 45초 돌린 기본 실행에서는 보고가 없었다(그만큼 긴 정지가 없음).
5. **비교의 한계**: 실행마다 샘플 수가 다르다(대기를 뺀 샘플 5,177, 5,568, 3,725개). 그래서 비교는 비율로 했다. 전체 CPU 시간 A/B는 이 노트북에서 흔들림이 커서 쓰지 않았다(i073 참고).

## 확인

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug / Release 빌드 | 경고 0 |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 162/162, 48/48(Debug·Release) |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 14,018개 차이 0, 129개 차이 0(Debug·Release) |
| `check_hle_pops.py` | 불일치 0 |
| 정지 보고 | 유도한 정지에서 보고와 재개가 맞게 나옴 |

## 남은 것

* HLE 경로에 남은 것은 가상 호출(`GuestCpu`), 호출마다의 활동 기록, 진단 검사 두 개다. 대기를 뺀 CPU의 약 4.5%다.
* 런타임 계층이 recomp를 직접 알지 못하게 하는 구조(`GuestCpu` 인터페이스)는 그대로 두었다.

---

# Issue #75 work log: trim the fixed cost of every HLE call

Work order: [20261016-i075](../work-orders/20261016-i075-hle-call-cost.md) · Design: [20261016-i075](../design/20261016-i075-hle-call-cost.md)

## Summary

The precise clock read on every HLE call is now the monitor's coarse clock. The esp re-read for every argument and one function level in register access are gone. In the demo window the HLE path's share of non-wait CPU fell from 9.1% to 4.5%.

## Steps

1. **Measurement**: v0.0.31 Release samples (frames 2,700–3,300) put 469 samples, 9.1% of non-wait CPU, on the HLE path.
   * 103 of them were `QueryPerformanceCounter`: `NoteHleEntry` called `steady_clock::now()` on every HLE call.
   * The guessed caller `RtlQueryPerformanceCounter ← rexbox_recomp_set_gpr` is taken to be the same path (**inferred**).
2. **First step**: the coarse clock and the esp cache in `HleCall`. 305 samples, 5.5%. Only 8 `QueryPerformanceCounter` samples were left.
3. **Second step**: `rexbox_recomp_get/set_gpr` as a direct register `switch`. 167 samples, 4.5%.
4. **Stall report check** (temporary code, not committed): `Swap` slept 3 s at frame 1,500, run with `REXBOX_STALL_REPORT=1.5`.
   * The report said `no present for 1.9 s`. The presenting thread showed `last HLE D3DDevice_Swap 2.0 s ago` and the others `0.1 s ago`, correct to 0.1 s.
   * The resumption, `presents resumed after 3.9 s`, was reported too.
   * A default 45-second run with a 0.3 s threshold reported nothing (no stall that long).
5. **Limits of the comparison**: runs give different sample counts (5,177, 5,568 and 3,725 non-wait samples), so shares were compared. Total CPU time A/B was not used, being too noisy on this laptop (see i073).

## Verified

| Item | Result |
|---|---|
| Win32 x86 Debug / Release builds | no warnings |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 162/162, 48/48 (Debug and Release) |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 14,018 with 0 differences, 129 with 0 differences (Debug and Release) |
| `check_hle_pops.py` | 0 mismatches |
| Stall report | a provoked stall reports and resumes correctly |

## Left open

* What remains on the HLE path is the virtual call (`GuestCpu`), the activity note on every call and two diagnostic checks, about 4.5% of non-wait CPU.
* The structure that keeps the runtime layer from knowing recomp directly (the `GuestCpu` interface) is unchanged.
