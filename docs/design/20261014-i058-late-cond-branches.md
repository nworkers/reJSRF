# 설계: 늦게 생긴 본문의 조건 분기가 빈 구간으로 가는 경우 (issue #58)

선행: [i055 미해결 스텁](20261014-i055-unresolved-stubs.md), [unresolved-stubs](../analysis/unresolved-stubs.md)

## 1. 문제

i055 뒤 남은 스텁 76개 중 위치가 애매한 세 곳을 정적으로 확인했다.

| 스텁 | 정체 | 실행되면 | 판단 |
|---|---|---|---|
| `0x22F30` | `sub_00022E50` 안의 루프 머리. 원래 함수는 `goto`로 처리하고, 스텁은 아무도 부르지 않는 alias `sub_00022F7F`에서만 불림 | 영향 없음 | 그대로 둠 |
| `0x1783FE` | stdcall 메서드 `sub_001783D0`(`ret 0xc`)의 한 경로. `[obj+0xC]==0`이면 내부 객체의 vtable `+0x20`으로 넘긴다(`jmp [ecx+0x20]`) | 스텁은 4바이트만 pop해서 esp가 12바이트 어긋난다 | 고침 |
| `0x1296C7` | 가상 메서드 `0x129180`(지역 `0xC04`)의 공통 마무리(`call 0x11BE0`, `call 0x11C20`, pop 4개, `add esp, 0xc04; ret`) | switch case 4(`0x129367`)에서 `sub_001343C0`이 1을 돌려주면 간다. 스텁이면 프레임이 풀리지 않는다 | 고침 |

* **확인됨**: 두 곳 모두 `je`로 닿고, 출발점은 tail-jump pass 뒤에 생긴 본문이다.
  * `0x1783FE`의 출발점은 alias `sub_001783D0`이다.
  * `0x1296C7`의 출발점은 `gap_prologue` `sub_00129367`이다.
* **확인됨**: `0x129180`의 switch 표 셋(`0x12A588`, `0x12A5A0`, `0x12A5BC`, 21칸)은 모두 디스패치에 있다. case 분기는 `RECOMP_ITAIL`로 동작한다.

## 2. 원인

* `_pass_cond_branch_orphans`(빈 구간으로 가는 `jcc`)는 pass 6 안에서 돈다. 그래서 그 뒤에 생긴 본문은 보지 못한다.
* `_pass_alias_branch_orphans`는 alias에서 출발하는 `jcc`를 보지만, 대상이 다음 중 하나여야 한다.
  * 짧은 탐침에서 `ret`에 닿는다.
  * 직접 `jmp`로 끝난다.
  * `0x1783FE`는 간접 `jmp`로 끝나서 거절됐다.
* i055의 `_pass_late_tail_jumps`는 무조건 `jmp`만 받는다.

## 3. 설계 (xboxrecomp, upstream PR #200에 덧붙임)

* `_pass_late_tail_jumps`가 `jcc`도 받는다.
  * 출발점과 대상 조건은 `jmp`와 같다. 최종 본문 또는 빈 구간 alias에서 출발해, 어느 본문에도 없는 디코드된 명령으로 간다.
  * `jcc`는 본문이 짧게 측정됐다는 뜻일 수도 있다. 그래서 `_pass_cond_branch_orphans`와 같은 받기 조건을 더한다. 대상은 다음 중 하나여야 한다.
    * `ret`로 끝나는 본문
    * vcall thunk
    * 직접 `jmp`로 끝나는 블록
    * **간접 `jmp`로 끝나는 블록(새 조건)**: 함수 안의 분기가 가리키므로 코드라는 근거가 이미 있다. 간접 `jmp`는 vtable을 통한 tail call이나 switch로, 블록의 끝이다.
  * 등록은 지금처럼 alias 항목으로 한다. 본문을 자르지 않는다.
* 엔진에 `block_ends_in_indirect_jump(addr)`를 더한다. `block_tail_jump`와 같은 직선 블록 읽기에서 첫 종결 명령이 간접 `jmp`인지 본다.
* 새 alias가 다른 본문 **안**으로 분기할 수 있다. 그래서 늦은 pass가 무언가를 더하면 `_pass_branches_into_bodies`를 한 번 더 돌린다.
* 테스트(`test_late_tail_jumps.py`에 추가):
  * 늦은 본문에서 `ret` 블록으로 가는 `jcc` → alias
  * 간접 `jmp` 블록으로 가는 `jcc` → alias
  * 받기 조건을 못 넘는 블록(길게 이어짐)으로 가는 `jcc` → 받지 않음

## 4. seed (reJSRF)

