# CRI 미들웨어: 스레드와 잠금

JSRF는 CRI의 사운드·스트리밍 미들웨어를 정적 링크한다. 이 문서는 우리 런타임과 부딪히는 부분(스레드, 우선순위, 잠금)을 모은다.

## 1. 구성 (**확인됨**, 문자열)

| 모듈 | 버전 문자열 |
|---|---|
| ADXXB | `ADXXB Ver.1.14 Build:Dec 10 2001` |
| ADXF/XB | `ADXF/XB Ver.6.21` |
| CVFS/XB | `CVFS/XB Ver.2.23` |
| SVM/XB (서버 관리) | `SVM/XB Ver.1.06` |
| SJ, LSC, RNA, RNADS, WXCI/XB, MFCI/XB | 각 버전 문자열 있음 |

* 문자열은 `0x001DF004`부터 `0x001DFFE4` 근처에 모여 있다.

## 2. ADXM 스레드 (**확인됨**, 디스어셈블; 함수 이름은 **추정**)

`ADXM_SetupThrd`(`0x0013B330`)가 스레드 넷을 `CREATE_SUSPENDED`로 만든다. 우선순위는 매개변수가 없을 때의 기본값이다.

| 핸들 변수 | 시작 함수 | 우선순위 | 하는 일 |
|---|---|---|---|
| `[0x27D0E0]` | `0x0013B180` safe | 2 | HLE 호출 없이 `[0x25EFA8]`을 올리며 돈다. 잠금 동안만 재개된다 |
| `[0x27D0FC]` | `0x0013B1C0` vsync | 1 | 수직 귀선마다 서버(`0x00141E50`)를 돌리고 idle 스레드를 `ResumeThread` |
| `[0x27D104]` | `0x0013B230` | 1 | 수직 귀선 대기 루프 |
| `[0x27D0E8]` | `0x0013B2A0` mwidle | −15 | 서버를 돌리고 스스로 `SuspendThread` |

* 잠금의 TIME_CRITICAL 값은 `[0x25EF8C]`(15)에서 온다.
* 정지 보고의 thread 3~6이 이 넷이다(thread 2는 present 스레드).

## 3. SVM 잠금 (**확인됨**)

* `SVM_SetCbLock`(`0x00141DB0`) → `[0x2615E8]`/`[0x2615EC]`, `SVM_SetCbUnlock`(`0x00141DD0`) → `[0x2615F0]`/`[0x2615F4]`.
* `SVM_Lock`(`0x00141B60`)과 `SVM_Unlock`(`0x00141B80`)이 콜백을 부른다.
* ADXM이 거는 콜백:
  * lock `0x0013B0A0`: 전역 횟수 `[0x25EFA0]`가 0이면, 자기 우선순위를 `[0x27D0F8]`에 저장하고 TIME_CRITICAL로 올린 뒤 safe 스레드를 재개한다. 그다음 횟수를 1 올린다.
  * unlock `0x0013B0E0`: 횟수를 1 내리고, 0이면 safe 스레드를 정지하고 우선순위를 되돌린다.
* `0x001437B0`은 "TIME_CRITICAL인 동안 `SVM_Unlock`"을 반복해 잠금을 모두 풀고 그 횟수를 돌려준다. `0x001437D0`이 같은 횟수만큼 다시 잡는다. 호출자는 `0x00140130`, `0x001406D0`, `0x00140760`, `0x00140C60`이다.
* 상호 배제는 단일 코어 + 엄격한 우선순위에만 기댄다. 호스트에서 스레드가 동시에 돌면 깨진다. 이것이 #68의 멈춤 원인이다(**추정**: 같은 장면을 다시 재현하지는 못했다). 데모에서는 thread 2와 thread 4가 이 잠금을 쓴다(**확인됨**, 70초에 약 1,000번).
* 대응: [설계 i068](../design/20261016-i068-adxm-lock.md). 두 콜백을 런타임 게스트 잠금으로 감싼다.

## 4. 미확정

* safe 스레드는 한 번 재개되면 계속 돈다. 다른 스레드에 대한 `NtSuspendThread`가 횟수만 세기 때문이다. 호스트 코어 하나를 쓰는 것 말고 다른 영향이 있는지는 모른다.
* `0x00145585`(잠금을 푼 채로 부르는 함수)가 무엇인지.

---

# CRI middleware: threads and locks

JSRF links CRI's sound and streaming middleware statically. This note gathers the parts that meet our runtime: threads, priorities and locks.

## 1. Components (**confirmed**, strings)

The table above lists the modules: ADXXB 1.14 (built Dec 10 2001), ADXF/XB 6.21, CVFS/XB 2.23, SVM/XB 1.06 (server management), and SJ, LSC, RNA, RNADS, WXCI/XB and MFCI/XB, each with its version string. The strings sit around `0x001DF004` to `0x001DFFE4`.

## 2. ADXM threads (**confirmed**, disassembly; function names **inferred**)

`ADXM_SetupThrd` (`0x0013B330`) creates four threads with `CREATE_SUSPENDED`. The priorities are its defaults when it gets no parameters.

| Handle variable | Start | Priority | What it does |
|---|---|---|---|
| `[0x27D0E0]` | `0x0013B180` safe | 2 | spins incrementing `[0x25EFA8]` with no HLE calls; resumed only while the lock is held |
| `[0x27D0FC]` | `0x0013B1C0` vsync | 1 | runs the server (`0x00141E50`) every vertical blank and `ResumeThread`s the idle thread |
| `[0x27D104]` | `0x0013B230` | 1 | a vertical-blank wait loop |
| `[0x27D0E8]` | `0x0013B2A0` mwidle | −15 | runs the server and `SuspendThread`s itself |

* The lock's TIME_CRITICAL value comes from `[0x25EF8C]` (15).
* Threads 3 to 6 in the stall report are these four; thread 2 is the presenting thread.

## 3. The SVM lock (**confirmed**)

* `SVM_SetCbLock` (`0x00141DB0`) stores into `[0x2615E8]`/`[0x2615EC]`, and `SVM_SetCbUnlock` (`0x00141DD0`) into `[0x2615F0]`/`[0x2615F4]`.
* `SVM_Lock` (`0x00141B60`) and `SVM_Unlock` (`0x00141B80`) call the callbacks.
* The callbacks ADXM installs:
  * Lock `0x0013B0A0`: at global count `[0x25EFA0]` 0, it saves its own priority in `[0x27D0F8]`, raises itself to TIME_CRITICAL and resumes the safe thread. Then it raises the count by 1.
  * Unlock `0x0013B0E0`: it lowers the count by 1; at 0 it suspends the safe thread and restores the priority.
* `0x001437B0` repeats `SVM_Unlock` while the caller is TIME_CRITICAL, dropping the lock entirely, and returns how many times; `0x001437D0` takes it back that many times. The callers are `0x00140130`, `0x001406D0`, `0x00140760` and `0x00140C60`.
* The exclusion relies only on a single core with strict priorities, and it breaks when host threads run in parallel. This is the cause of the #68 hang (**inferred**: the scene was not reproduced). In the demo, threads 2 and 4 take this lock (**confirmed**, about 1,000 times in 70 s).
* Remedy: [design i068](../design/20261016-i068-adxm-lock.md) wraps both callbacks in the runtime's guest lock.

## 4. Unresolved

* Once resumed, the safe thread keeps spinning, because `NtSuspendThread` of another thread only counts. Whether this matters beyond keeping a host core busy is unknown.
* What `0x00145585`, called with the lock dropped, does.
