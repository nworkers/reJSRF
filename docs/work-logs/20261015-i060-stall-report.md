# Issue #60 작업 로그 — present가 멈췄을 때 게스트 스레드 상태 보고

작업 지시: [20261015-i060](../work-orders/20261015-i060-stall-report.md) · 설계: [20261015-i060](../design/20261015-i060-stall-report.md)

## 요약

present가 정해진 시간(기본 10초) 동안 없으면, 게스트 스레드마다 다음을 한 번 stderr에 쓰게 했다.

* 마지막 HLE 호출(이름, 호출한 곳, 경과 시간)
* 기다리는 중이면 대기 함수, 대상 객체와 그 객체의 **현재** 종류·신호 상태, 시한

present가 다시 오면 재개도 알린다. 런타임 공용 기능이며 기본으로 켠다(`REXBOX_STALL_REPORT=<초>`, `0`이면 끔).

## 과정

1. **구조 확인**:
   * XDK HLE는 모두 `rexbox_hle_enter`를, 커널 import는 `recomp_lookup_manual`을 지난다.
   * 커널 대기는 `WaitAndDeliver` 한 곳으로 모인다.
   * 그 밖에 따로 기다리는 곳이 다섯 군데 있다.
     * `KeDelayExecutionThread`
     * 임계 구역 경합(`RtlEnterCriticalSection`)
     * 자기 정지(`NtSuspendThread`)
     * 일시 정지로 만든 스레드의 시작
     * D3D8 수직 동기·present 간격
2. **런타임 `thread_activity`**:
   * 스레드마다 레코드를 하나 두고 한 번 전역 목록에 넣는다. 레코드는 해제하지 않는다.
   * HLE·대기·present 기록은 relaxed atomic 저장이다.
   * 감시 스레드는 1초마다 확인한다. 첫 present 전에는 감시하지 않고, 정지 하나에 한 번만 보고한다.
   * 대기 객체 설명은 커널이 등록한 함수가 보고할 때 메모리에서 읽는다(종류, 신호 상태).
3. **연결**:
   * `HleRegistry::LookupExport`(주소 → export와 이름)를 더했다. `Lookup`은 이것을 쓰게 바꿨다.
   * 커널: 스레드 이름 `thread <id> (<시작 함수>)`, `WaitNote`(RAII)
   * D3D8: `Swap`에서 `NotePresent`, 수직 동기 대기에서 `ScopedWait`
   * 앱: `StartStallMonitor`
4. **확인**:
   * 기본값 데모 60초: 보고 0건. present 스레드 CPU는 데모 구간 4.2~5.0 ms/프레임으로, 이전(4.4~5.4)과 흔들림 안이다.
   * 정지 유도(임시 코드, 커밋하지 않음): 프레임 1,500의 `Swap`에서 4초 쉬게 하고 `REXBOX_STALL_REPORT=2`로 실행했다.
     * 2.3초 뒤 보고가 나왔고, `presents resumed after 4.3 s`도 나왔다.
     * 첫 판에서는 모든 스레드가 "running"으로 나왔다. 대기 경로 다섯 곳(1의 목록)이 기록되지 않았기 때문이다. 그곳들에 기록을 더한 뒤에는 스레드 4·5가 `D3D8 vertical blank`, 스레드 6이 `NtSuspendThread (itself, until resumed)`로 나왔다.
     * 스레드 3은 HLE 호출 없이 27초째 게스트 코드를 실행 중으로 나왔다. 데모 중 HLE를 부르지 않고 도는 스레드가 있다는 뜻이다(**확인됨**, 정체는 **미확정**).
   * `REXBOX_STALL_REPORT=0`이면 보고가 없다.
   * 데모 프레임 6장:
     * 빌드 직후 두 번은 3,000·4,800이 i055 때부터 보던 다른 상태였다(타이밍 흔들림).
     * 이어진 세 번과 보고를 끈 한 번은 기준과 같았다.

## 검증