* `0x1783FE`와 `0x1296C7`을 `seed_functions.json`에 넣는다(사용자 결정).
* 3의 수정만으로 되찾는지는 seed 없이 먼저 다시 생성해서 확인한다. 그 뒤 seed를 넣는다.

## 5. 검증

| 검증 | 기대 |
|---|---|
| xboxrecomp `pytest tools/recomp tools/disasm` | 새 테스트 통과, 회귀 없음 |
| seed 없이 다시 생성 | `0x1783FE`, `0x1296C7`이 함수(alias)가 되고, 스텁이 76개에서 줄어듦. 다른 변화 기록 |
| seed를 넣고 다시 생성 | 같은 결과 |
| 데모 프레임 6장, 회귀 도구, Debug/Release | 같음, 통과, 경고 0 |

---

# Design: conditional branches from late bodies into gaps (issue #58)

Prerequisites: [i055 unresolved stubs](20261014-i055-unresolved-stubs.md), [unresolved-stubs](../analysis/unresolved-stubs.md)

## 1. Problem

Three of the 76 stubs left after i055, whose position was unclear, were checked statically:

* `0x22F30` is a loop head inside `sub_00022E50`. The function itself handles it with a `goto`, and the stub is called only from the alias `sub_00022F7F`, which nothing calls. It has no effect and stays.
* `0x1783FE` is a path of the stdcall method `sub_001783D0` (`ret 0xc`). When `[obj+0xC]==0`, it forwards to an inner object's vtable `+0x20` (`jmp [ecx+0x20]`). As a stub it pops 4 bytes, leaving esp 12 off. To fix.
* `0x1296C7` is the shared epilogue of the virtual method `0x129180` (locals `0xC04`): `call 0x11BE0`, `call 0x11C20`, four pops, `add esp, 0xc04; ret`. Switch case 4 (`0x129367`) goes there when `sub_001343C0` returns 1. As a stub the frame is never unwound. To fix.

Findings:

* **Confirmed**: both are reached by `je`, from bodies made after the tail-jump pass:
  * `0x1783FE` from the alias `sub_001783D0`
  * `0x1296C7` from the gap prologue `sub_00129367`
* **Confirmed**: the three switch tables of `0x129180` (`0x12A588`, `0x12A5A0`, `0x12A5BC`, 21 entries) are all in the dispatch table, so its cases work through `RECOMP_ITAIL`.

## 2. Cause

* `_pass_cond_branch_orphans` (a `jcc` into a gap) runs inside pass 6 and never sees later bodies.
* `_pass_alias_branch_orphans` takes a `jcc` from an alias, but the target must:
  * reach a `ret` in a short probe, or
  * end in a direct `jmp`
  * `0x1783FE` ends in an indirect `jmp` and was refused.
* i055's `_pass_late_tail_jumps` takes only an unconditional `jmp`.

## 3. Design (xboxrecomp, added to upstream PR #200)

* `_pass_late_tail_jumps` takes a `jcc` too.
  * Sources and targets are as for a `jmp`: from a final body or a gap alias, to a decoded instruction no body claims.
  * A `jcc` can also mean a body measured short, so `_pass_cond_branch_orphans`'s acceptance test is added. The target must be one of:
    * a body that ends in `ret`
    * a vcall thunk
    * a block that ends in a direct `jmp`
    * **a block that ends in an indirect `jmp` (new)**: a branch from inside a function already says it is code, and an indirect `jmp` (a tail call through a vtable, or a switch) ends the block.
  * It is still registered as an alias entry, which cannot cut a body.
* The engine gains `block_ends_in_indirect_jump(addr)`: it reads a straight-line block the way `block_tail_jump` does and tells whether its first terminator is an indirect `jmp`.
* A new alias can branch **into** another body, so `_pass_branches_into_bodies` runs once more whenever the late pass adds anything.
* Tests (added to `test_late_tail_jumps.py`):
  * a `jcc` from a late body to a `ret` block becomes an alias
  * a `jcc` to an indirect-`jmp` block becomes an alias
  * a `jcc` to a block that fails the test (runs on) is not taken

## 4. Seeds (reJSRF)

* `0x1783FE` and `0x1296C7` go into `seed_functions.json` (the user's decision).
* First the code is regenerated without the seeds, to see whether §3 alone recovers them; the seeds go in after.

## 5. Verification

* xboxrecomp `pytest tools/recomp tools/disasm`: the new tests pass, with no regressions.
* Regenerated without seeds: `0x1783FE` and `0x1296C7` become functions (aliases) and stubs drop below 76; other changes recorded.
* Regenerated with seeds: the same result.
* Six demo frames, regression tools, Debug and Release: identical, passing, zero warnings.
