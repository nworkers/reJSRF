# 설계: 장면 노드 순회 fault — 다른 함수 본문 중간으로 가는 분기 (issue #21)

선행: [i015 별칭 분기 고아 수정](20261012-i015-xinput-hle.md), [i019 사용자 APC](20261012-i019-user-apc.md), [부팅 frontier](../analysis/boot-frontier.md)

## 1. 요구사항

* 첫 프레임과 폰트·효과음 버퍼 생성 뒤 약 50초에 thread 2가 `0x0FE80026`을 읽다 fault한다. 원인을 찾아 고치고 다음 frontier까지 부팅한다.

## 2. 원인 (확인됨)

* **확인됨**: fault 지점은 게임의 재귀 노드 순회 `0x00011070`이다. 노드마다 `[esi+4]`를 검사하고, 행렬 함수와 vtable `+4`를 부른 뒤, 자식(`+0x28`)과 형제(`+0x30`)를 따라간다. `esi = 0x0FE80022`는 노드가 아니다.
* **확인됨**: `REJSRF_RECOMP_ABI_CHECK` 빌드에서 첫 위반은 `call 0x0007E180`이다. 반환 주소를 pop하지 않고 돌아왔고, 그다음 `0x00011070`의 `esi`가 바뀌었다.
* **확인됨**: `0x0007E180`은 `push esi … pop esi; ret`인 thiscall이고, 네 개의 `jcc`가 공유 꼬리 `0x0007E255`(`pop esi; ret`)로 간다. 디스어셈블러(xboxrecomp)의 경위는 이렇다.
  1. `0x0007E180`은 vtable로만 닿으므로 빈 구간의 alias다.
  2. 꼬리 점프 패스가 끝난 뒤 `_pass_gap_prologues`가 `ret` 바로 뒤의 `0x0007E242`를 함수로 만들어, `0x0007E180`이 거기서 잘렸다.
  3. 이제 `0x0007E255`는 `sub_0007E242`의 본문 중간이다. `jmp`만 alias로 만드는 꼬리 점프 패스도, 빈 구간만 보는 조건 분기 고아 패스도 이 `jcc`를 다루지 않았다.
  4. 분기는 반환 주소만 pop하는 미해결 스텁 `sub_0007E255`(`g_esp += 4`)로 lift되었다. 그래서 `esi`가 복원되지 않고 esp가 4 모자라게 돌아왔다.

## 3. 수정

* xboxrecomp에 `_pass_branches_into_bodies`를 더한다. 본문을 자르는 패스들이 끝난 뒤 돌며, `jmp`나 `jcc`의 대상이 다른 함수 본문의 중간(시작이 아님)이면 그 본문 끝까지 가는 alias 진입점으로 등록한다.
  * 출발 구간은 함수 본문과 alias 범위다. `0x0007E180` 같은 빈 구간의 alias도 출발점이 된다.
  * 새 alias의 분기도 다시 본다(반복).
  * 대상은 스윕이 디코드한 명령 경계여야 한다.
* alias는 본문이 정해진 뒤 만들어지므로 다른 함수의 끝을 자르지 않는다(꼬리 점프 패스의 alias와 같은 이유).
* 정책(AGENTS.md, 사용자 결정)에 따라 업스트림 기준 브랜치 `fix/cond-branch-into-body`에서 고치고 PR을 올린다. 포크의 `rejsrf` 브랜치에 병합해 submodule이 가리키게 한다.

## 4. 진단 보강

* `REXBOX_FRAME_LOG=<n>`: n번째 present마다 한 줄(프레임 수, 경과 ms). 추적 없이 프레임 루프가 도는지 멈췄는지 구별한다.

## 5. 검증

| 검증 | 기대 |
|---|---|
| xboxrecomp `pytest tools/disasm tools/recomp` | 통과(새 `test_branches_into_bodies.py`) |
| 재생성 | `sub_0007E255`가 실제 함수, 미해결 스텁 감소 |
| Win32 Debug/Release, probe·check 도구, `check_hle_pops.py` | 회귀 없음 |
| 부팅 | `0x0FE80026` fault 없음, 다음 frontier 기록 |

---

# Design: the scene node walk fault — branches into another function's interior (issue #21)

Prerequisites: [i015 alias-branch-orphans fix](20261012-i015-xinput-hle.md), [i019 user APCs](20261012-i019-user-apc.md), [boot frontier](../analysis/boot-frontier.md)

## 1. Requirements

About 50 s into boot, after the first frame, fonts and sound-effect buffers, thread 2 faults reading `0x0FE80026`. Find and fix the cause and boot to the next frontier.

## 2. Cause (confirmed)

* **Confirmed**: the fault is in the game's recursive node walk at `0x00011070`. For each node it checks `[esi+4]`, calls a matrix function and vtable `+4`, then follows the children (`+0x28`) and siblings (`+0x30`). `esi = 0x0FE80022` is not a node.
* **Confirmed**: in a `REJSRF_RECOMP_ABI_CHECK` build, the first violation is `call 0x0007E180`, which came back without popping its return address. `0x00011070`'s `esi` changed after it.
* **Confirmed**: `0x0007E180` is a `push esi … pop esi; ret` thiscall whose four `jcc` share the tail `0x0007E255` (`pop esi; ret`). In the disassembler (xboxrecomp):
  1. `0x0007E180` is reached only from a vtable, so it is an alias in a gap.
  2. After the tail-jump pass, `_pass_gap_prologues` made a function of `0x0007E242`, right after a `ret`, which cut `0x0007E180` short there.
  3. `0x0007E255` now lay inside `sub_0007E242`. Neither the tail-jump pass (which aliases `jmp` only) nor the conditional-branch orphan pass (which looks at gaps only) took those `jcc`.
  4. They were lifted as calls to the unresolved stub `sub_0007E255` (`g_esp += 4`), which pops only the return address, so `esi` was never restored and esp came back 4 low.

## 3. Fix

* xboxrecomp gains `_pass_branches_into_bodies`. It runs after the passes that split bodies and registers every `jmp` or `jcc` target in the interior (not the start) of another function as an alias entry running to that body's end.
  * Sources are function bodies and alias ranges, so an alias in a gap such as `0x0007E180` counts.
  * New aliases are examined again (it repeats).
  * The target must be an instruction boundary the sweep decoded.
* Aliases are built after the bodies, so they clamp no other function's end (the same reason as the tail-jump pass's aliases).
* Per the policy (AGENTS.md, user decision), the fix is made on `fix/cond-branch-into-body`, based on upstream, and sent as a PR; it is merged into the fork's `rejsrf` branch, which the submodule points at.

## 4. Diagnostics

`REXBOX_FRAME_LOG=<n>` prints one line every n presents (frame count, elapsed ms), which tells a running frame loop from a stall without a full trace.

## 5. Verification

The table above applies: xboxrecomp's `pytest tools/disasm tools/recomp` passes (with the new `test_branches_into_bodies.py`); after regeneration `sub_0007E255` is a real function and fewer stubs remain; no regressions on Win32 Debug/Release, the probes and checks, and `check_hle_pops.py`; boot has no `0x0FE80026` fault, and the next frontier is recorded.
