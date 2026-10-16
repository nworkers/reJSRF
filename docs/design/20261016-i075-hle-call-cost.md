# 설계: HLE 호출마다 드는 고정 비용 줄이기 (issue #75)

## 1. 문제

* 데모 구간(프레임 2,700~3,300) present 스레드 샘플링(v0.0.31, Release)에서 HLE 호출 경로가 대기를 뺀 CPU의 약 9.1%였다(**확인됨**).
* 항목별 샘플 수:

| 항목 | 샘플 | 원인 |
|---|---|---|
| `RtlQueryPerformanceCounter` + `_Query_perf_frequency` | 103 | `NoteHleEntry`(정지 보고 i060)가 호출마다 `steady_clock::now()` |
| `gpr_slot` + `rexbox_recomp_get/set_gpr` | 125 | 레지스터 접근이 TLS 칸의 주소를 따로 함수로 구함 |
| `RecompCpu::Get/Set`, `HleCall` 생성·`Arg`·`Return`, `NoteHleEntry` | 나머지 | `Arg`가 인자마다 esp를 다시 읽음(가상 호출 → C 함수 → `switch`) |

* 정지 보고는 초 단위 경과만 쓴다("마지막 HLE 2.0초 전").

## 2. 설계

* **거친 시계**: `thread_activity`가 `g_coarse_now_ns`를 둔다.
  * 정지 감시 스레드가 0.1초마다 `NowNs()`로 갱신한다. 정지 판단은 지금처럼 1초마다다.
  * `NoteHleEntry`는 그 값을 relaxed로 읽어 기록만 한다.
  * 감시 스레드가 없으면(`REXBOX_STALL_REPORT=0`) 값은 0이다. 그때는 이 기록을 읽는 보고도 없다.
  * 대기 시작(`NoteWaitBegin`)은 호출이 드물어 정확한 시계를 그대로 쓴다.
* **esp 한 번 읽기**: `HleCall`이 생성할 때 esp를 읽어 두고 `Arg`가 그 값을 쓴다.
  * HLE 코드는 `Return` 전에 esp를 바꾸지 않는다(207곳 확인).
  * 중간에 게스트 콜백이 있어도 ABI상 esp는 돌아온다.
  * `Return`은 지금처럼 그때의 esp를 읽는다.
* **레지스터 직접 `switch`**: `rexbox_recomp_get_gpr`/`set_gpr`이 레지스터 주소를 돌려주는 `gpr_slot`을 거치지 않고, 레지스터를 직접 읽고 쓰는 `switch`가 된다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 샘플링 | HLE 경로 비중이 줄어듦. `QueryPerformanceCounter`가 거의 사라짐 |
| 정지 보고 | 임시 코드로 정지를 만들면 마지막 HLE와 경과 시간이 맞게 보고됨 |
| 회귀 | Debug·Release, probe 3종, `rejsrf_d3d8_check`, `rejsrf_decomp_check`, `check_hle_pops.py` |

---

# Design: trim the fixed cost of every HLE call (issue #75)

## 1. Problem

* Sampling the presenting thread over demo frames 2,700–3,300 (v0.0.31, Release) put the HLE call path at about 9.1% of non-wait CPU (**confirmed**).
* By item:
  * `RtlQueryPerformanceCounter` + `_Query_perf_frequency`, 103 samples: `NoteHleEntry` (the i060 stall report) calls `steady_clock::now()` on every call.
  * `gpr_slot` + `rexbox_recomp_get/set_gpr`, 125 samples: register access goes through a separate function that returns the thread-local slot's address.
  * The rest: `RecompCpu::Get/Set`, `HleCall` construction, `Arg` and `Return`, and `NoteHleEntry`. `Arg` re-reads esp for every argument (a virtual call, a C function, a `switch`).
* The stall report only needs elapsed seconds ("last HLE 2.0 s ago").

## 2. Design

* **Coarse clock**: `thread_activity` keeps `g_coarse_now_ns`.
  * The stall monitor refreshes it from `NowNs()` every 0.1 s. The stall check stays once a second.
  * `NoteHleEntry` only reads it (relaxed) and stores it.
  * Without a monitor (`REXBOX_STALL_REPORT=0`) it stays 0, and then no report reads those times either.
  * Wait begins (`NoteWaitBegin`) are rare and keep the precise clock.
* **esp read once**: `HleCall` reads esp when it is made, and `Arg` uses that value.
  * HLE code never moves esp before `Return` (all 207 uses checked).
  * A guest callback in between returns with esp restored, by the ABI.
  * `Return` still reads esp at that moment.
* **Direct register switch**: `rexbox_recomp_get_gpr`/`set_gpr` read and write the register in a `switch`, without `gpr_slot` returning its address.

## 3. Verification

| Check | Expected |
|---|---|
| Sampling | the HLE path's share drops; `QueryPerformanceCounter` almost disappears |
| Stall report | a stall provoked by temporary code reports the last HLE and its age correctly |
| Regression | Debug and Release, the three probes, `rejsrf_d3d8_check`, `rejsrf_decomp_check`, `check_hle_pops.py` |
