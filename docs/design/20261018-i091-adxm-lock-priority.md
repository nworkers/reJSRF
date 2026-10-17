# 설계: CRI ADXM 잠금의 우선순위를 스레드마다 되돌리기 (issue #91)

## 1. 문제

* 플레이 중 다시 멈췄다(v0.0.37 뒤 작업 브랜치). 정지 보고는 [i068](20261016-i068-adxm-lock.md)과 같은 모양이다.
  * thread 2가 `running`이고, 마지막 HLE `ObfDereferenceObject`가 방금 호출됐다. 기본 우선순위는 16(TIME_CRITICAL)이다.
  * 추정 스택: `0x001437BA`(SVM 잠금을 다 푸는 루프) ← `0x0014076D`(`0x00140760`) ← `0x0014090D`(`0x001408C0`) ← `0x0014097B`(`0x00140920`).
* 들어온 경로(**확인됨**, 디스어셈블과 XBE 문자열)
  * `0x00140920`은 `wxCiGetFileSize`다(`E0092901:fname is null.(wxCiGetFileSize)`).
  * 파일이 캐시에 없으면 `0x001408C0`(`E0109261:'%s' is not in cache.`)를 거쳐 `0x00140760`으로 간다.
  * `0x00140760`은 먼저 `0x001437B0`으로 SVM 잠금을 다 풀고, 디스크에서 직접 연다(`E0040201:can not open '%s'.`).
  * 캐시 미스 자체는 이 경로가 원래 처리하는 경우다. 멈춘 곳은 `0x001437B0`이다.
* 원인(**추정**): i068의 2.2.1로, 잠금을 가진 스레드는 대기하는 동안 호스트 잠금을 내려놓는다. 그 사이 다른 스레드가 들어와 횟수만 올리고, 나가는 순서가 엇갈릴 수 있다.

  | 순서 | 횟수 | 결과 |
  |---|---|---|
  | A 들어감 | 0 → 1 | A의 우선순위 P_A를 전역 하나에 저장, A는 TIME_CRITICAL |
  | A가 대기(내려놓음), B 들어감 | 1 → 2 | B는 그대로 |
  | B가 대기(내려놓음), A가 다시 잡고 나감 | 2 → 1 | 아무것도 되돌리지 않음. **A는 잠금 없이 TIME_CRITICAL** |
  | B가 다시 잡고 나감 | 1 → 0 | B가 P_A로 바뀜 |

  그 뒤 A가 `0x001437B0`에 들어가면, unlock이 횟수를 음수로만 내리고 우선순위는 그대로라 루프가 끝나지 않는다.
* Xbox에서는 TIME_CRITICAL 스레드가 대기에서 깨면 바로 선점하므로, 이런 엇갈림이 드물거나 없다(**추정**). 호스트에서는 깨어난 순서와 잠금을 다시 잡는 순서가 보장되지 않는다.

## 2. 설계

* 어댑터(`src/game/jsrf/decomp/adxm_lock.c`)가 "TIME_CRITICAL ⇔ 잠금을 가짐"을 **스레드마다** 지킨다. 원본 본문은 그대로 실행하고, 그 뒤에 보정만 더한다.
  * 스레드 지역 변수: 이 스레드의 잠금 깊이와, 가장 바깥 lock 때의 우선순위.
  * lock: 호스트 잠금을 잡고, 깊이가 0이었으면 지금 우선순위를 저장한 뒤 원본 lock을 실행한다.
  * unlock: 원본 unlock을 실행한다. 이 스레드의 깊이가 0이 되면, 우선순위가 저장값과 다를 때 저장값으로 되돌린다. 그다음 호스트 잠금을 놓는다.
  * 우선순위는 게임이 쓰는 XAPI 그대로 읽고 쓴다: `GetThreadPriority`(`0x00147D12`), `SetThreadPriority`(`0x00147CC0`), 핸들 `GetCurrentThread()`(−2).
* 위 표에서 A는 자기 마지막 unlock 때 P_A로 돌아가고, B도 자기 마지막 unlock 때 P_B로 돌아간다. 원본의 전역 저장값이 섞여도 결과는 스레드마다 맞다.
* 짝 없는 unlock(이 스레드의 깊이 0)은 원본만 실행한다(원래 동작).

## 3. 검증

| 검증 | 기대 |
|---|---|
| Release/Debug 빌드 | 경고 0 |
| 회귀 도구 | 통과 |
| 데모(숨긴 창) | 정지·시한 보고 없음 |
| 사용자 플레이 | 멈추지 않음 |

---

# Design: restoring the CRI ADXM lock's priority per thread (issue #91)

## 1. Problem

* The game hung in play again (the working branch after v0.0.37). The stall report has the shape of [i068](20261016-i068-adxm-lock.md).
  * Thread 2 is `running`, its last HLE call `ObfDereferenceObject` is current, and its base priority is 16 (TIME_CRITICAL).
  * Guessed stack: `0x001437BA` (the loop that releases every SVM lock) ← `0x0014076D` (`0x00140760`) ← `0x0014090D` (`0x001408C0`) ← `0x0014097B` (`0x00140920`).
* How it got there (**confirmed**, disassembly and XBE strings)
  * `0x00140920` is `wxCiGetFileSize` (`E0092901:fname is null.(wxCiGetFileSize)`).
  * A file missing from the cache goes through `0x001408C0` (`E0109261:'%s' is not in cache.`) to `0x00140760`.
  * `0x00140760` first releases every SVM lock through `0x001437B0`, then opens the file from disk (`E0040201:can not open '%s'.`).
  * The cache miss is a case this path is meant to handle. The hang is in `0x001437B0`.
* Cause (**inferred**): since i068 2.2.1, a holder puts the host lock down while it waits. Meanwhile another thread can enter, raising only the count, and the two can leave out of order.

  | Step | Count | Result |
  |---|---|---|
  | A enters | 0 → 1 | A's priority P_A is saved in the single global; A is TIME_CRITICAL |
  | A waits (puts down), B enters | 1 → 2 | B unchanged |
  | B waits (puts down), A picks up and leaves | 2 → 1 | nothing restored: **A is TIME_CRITICAL holding nothing** |
  | B picks up and leaves | 1 → 0 | B takes P_A |

  When A later enters `0x001437B0`, the unlock only drives the count negative and leaves the priority alone, so the loop never ends.
* On the Xbox a TIME_CRITICAL thread preempts at once when its wait ends, so this interleaving is rare or impossible (**inferred**). On the host, the order threads wake in and the order they pick the lock up again are not guaranteed.

## 2. Design

* The adapter (`src/game/jsrf/decomp/adxm_lock.c`) keeps "TIME_CRITICAL ⇔ holding the lock" **per thread**. The original bodies run unchanged, and only a correction follows them.
  * Thread-local state: this thread's lock depth and its priority at its outermost lock.
  * Lock: take the host lock; at depth 0, save the current priority; then run the original lock.
  * Unlock: run the original unlock. When this thread's depth reaches 0 and its priority differs from the saved one, restore the saved one. Then release the host lock.
  * The priority is read and written through the XAPI the game uses: `GetThreadPriority` (`0x00147D12`) and `SetThreadPriority` (`0x00147CC0`) on `GetCurrentThread()` (−2).
* In the table above, A returns to P_A at its own last unlock and B to P_B at its. However the original's global mixes, each thread ends right.
* An unmatched unlock (this thread at depth 0) runs only the original, as before.

## 3. Verification

| Check | Expected |
|---|---|
| Release/Debug builds | no warnings |
| Regression tools | pass |
| Demo (hidden window) | no stall or timeout report |
| User play | no hang |
