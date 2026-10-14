# Issue #58 작업 로그 — 늦게 생긴 본문의 조건 분기

작업 지시: [20261014-i058](../work-orders/20261014-i058-late-cond-branches.md) · 설계: [20261014-i058](../design/20261014-i058-late-cond-branches.md) · 분석: [unresolved-stubs §2b](../analysis/unresolved-stubs.md)

## 요약

남은 스텁 가운데 위치가 애매한 세 곳을 정적으로 확인했다.

* `0x22F30`은 영향이 없었다.
* `0x1783FE`와 `0x1296C7`은 실행되면 스택이 깨지는 진짜 누락이었다.

i055와 같은 pass 순서 문제의 `jcc` 판이었다. 디스어셈블러 수정을 upstream PR #200에 덧붙였다. 그 수정이 lifter의 공백(복구된 switch case 안의 중첩 switch)을 드러내서 그것도 고쳐 같은 PR에 넣었다. 두 주소는 seed로도 넣었다.

결과:

* 스텁은 76개에서 74개, 함수는 9,203개에서 9,213개가 됐다.
* 디스패치 항목은 늘기만 했다.

## 과정

1. **정적 확인**(설계 §1):
   * `0x22F30`은 루프 머리다. 원래 함수 `sub_00022E50`이 `goto`로 처리하고, 스텁은 아무도 부르지 않는 alias에서만 불린다.
   * `0x1783FE`는 `jmp [ecx+0x20]`으로 넘기는 stdcall 메서드 경로다.
   * `0x1296C7`은 `0x129180`의 공통 마무리다.
   * `0x129180`의 switch 표 셋(21칸)은 모두 디스패치에 있었다.
2. **xboxrecomp fork, PR #200 브랜치(`fix/cond-branch-into-body`)**:
   * `0c32a78`: `block_ends_in_indirect_jump`를 더했다. `_pass_late_tail_jumps`가 `jcc`도 받는다. 무언가를 더하면 `_pass_branches_into_bodies`를 다시 돈다. 테스트를 4개에서 10개로 늘렸다.
     * 기존 테스트 하나("빈 구간으로 가는 `jcc`는 다른 pass에 맡김")는 새 설계에 맞게 "받기 조건을 못 넘는 블록"으로 바꿨다.
   * seed 없이 다시 생성했다.
     * 두 주소를 되찾았다(스텁 76 → 74).
     * 그러나 디스패치 항목 12개(표 `0x12A5BC`의 8칸 등)가 사라졌다.
3. **lifter 원인**:
   * `sub_001297EC`(표 `0x12A588`의 case로 복구된 함수, 범위 `0x12A586`까지) 안의 switch `0x129842`가 표 `0x12A5BC`로 `RECOMP_ITAIL`을 낸다. 그런데 그 case들에 진입점이 없었다.
   * 전에는 이웃 빈 구간의 본문에서 우연히 진입점이 생겼다. `jcc` 수정으로 `0x1296C7`이 그 구간을 덮는 alias가 되자 사라졌다.
   * 고친 것:
     * `982cd28`: `discover_jump_table_entries`가 새로 만든 case 진입점을 다시 검사한다(고정점).
     * `7c58ec6`: 복구된 본문에는 "case가 모두 범위 안이면 함수 안 goto"라는 건너뛰기를 적용하지 않는다. 복구된 본문은 복구 때 알아낸 표만 `goto`로 번역하기 때문이다.
   * 처음 쓴 테스트는 case가 범위 밖이라 `982cd28`만으로 통과했다. 그래서 JSRF처럼 case가 범위 안인 모양으로 바꿨다.
     * 바꾼 테스트의 첫 판은 case 진입점이 `je`(디스패처가 세운 플래그 읽기)로 시작해 `_arm_is_whole`가 거절했다. `test eax, eax`를 앞에 넣었다.
     * 고친 lifter를 빼면 이 테스트가 실패하는 것을 확인했다(`{arm_a}`만 나옴).
4. 다시 생성(seed 없음): 디스패치는 9,203에서 9,215가 됐고 사라진 항목은 없었다.
   * 끝이 짧아진 함수 5개는 새 alias 시작에서 끊겼다.
   * 그중 2개(`0xF8000`, `0x104000`)는 종결 명령 없이 이어지는데, 생성 코드는 `/* fallthrough */` tail call로 다음 alias를 부른다. 같은 동작이다.
