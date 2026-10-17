# 작업 지시: CRI ADXM 잠금의 우선순위를 스레드마다 되돌리기 (issue #91)

설계: [20261018-i091](../design/20261018-i091-adxm-lock-priority.md)

1. `src/game/jsrf/decomp/adxm_lock.c`: 스레드 지역 깊이와 저장 우선순위, lifted `GetThreadPriority`/`SetThreadPriority` 호출 도우미, lock 뒤 저장·unlock 뒤 되돌림.
2. 검증: Release/Debug 빌드, 회귀 도구, 숨긴 창 데모.
3. 문서: 작업 로그, i068 설계에 후속 링크.

---

# Work order: restoring the CRI ADXM lock's priority per thread (issue #91)

Design: [20261018-i091](../design/20261018-i091-adxm-lock-priority.md)

1. `src/game/jsrf/decomp/adxm_lock.c`: thread-local depth and saved priority, helpers calling the lifted `GetThreadPriority`/`SetThreadPriority`, saving at lock and restoring after unlock.
2. Verification: Release/Debug builds, regression tools, a hidden-window demo.
3. Documents: work log, a follow-up link in the i068 design.