| 검증 | 결과 |
|---|---|
| 기본값 데모 | `[stall]` 0건, 프레임 시간 흔들림 안 |
| 정지 유도(2초) | 보고 한 번, 스레드별 상태, 재개 줄 |
| `REXBOX_STALL_REPORT=0` | 보고 없음 |
| 데모 프레임 6장 | 기준과 같음(빌드 직후 2회는 알려진 흔들림 상태) |
| Win32 Debug / Release 빌드 | 성공, 경고 0 |
| 회귀 도구(Debug, Release) | kernel 56/56, d3d8 116/116, dsound 48/48, d3d8 check 14,018/0, decomp check 6/0 |

## 남은 일

* 사용자가 멈춘 장면을 이 빌드로 재현해 `[stall]` 줄을 얻는다. 그 결과로 D3D8, 커널 타이머(`KeSetTimer`가 발화하지 않음), 스레드 문제 중 무엇을 고칠지 정한다.
* 데모 중 HLE를 부르지 않고 도는 스레드 3의 정체.

---

# Issue #60 work log — reporting guest threads when presents stop

Work order: [20261015-i060](../work-orders/20261015-i060-stall-report.md) · Design: [20261015-i060](../design/20261015-i060-stall-report.md)

## Summary

When no present comes for a set time (10 s by default), stderr now gets, once, for each guest thread:

* its last HLE call (name, caller, how long ago)
* when it is waiting: the wait function, the objects with their **current** kind and signal state, and the timeout

Presents resuming are reported too. It is a runtime feature, on by default (`REXBOX_STALL_REPORT=<seconds>`, `0` off).

## Process

1. **Structure**:
   * XDK HLE calls all pass `rexbox_hle_enter`, and kernel imports `recomp_lookup_manual`.
   * Kernel waits gather in `WaitAndDeliver`.
   * Five other places wait on their own:
     * `KeDelayExecutionThread`
     * critical-section contention (`RtlEnterCriticalSection`)
     * self-suspension (`NtSuspendThread`)
     * the start of a thread created suspended
     * D3D8's vertical blank and present interval
2. **Runtime `thread_activity`**:
   * One record per thread, put in a global list once and never freed.
   * HLE, wait and present notes are relaxed atomic stores.
   * The monitor checks every second, does not watch before the first present, and reports once per stall.
   * Wait objects are described, at report time, by a function the kernel registers that reads the object (kind, signal state).
3. **Wiring**:
   * `HleRegistry::LookupExport` (address → export and name); `Lookup` now uses it.
   * Kernel: thread names `thread <id> (<start function>)`, `WaitNote` (RAII).
   * D3D8: `NotePresent` in `Swap`, `ScopedWait` in the vertical-blank waits.
   * App: `StartStallMonitor`.
4. **Checks**:
   * Default, 60 s of demo: no reports. Presenting-thread CPU in the demo windows was 4.2–5.0 ms a frame, within the noise of before (4.4–5.4).
   * A provoked stall (temporary code, not committed): `Swap` slept 4 s at frame 1,500, run with `REXBOX_STALL_REPORT=2`.
     * The report came after 2.3 s, followed by `presents resumed after 4.3 s`.
     * The first version showed every thread as "running", because the five wait paths listed in 1 were not noted. With them noted, threads 4 and 5 showed `D3D8 vertical blank` and thread 6 `NtSuspendThread (itself, until resumed)`.
     * Thread 3 showed as running guest code with no HLE call for 27 s: a thread that runs without calling the HLE during the demo (**confirmed**; what it is, **unresolved**).
   * `REXBOX_STALL_REPORT=0`: no report.
   * Six demo frames:
     * Two runs right after a build gave, at 3,000 and 4,800, the other state seen since i055 (timing jitter).
     * The following three runs, and one with the report off, matched the baseline.

## Verification

* Default demo: no `[stall]` lines; frame time within the noise.
* Provoked stall (2 s): one report with each thread's state, then the resume line.
* `REXBOX_STALL_REPORT=0`: no report.
* Six demo frames: as the baseline (the two runs right after builds were the known jitter state).
* Win32 Debug and Release builds succeed with zero warnings.
* Regression tools in both configurations: kernel 56/56, D3D8 116/116, DSOUND 48/48, d3d8 check 14,018/0, decomp check 6/0.

## Remaining

* The user reproduces the hanging scene with this build to get the `[stall]` lines. They decide what to fix: D3D8, the kernel timer (`KeSetTimer` never fires), or threads.
* What thread 3, which runs without calling the HLE during the demo, is.