5. **seed**(사용자 결정): `0x1783FE`, `0x1296C7`을 넣고 다시 생성했다(9,213 함수, 스텁 74).
   * seed가 있으면 두 곳은 `seed_vtable_thunk` 함수가 되고, 끝이 블록의 `ret` 바로 뒤(`0x1296E5`, `0x178416`)로 정확해진다.
   * seed가 없을 때는 `0x1296C7`이 2,275바이트 alias로 뒤쪽 코드를 중복 번역했다. 그 본문에서 나온 alias 2개(`0x129FD9`, `0x12A55A`)가 seed가 있으면 생기지 않는다.
   * 스텁 수는 같고 새 스텁도 없어서 seed 쪽으로 했다. 설계 §5의 "같은 결과"와는 이 점이 다르다.
6. **확인**:
   * Debug와 Release 빌드가 성공했다.
   * 데모 95초 4회 모두 `[recomp]` 보고는 0건이었다.
   * 프레임 6장은 4회 중 3회 기준과 같았다. 1회는 3,000·4,800이 달랐는데, 그 3,000은 i055 때 흔들렸던 프레임과 바이트 단위로 같다. 타이밍 흔들림이다.
   * 회귀 도구는 모두 통과했다.
7. **push**:
   * fork의 `fix/cond-branch-into-body`(`58f7166..7c58ec6`)와 `rejsrf`(`2e73f7c..1991221`)를 올렸다.
   * PR #200은 커밋 5개가 됐고, 제목은 `disasm, recomp: re-examine branches of bodies made after the tail-jump pass`로 바꿨다.
   * 본문에 `jcc`(§2)와 lifter(§3) 내용, 테스트 목록을 더했다.
8. **간접 호출 누락 `0x1FD10`**(사용자 플레이 로그, [unresolved-stubs §2c](../analysis/unresolved-stubs.md)):
   * 표 `0x1EBC80`(78칸)의 23번 칸이다. 같은 표의 81번 칸 `0x20F10`도 빠져 있었다.
   * 둘 다 인라인 바이트 표 뒤의 함수라, i017부터 있던 포인터 seed가 "명령 한가운데"로 거절되고 있었다.
   * `observed` seed로 넣었다. 다시 생성하자 디스패치가 2개 늘었고(9,215), 거절된 seed와 `find_pointer_targets.py` 후보는 0개가 됐다.
   * 데모 2회는 `[recomp]` 0건, 프레임 6장이 같았다. Debug/Release 빌드와 회귀 도구도 통과했다.

## 검증

| 검증 | 결과 |
|---|---|
| xboxrecomp `pytest tools/recomp tools/disasm` | PR 브랜치 526 통과·38 건너뜀, `rejsrf` 548 통과·42 건너뜀 |
| 다시 생성(seed 포함) | 9,215 함수(실패 0), 스텁 74, 디스패치는 늘기만 함, 거절된 seed 0, 포인터 후보 0 |
| 데모 95초 4회 | `[recomp]` 0건, 프레임 6장 3회 같음(1회 흔들림) |
| Win32 Debug / Release 빌드 | 성공, 경고 0 |
| 회귀 도구(Debug, Release) | kernel 56/56, d3d8 116/116, dsound 48/48, d3d8 check 14,018/0, decomp check 6/0 |

## 남은 일

* 게임 진행으로 확인(사용자): 두 경로가 실행되는 장면은 아직 모른다. 실행 중 보고로 볼 수 있다.
* 남은 74개: alias 역방향 분기(진입점 여러 개 번역)와 데이터를 코드로 잘못 읽은 곳.

---

# Issue #58 work log — conditional branches from late bodies

Work order: [20261014-i058](../work-orders/20261014-i058-late-cond-branches.md) · Design: [20261014-i058](../design/20261014-i058-late-cond-branches.md) · Analysis: [unresolved-stubs §2b](../analysis/unresolved-stubs.md)

## Summary

Three of the remaining stubs whose position was unclear were checked statically:

* `0x22F30` has no effect.
* `0x1783FE` and `0x1296C7` were real misses that would break the stack if run.

It was the `jcc` form of i055's pass-order problem. The disassembler fix went into upstream PR #200. It exposed a lifter gap (nested switches inside recovered switch arms), which was fixed and added to the same PR. Both addresses were also seeded.

Results:

* Stubs went from 76 to 74, and functions from 9,203 to 9,213.
* The dispatch table only grew.

## Process

1. **Static check** (design §1):
   * `0x22F30` is a loop head handled by `goto` in `sub_00022E50`, and the stub is called only from an alias nothing calls.
   * `0x1783FE` is a stdcall method path forwarding through `jmp [ecx+0x20]`.
   * `0x1296C7` is `0x129180`'s shared epilogue.
   * `0x129180`'s three switch tables (21 entries) were all in the dispatch table.
