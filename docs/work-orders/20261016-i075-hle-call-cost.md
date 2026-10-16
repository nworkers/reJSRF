# 작업 지시: HLE 호출마다 드는 고정 비용 줄이기 (issue #75)

설계: [20261016-i075](../design/20261016-i075-hle-call-cost.md)

1. 샘플링으로 HLE 경로의 항목별 비용을 확인한다.
2. `thread_activity`: 거친 시계(감시 스레드가 0.1초마다 갱신), `NoteHleEntry`는 그 값을 기록한다.
3. `HleCall`: 생성할 때 esp를 한 번 읽는다.
4. `recomp_abi.c`: 레지스터 get/set을 직접 `switch`로 한다.
5. 다시 샘플링해 비교한다. 임시 코드로 정지를 만들어 정지 보고를 확인한다(커밋하지 않음). Debug/Release, 회귀 도구
6. 문서: frame-time 분석, 작업 로그

---

# Work order: trim the fixed cost of every HLE call (issue #75)

Design: [20261016-i075](../design/20261016-i075-hle-call-cost.md)

1. Measure the HLE path's costs by item with sampling.
2. `thread_activity`: a coarse clock the monitor refreshes every 0.1 s; `NoteHleEntry` records it.
3. `HleCall`: read esp once at construction.
4. `recomp_abi.c`: register get/set as a direct `switch`.
5. Sample again and compare; provoke a stall with temporary code (not committed) to check the stall report; Debug and Release, regression tools.
6. Documents: the frame-time analysis, the work log.
