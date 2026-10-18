# 작업 지시: SVM "모두 풀기" 루프를 스레드별 깊이로 (issue #100)

설계: [20261019-i100](../design/20261019-i100-svm-unlock-all.md)

1. `src/game/jsrf/decomp/adxm_lock.c`: `sub_001437B0` 대체(이 스레드의 깊이만큼 `SVM_Unlock`, 수를 돌려줌, 남은 TIME_CRITICAL은 내리고 한 번 보고).
2. `regenerate.py`, Release/Debug 빌드, 회귀 도구.
3. 검증: 숨긴 창 데모, 사용자 플레이.
4. 문서: 작업 로그, `ARCHITECTURE.md`.

---

# Work order: the SVM "release all" loop keyed on per-thread depth (issue #100)

Design: [20261019-i100](../design/20261019-i100-svm-unlock-all.md)

1. `src/game/jsrf/decomp/adxm_lock.c`: the `sub_001437B0` replacement (`SVM_Unlock` as many times as this thread's depth, returning the count, lowering a leftover TIME_CRITICAL with one report).
2. `regenerate.py`, Release/Debug builds, regression tools.
3. Verification: hidden-window demo, user play.
4. Documents: work log, `ARCHITECTURE.md`.