2. **xboxrecomp fork, PR #200 branch (`fix/cond-branch-into-body`)**:
   * `0c32a78`: `block_ends_in_indirect_jump`; `_pass_late_tail_jumps` takes a `jcc` too; `_pass_branches_into_bodies` runs again when it adds anything; tests went from 4 to 10.
     * One old test ("a `jcc` into a gap is left to other passes") now uses a block that fails the acceptance test, as the new design requires.
   * Regenerated without seeds:
     * both addresses were recovered (stubs 76 → 74)
     * but 12 dispatch entries disappeared (the 8 arms of table `0x12A5BC`, among others)
3. **The lifter cause**:
   * Inside `sub_001297EC` (recovered as an arm of table `0x12A588`, spanning to `0x12A586`), the switch at `0x129842` emits `RECOMP_ITAIL` through table `0x12A5BC`, whose arms had no entries.
   * Before, a neighbouring gap body happened to supply them. Once the `jcc` fix made `0x1296C7` an alias covering that region, they were gone.
   * Fixed:
     * `982cd28`: `discover_jump_table_entries` scans the arm entries it adds (to a fixed point).
     * `7c58ec6`: a recovered body no longer gets the "all arms in range, so in-function gotos" skip, since it turns into gotos only the tables its recovery found.
   * The first test had its arms out of range and passed with `982cd28` alone, so it was rewritten with the arms in range, as in JSRF.
     * Its first version started the arm with `je` (reading the dispatcher's flags), which `_arm_is_whole` rejects; `test eax, eax` was added in front.
     * Without the lifter change the test fails (only `arm_a`).
4. Regenerated without seeds: the dispatch table went from 9,203 to 9,215, with nothing lost.
   * Five functions whose end shortened stop at a new alias start.
   * Two of them (`0xF8000`, `0x104000`) run on without a terminator, and the generated code calls the next alias as a `/* fallthrough */` tail call, which is the same behaviour.
5. **Seeds** (the user's decision): `0x1783FE` and `0x1296C7`, regenerated (9,213 functions, 74 stubs).
   * With the seeds the two become `seed_vtable_thunk` functions ending right after their block's `ret` (`0x1296E5`, `0x178416`).
   * Without them, `0x1296C7` was a 2,275-byte alias translating the code after it a second time, and two aliases from that body (`0x129FD9`, `0x12A55A`) do not appear with the seeds.
   * The stub count is the same and there are no new stubs, so the seeds were kept. This differs from design §5's "same result".
6. **Checks**:
   * Debug and Release builds succeed.
   * In four 95 s demo runs there were no `[recomp]` reports.
   * The six frames matched the baseline in three of the four runs. In one run, 3,000 and 4,800 differed, and that 3,000 is byte-identical to the frame that jittered in i055, so it is timing jitter.
   * The regression tools pass.
7. **Push**:
   * The fork's `fix/cond-branch-into-body` (`58f7166..7c58ec6`) and `rejsrf` (`2e73f7c..1991221`) were pushed.
   * PR #200 now has five commits and is titled `disasm, recomp: re-examine branches of bodies made after the tail-jump pass`.
   * Its description gained the `jcc` (§2) and lifter (§3) parts and the updated test list.
8. **The missed indirect call `0x1FD10`** (user play log, [unresolved-stubs §2c](../analysis/unresolved-stubs.md)):
   * It is entry 23 of table `0x1EBC80` (78 entries); entry 81 of the same table, `0x20F10`, was missing too.
   * Both follow an inline byte table, so the pointer seeds in place since i017 were refused as mid-instruction.
   * They became `observed` seeds. Regenerating added two dispatch entries (9,215), and left no refused seeds and no `find_pointer_targets.py` candidates.
   * Two demo runs: no `[recomp]` reports, six frames identical; Debug and Release builds and the regression tools pass.

## Verification

* xboxrecomp `pytest tools/recomp tools/disasm`: the PR branch 526 passed, 38 skipped; `rejsrf` 548 passed, 42 skipped.
* Regeneration with seeds: 9,215 functions (0 failed), 74 stubs, dispatch only grows, no refused seeds, no pointer candidates.
* Four 95 s demo runs: no `[recomp]` reports; the six frames identical in three runs (one jittered).
* Win32 Debug and Release builds succeed with zero warnings.
* Regression tools in both configurations: kernel 56/56, D3D8 116/116, DSOUND 48/48, d3d8 check 14,018/0, decomp check 6/0.

## Remaining

* A play check (user): which scenes reach the two paths is unknown; the run-time reports will show.
* The remaining 74: alias bodies branching back (needing a translation with several entries) and data decoded as code.
