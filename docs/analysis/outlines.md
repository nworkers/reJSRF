# 캐릭터 윤곽선

JSRF의 셀 셰이딩 윤곽선은 모델을 한 번 더 그려서 만든다. 법선 방향으로 부풀리고, 감는 방향을 반대로 컬링하는 inverted hull 방식이다. 이 문서는 그 정점 셰이더와 상수를 모은다(issue #70).

## 1. 윤곽선 셰이더

핸들은 `REXBOX_SHADER_DUMP`가 출력하는 10진수다. 실행마다 같았다(**확인됨**, 여러 번 실행).

| 핸들 | 입력 | 색 `oD0` | 두께 | 그 밖 | 쓰이는 곳 |
|---|---|---|---|---|---|
| `66950977` | 스키닝(행렬 두 개 혼합), 법선 `v3` | `c[97]` | `c[98].y`(법선 방향) | `c[98].x`를 clip w에 더함(깊이 밀기), `oFog = z` | 데모 캐릭터(**확인됨**) |
| `66942593` | 스키닝(행렬 하나), 법선 `v2` | `c[97]` | `c[98].y` | 같음 | 데모(**확인됨**) |
| `66949121` | 위치 `v0`, 법선 `v1` | `c[101].xxxx` | `c[101].y`(`c[100]` 방향) | `sge(N·c[100], c[101].x)`인 정점만 부풀림 | 플레이에서만(**추정**: 데모에서는 그려지지 않음) |

* 데모의 윤곽선 그리기는 컬링 `0x900`이다. 몸체는 `0x901`이다.
* 데모 값(**확인됨**, `REXBOX_VSH_WATCH`): `c[97] = (0.08, 0, 0, 1)`(거의 검정), `c[98] = (0 또는 −0.05, 0.3, 0, 0)`. 블렌드 SRCALPHA/INVSRCALPHA, 0번 스테이지 MODULATE(텍스처, diffuse), 텍스처 없음.
* 다른 셰이더 대부분은 `c[97] = (1, 1, 1, 1)`을 쓴다.

## 2. 흰 윤곽선 (미확정)

* `66949121`에서 `c[101].x`는 색이자 `sge` 기준값이다. 1에 가까우면 희고 가늘어진다. 플레이에서 그 값을 아직 보지 못했다.
* 배제한 것(**확인됨**): 게임 코드는 상수를 push buffer에 직접 쓰지 않는다. `LoadVertexShader`/`SelectVertexShader`도 부르지 않는다.
* 다음 단계: 사용자가 `REXBOX_VSH_WATCH=66949121,66950977,66942593`으로 플레이한 줄([설계 i070](../design/20261016-i070-outline-watch.md)).

---

# Character outlines

JSRF's cel-shaded outlines are a second draw of the model, inflated along the normals and culled with the opposite winding (an inverted hull). This note gathers its vertex shaders and constants (issue #70).

## 1. Outline shaders

Handles are the decimal numbers `REXBOX_SHADER_DUMP` prints; they were the same in every run (**confirmed**, several runs).

| Handle | Inputs | Color `oD0` | Thickness | Other | Used in |
|---|---|---|---|---|---|
| `66950977` | skinned (two matrices blended), normal `v3` | `c[97]` | `c[98].y` (along the normal) | `c[98].x` added to clip w (depth push); `oFog = z` | demo characters (**confirmed**) |
| `66942593` | skinned (one matrix), normal `v2` | `c[97]` | `c[98].y` | the same | demo (**confirmed**) |
| `66949121` | position `v0`, normal `v1` | `c[101].xxxx` | `c[101].y` (along `c[100]`) | only vertices with `sge(N·c[100], c[101].x)` are inflated | play only (**inferred**: never drawn in the demo) |

* The demo's outline draws cull with `0x900`; bodies use `0x901`.
* Demo values (**confirmed**, `REXBOX_VSH_WATCH`): `c[97] = (0.08, 0, 0, 1)` (almost black) and `c[98] = (0 or −0.05, 0.3, 0, 0)`; blending SRCALPHA/INVSRCALPHA; stage 0 MODULATE(texture, diffuse) with no texture.
* Most other shaders use `c[97] = (1, 1, 1, 1)`.

## 2. White outlines (unresolved)

* In `66949121`, `c[101].x` is both the color and the `sge` threshold; near 1 the outline is white and thin. Its value in play has not been seen yet.
* Ruled out (**confirmed**): the game code writes no constants straight into the push buffer, and it never calls `LoadVertexShader`/`SelectVertexShader`.
* Next: lines from the user's play with `REXBOX_VSH_WATCH=66949121,66950977,66942593` ([design i070](../design/20261016-i070-outline-watch.md)).
