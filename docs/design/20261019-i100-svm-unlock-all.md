# 설계: SVM "모두 풀기" 루프를 스레드별 깊이로 (issue #100)

## 1. 문제

* v0.0.41로 21:9 플레이 중 다시 멈췄다. 정지 보고(`rejsrf_log.txt`):
  * thread 2가 `running`, 마지막 HLE `ObReferenceObjectByHandle`이 방금, 기본 우선순위 16(TIME_CRITICAL). 스택 `0x001437BA`(`0x001437B0`의 루프) ← `0x0014076D`(`0x00140760`, wxCi 캐시 미스 → 디스크 열기). [i068](20261016-i068-adxm-lock.md)·[i091](20261018-i091-adxm-lock-priority.md)과 같은 자리.
  * thread 3(ADXM safe 스레드)은 suspend count 1, 우선순위 2 — SVM 잠금 횟수 0과 맞는다. 즉 thread 2는 **잠금을 하나도 갖지 않은 채 TIME_CRITICAL**이다.
  * i091의 `[adxm lock] ... restored` 보고가 없다. i091의 보정(마지막 unlock에서 자기 우선순위 복원)은 이번 경로로는 걸리지 않았다.
* `0x001437B0`(**확인됨**, 디스어셈블):

  ```c
  int n = 0;
  while (GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_TIME_CRITICAL) { SVM_Unlock(); n++; }  // 0x00141B80
  return n;   // 0x001437D0이 n번 SVM_Lock()
  ```

  "TIME_CRITICAL이면 잠금을 갖고 있다"는 단일 코어 전제다. 잠금 횟수가 0인 unlock은 횟수를 음수로 내릴 뿐 우선순위를 바꾸지 않으므로, 그 전제가 깨지면 루프는 끝나지 않는다.
* 우선순위가 남는 경로(**미확정**): lock/unlock 콜백의 엇갈림은 i091이 막지만, 호스트에서는 safe 스레드의 자기 정지(`NtSuspendThread`)가 unlock 본문 한가운데서 대기가 되고 그동안 잠금이 내려가는 등, 원본이 가정하지 않는 순서가 더 있다. 모든 경우를 열거하는 대신 루프의 종료 조건을 바꾼다.

## 2. 설계

* 어댑터(`src/game/jsrf/decomp/adxm_lock.c`)가 `0x001437B0`을 대체한다.
  * i091이 스레드마다 두는 잠금 깊이 `t_depth`만큼 `SVM_Unlock`(`0x00141B80`, 콜백 포인터를 거쳐 우리 unlock 어댑터로 간다)을 부르고 그 수를 돌려준다. 원본이 "자기가 가진 만큼" 풀려던 것과 같은 수다.
  * 그 뒤에도 TIME_CRITICAL이면 이 스레드의 저장된 우선순위로 내리고 한 번 보고한다(`[adxm lock] a thread was TIME_CRITICAL holding no lock; ... (issue #100)`). 남은 우선순위가 다음 잠금의 저장값으로 들어가는 것을 막는다.
* `0x001437D0`(n번 다시 잠금)은 그대로다.
* 호출자 다섯 곳(`0x00140134`, `0x00140145`, `0x001406D3`, `0x00140768`, `0x00140C88`)은 모두 wxCi 파일 경로다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 재생성 | `sub_001437B0_gen`이 생김 |
| Release/Debug 빌드, 회귀 도구 | 경고 0, 통과 |
| 숨긴 창 데모 | 정지·시한 보고 없음 |
| 사용자 플레이 | 멈추지 않음. 보고 줄이 나오면 그 경로가 실제로 있었다는 뜻 |

---

# Design: the SVM "release all" loop keyed on per-thread depth (issue #100)

## 1. Problem

* The game hung again in 21:9 play on v0.0.41. The stall report (`rejsrf_log.txt`):
  * Thread 2 `running`, last HLE `ObReferenceObjectByHandle` just now, base priority 16 (TIME_CRITICAL); stack `0x001437BA` (the loop in `0x001437B0`) ← `0x0014076D` (`0x00140760`, a wxCi cache miss opening from disk). The place of [i068](20261016-i068-adxm-lock.md) and [i091](20261018-i091-adxm-lock-priority.md).
  * Thread 3 (the ADXM safe thread) has suspend count 1 and priority 2, matching an SVM lock count of 0: thread 2 is **TIME_CRITICAL holding no lock**.
  * No `[adxm lock] ... restored` line from i091: its correction (restoring a thread's own priority at its last unlock) did not cover this path.
* `0x001437B0` (**confirmed**, disassembly):

  ```c
  int n = 0;
  while (GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_TIME_CRITICAL) { SVM_Unlock(); n++; }  // 0x00141B80
  return n;   // 0x001437D0 calls SVM_Lock() n times
  ```

  "TIME_CRITICAL means holding the lock" is single-core reasoning. An unlock at count 0 only makes the count negative and leaves the priority, so once the premise breaks the loop never ends.
* How the priority is left behind (**unconfirmed**): i091 covers the lock/unlock callbacks leaving out of order, but the host has more orders the original never assumed, such as the safe thread's self-suspend (`NtSuspendThread`) becoming a wait in the middle of the unlock body with the lock put down. Rather than enumerating them, the loop's exit condition changes.

## 2. Design

* The adapter (`src/game/jsrf/decomp/adxm_lock.c`) replaces `0x001437B0`.
  * It calls `SVM_Unlock` (`0x00141B80`, which reaches our unlock adapter through the callback pointer) as many times as this thread's depth `t_depth` from i091, and returns that count: the number the original meant to release, "what this thread holds".
  * If the thread is still TIME_CRITICAL afterwards, it is lowered to the thread's saved priority and reported once (`[adxm lock] a thread was TIME_CRITICAL holding no lock; ... (issue #100)`), so the leftover does not become the next lock's saved value.
* `0x001437D0` (re-lock n times) stays.
* The five callers (`0x00140134`, `0x00140145`, `0x001406D3`, `0x00140768`, `0x00140C88`) are all wxCi file paths.

## 3. Verification

| check | expected |
|---|---|
| regeneration | `sub_001437B0_gen` emitted |
| Release/Debug builds, regression tools | zero warnings, pass |
| hidden-window demo | no stall or timeout report |
| user play | no hang; a report line means the path really occurred |
