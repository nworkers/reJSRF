# 작업 로그: CRI ADXM 잠금의 우선순위를 스레드마다 되돌리기 (issue #91)

설계: [20261018-i091](../design/20261018-i091-adxm-lock-priority.md) · 작업 지시: [20261018-i091](../work-orders/20261018-i091-adxm-lock-priority.md)

## 한 일

1. 정지 보고 분석
   * 스택 주소를 lift 함수에 대응시켰다: `0x001437B0` ← `0x00140760` ← `0x001408C0` ← `0x00140920`.
   * XBE 문자열로 `wxCiGetFileSize`의 캐시 미스 경로임을 확인했다.
   * 잠금 콜백(`0x0013B0A0`, `0x0013B0E0`)이 전역 저장 우선순위 하나를 쓰는 것과, i068 2.2.1의 내려놓기로 생길 수 있는 엇갈림을 정리했다(설계 1절).
2. `src/game/jsrf/decomp/adxm_lock.c`
   * 스레드 지역 깊이(`t_depth`)와 가장 바깥 lock 때의 우선순위(`t_priority`)를 둔다.
   * lift된 XAPI `GetThreadPriority`(`0x00147D12`), `SetThreadPriority`(`0x00147CC0`)를 게스트 스택으로 부르는 도우미를 두었다.
   * unlock 뒤 이 스레드의 깊이가 0이 되면 우선순위를 자기 값으로 되돌린다. 처음 되돌릴 때 `[adxm lock] ... restored (issue #91)`을 한 번 보고한다.
3. 문서: i068 설계에 후속 절(2.2.2)과 링크.

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 숨긴 창 데모 100~120초 | 정지·시한 보고 없음. 되돌림 보고도 없음(데모에서는 엇갈림이 생기지 않음) |
| 데모 프레임 | 1500은 변경 전과 같다. 3000, 4650은 같은 장면이 몇 프레임 어긋난다. 같은 바이너리는 실행마다 같은 프레임을 낸다. 변경 전 바이너리는 기준과 같다. lock마다 우선순위를 읽는 HLE 호출이 늘어 실시간 기준 진행이 조금 밀린 것으로 본다(**추정**) |
| 사용자 플레이 | 확인 대기. 같은 정지가 다시 나면 `[adxm lock]` 줄이 있는지 함께 본다 |

---

# Work log: restoring the CRI ADXM lock's priority per thread (issue #91)

Design: [20261018-i091](../design/20261018-i091-adxm-lock-priority.md) · Work order: [20261018-i091](../work-orders/20261018-i091-adxm-lock-priority.md)

## Done

1. Stall report analysis
   * The stack addresses were mapped to lifted functions: `0x001437B0` ← `0x00140760` ← `0x001408C0` ← `0x00140920`.
   * XBE strings confirmed the path is a `wxCiGetFileSize` cache miss.
   * The lock callbacks (`0x0013B0A0`, `0x0013B0E0`) use one global saved priority; the out-of-order exit that i068 2.2.1's putting down allows is written up in design section 1.
2. `src/game/jsrf/decomp/adxm_lock.c`
   * A thread-local depth (`t_depth`) and the priority at the outermost lock (`t_priority`).
   * Helpers call the lifted XAPI `GetThreadPriority` (`0x00147D12`) and `SetThreadPriority` (`0x00147CC0`) through the guest stack.
   * After an unlock that brings this thread's depth to 0, the thread's own priority is restored. The first restore is reported once as `[adxm lock] ... restored (issue #91)`.
3. Documents: a follow-up section (2.2.2) and link in the i068 design.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Hidden-window demo, 100-120 s | no stall or timeout report; no restore report either (the demo does not interleave) |
| Demo frames | 1500 matches the previous build; 3000 and 4650 show the same scenes a few frames apart. One binary gives the same frames on every run, and the previous binary matches the baseline. The extra HLE calls that read the priority at each lock shift the real-time progress slightly (**inferred**) |
| User play | waiting for the user's check; if the hang returns, look for an `[adxm lock]` line too |
