# 작업 로그: 흰 윤곽선 조사용 정점 셰이더 그리기 감시 (issue #70)

설계: [20261016-i070](../design/20261016-i070-outline-watch.md) · 작업 지시: [20261016-i070](../work-orders/20261016-i070-outline-watch.md)

## 한 일

1. 조사(임시 계측, 커밋하지 않음)
   * 데모에서 프로그램 그리기의 핸들, 컬링, `c[96..97]`, 0번 스테이지 상태를 셰이더·컬링 조합마다 한 번 찍었다.
   * 컬링이 반대(`0x900`)인 그리기는 `66950977`과 `66942593` 둘뿐이다. 둘 다 색을 `c[97] = (0.08, 0, 0, 1)`에서 받는다.
   * `66949121`(`c[101]`이 색·기준·두께)은 데모에서 그려지지 않는다.
2. 가설 배제
   * 게임 코드에 NV2A 상수 메서드(`0x1EA4`, `0x0B80`) 즉값이 없다. 인라인 상수 쓰기가 없다는 뜻이다.
   * `LoadVertexShader`/`SelectVertexShader` 호출 지점이 없다.
3. `d3d8_draw_watch.h/.cpp`: `REXBOX_VSH_WATCH`. 프로그램 그리기에서 부른다.
4. 문서: [outlines](../analysis/outlines.md)(새 문서, 색인), ARCHITECTURE.md, 가이드.

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드 | 새 경고 없음 |
| 데모, `REXBOX_VSH_WATCH=66950977,66942593:96-98` | 66줄(`66950977` 64줄 상한, `66942593` 2줄). `c[97] = (0.08 0 0 1)`, `c[98].y = 0.3`, 컬링 `900` |
| 데모 60초, 변수 없음 | 출력 없음 |
| Debug 빌드 | 새 경고 없음 |
| 사용자 플레이 | 대기. 흰 윤곽선이 보일 때의 `[vsh watch]` 줄이 필요 |

## 남은 것

* 사용자 보고를 받아 원인을 정하고, 이 issue에 수정 설계를 이어 쓴다.

---

# Work log: watching vertex-shader draws to investigate white outlines (issue #70)

Design: [20261016-i070](../design/20261016-i070-outline-watch.md) · Work order: [20261016-i070](../work-orders/20261016-i070-outline-watch.md)

## Done

1. Investigation (temporary trace, not committed)
   * In the demo, logged the handle, culling, `c[96..97]` and stage-0 state of program draws once per shader and culling combination.
   * Only `66950977` and `66942593` draw with reversed culling (`0x900`); both take their color from `c[97] = (0.08, 0, 0, 1)`.
   * `66949121` (where `c[101]` is the color, threshold and thickness) is never drawn in the demo.
2. Hypotheses ruled out
   * The game code has no immediates of the NV2A constant methods (`0x1EA4`, `0x0B80`), so it has no inline constant writes.
   * There are no call sites of `LoadVertexShader`/`SelectVertexShader`.
3. `d3d8_draw_watch.h/.cpp`: `REXBOX_VSH_WATCH`, called from the program draw path.
4. Documents: [outlines](../analysis/outlines.md) (new, indexed), ARCHITECTURE.md, the guide.

## Verification

| Check | Result |
|---|---|
| Release build | no new warnings |
| Demo, `REXBOX_VSH_WATCH=66950977,66942593:96-98` | 66 lines (64, the cap, for `66950977`; 2 for `66942593`); `c[97] = (0.08 0 0 1)`, `c[98].y = 0.3`, culling `900` |
| Demo, 60 s, no variable | no output |
| Debug build | no new warnings |
| User play | waiting: needs the `[vsh watch]` lines from when white outlines show |

## Left

* Settle the cause from the user's report and continue with a fix design in this issue.
