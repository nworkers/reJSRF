# 작업 로그: SVM "모두 풀기" 루프를 스레드별 깊이로 (issue #100)

설계: [20261019-i100](../design/20261019-i100-svm-unlock-all.md) · 작업 지시: [20261019-i100](../work-orders/20261019-i100-svm-unlock-all.md)

## 한 일

1. 정지 보고 분석: thread 2가 잠금 횟수 0(safe 스레드가 정지돼 있음)인데 TIME_CRITICAL이고, `0x001437B0`의 "TIME_CRITICAL인 동안 풀기" 루프가 끝나지 않았다. i091의 복원 보고도 없었다(설계 1절).
2. `src/game/jsrf/decomp/adxm_lock.c`: `sub_001437B0` 대체. 이 스레드의 깊이 `t_depth`만큼 `SVM_Unlock`(`0x00141B80`)을 부르고 그 수를 `eax`로 돌려주고 리프트된 호출 규약대로 반환 주소를 `g_esp`에서 뺀다. 그래도 TIME_CRITICAL이면 저장된 자기 우선순위로 내리고 한 번 보고한다.
3. 재생성: 생성기가 `sub_001437B0`을 수동 정의로 뺀다(원본 본문은 쓰지 않으므로 `_gen`은 없다).
4. 문서: `ARCHITECTURE.md`.

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 데모 120초(21:9) | 오류·정지·잠금 보고 없음. 리프트된 `ret`(`g_esp += 4`)를 빠뜨린 첫 빌드는 데모에서 깨진 파일 이름으로 fault가 났으므로, 데모가 이 경로를 실제로 지난다 |
| 사용자 플레이 | 대기 |

---

# Work log: the SVM "release all" loop keyed on per-thread depth (issue #100)

Design: [20261019-i100](../design/20261019-i100-svm-unlock-all.md) · Work order: [20261019-i100](../work-orders/20261019-i100-svm-unlock-all.md)

## Done

1. Stall report analysis: thread 2 was TIME_CRITICAL with an SVM lock count of 0 (the safe thread suspended), and the "release while TIME_CRITICAL" loop in `0x001437B0` never ended; no i091 restore report either (design §1).
2. `src/game/jsrf/decomp/adxm_lock.c`: the `sub_001437B0` replacement. It calls `SVM_Unlock` (`0x00141B80`) as many times as this thread's depth `t_depth` and returns the count in `eax`, popping the return address from `g_esp` as the lifted convention expects; if still TIME_CRITICAL, it lowers the thread to its saved priority and reports once.
3. Regeneration: the generator leaves `sub_001437B0` to the manual definition (no `_gen`, since the original body is not used).
4. Documents: `ARCHITECTURE.md`.

## Verification

| check | result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks 0 failures / 14018 cases 0 differences / 129 cases 0 differences |
| 120 s demo (21:9) | no fault, stall or lock report; the first build, which forgot the lifted `ret` (`g_esp += 4`), faulted at a garbage file name in the demo, so the demo does run this path |
| User play | pending |
