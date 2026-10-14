# 미해결 스텁

**미해결 스텁**은 lift된 코드가 부르거나 점프하지만 디스어셈블러가 함수로 찾지 못한 주소다. 생성기는 반환 주소(와 `ret N`의 인자)만 pop하는 함수를 만든다. 스텁이 실행되면 원본이 그 주소에서 하던 일이 빠진다. 목록은 `gen/jsrf_us/gen/recomp_stubs_unresolved.c`에 있고, 재생성할 때마다 바뀔 수 있다.

## 1. 실행 중 보고 (i055부터)

* 스텁은 `RECOMP_UNRESOLVED_STUB(va)`를 부른다. reJSRF는 `RECOMP_STUB_REPORT`를 정의하므로, 스텁이 처음 불릴 때 한 번 보고한다.

  ```
  [recomp] unresolved stub <va> called; its code was skipped (returns to <ret>)
  ```

* **확인됨**(i055, RTX 4090): 로고와 데모 95초 동안 보고가 0건이었다.

## 2. 고친 것: 늦게 생긴 본문의 tail jump

* **확인됨**(i055): i055 전의 스텁 83개 가운데 7개가 진짜 함수였다. 모두 패딩 뒤 정렬된 주소에서 시작하고, tail jump로만 닿았다.
* 점프는 tail-jump pass(pass 6) 뒤에 생긴 본문(`gap_prologue`, alias) 안에 있어서, 어느 pass도 보지 않았다.
* 그중 `0x00054750`은 데모의 매 프레임 실행된다. 상태 표 `0x1F9888`의 처리 함수 `0x0005BB31`이 그 함수로 tail jump한다.
* xboxrecomp fork의 `_pass_late_tail_jumps`가 무조건 `jmp`인 6개(0x54750, 0x75E90, 0xA5B60, 0xB8A60, 0xC67A0, 0x11B680)와 `0x1295BC`를 alias로 되찾았다. 남은 스텁은 76개다.
* **확인됨**: 다시 생성한 뒤 `0x54750`이 실행된다(데모 95초에 1,000번 이상). 프레임 300·1,200·2,400·3,000·3,600·4,800은 바이트 단위로 같았다.
* **확인됨**(사용자 플레이, i055 빌드): New Game 뒤 첫 대화가 끝난 다음의 화면 전환이 전에는 제대로 되지 않았다. 지금은 제대로 된다.
  * 원인 함수를 가리려고 i055 생성 코드에서 일부 함수를 다시 스텁으로 되돌린 시험 빌드로 같은 장면을 플레이했다(사용자).
    * 7개를 모두 되돌리면 원래 증상이 재현됐다. 보고된 스텁은 `0x54750`과 `0xA5B60`뿐이었다.
    * `0xA5B60`만 되돌리면 증상이 달라졌지만 화면 전환은 여전히 되지 않았다.
  * **확인됨**: `0xA5B60`이 없으면 화면 전환이 되지 않는다. 데모에서는 불리지 않고, 이 장면에서 처음 불린다(`0x000A28C8`로 돌아감).
  * **추정**: 두 시험 빌드의 차이는 `0x54750`의 실행 여부뿐이므로, 증상이 달라진 것은 `0x54750` 때문이다.
* `0x22F30`은 조건 분기(`jcc`)로만 닿아서 이 pass의 대상이 아니다. 남은 76개 중 "다른 본문 안"에 들어 있다.

## 2a. 간접 호출 대상 누락: `0x352D0`

* **확인됨**(i055, New Game 플레이 로그): `[recomp] indirect call to 000352D0 has no target; skipped (returns to 0002576A)`.
  * 함수 포인터 표 `0x1EC288`(앞 34칸이 함수 포인터)의 10번 칸이다. `0x00025768`의 `call edx`로 불린다.
  * 진짜 함수다(`push ebx; push esi; mov esi, ecx`, thiscall). 바로 앞에 인라인 바이트 표가 있고, 그 표와 함수가 alias `sub_000348A0`의 범위(끝 `0x35560`) 안에 묻혀서 디스패치 항목이 없었다.
  * 표의 나머지 33칸과 상태 표 `0x1F9888`의 119칸은 모두 디스패치에 있다.
* **고침**: `seed_functions.json`에 `observed` seed로 넣었다(issue #19의 `0x7BE30`과 같은 모양). 다시 생성하면 디스패치 항목이 하나 늘고(9,203 함수), 다른 출력은 같다.
* **확인됨**(사용자 플레이): seed를 넣은 빌드로 New Game부터 같은 장면까지 정상으로 진행됐고, `[recomp]` 보고(스텁, 간접 호출 누락)는 0건이었다.
* 이 호출은 seed 전의 i055 빌드에서도 건너뛰어졌다. 화면 전환 시험 빌드 셋 모두에서 같았으므로 화면 전환과는 관계없다.
* **미확정**:
  * 이 함수가 빠져서 생기던 증상
  * 디스어셈블러가 alias 범위 안의 표 항목을 놓친 원인(데이터 포인터 pass의 조건)

## 2b. 늦은 본문의 조건 분기 (i058)

* **확인됨**(정적): 위치가 애매했던 세 곳을 확인했다.
  * `0x22F30`은 `sub_00022E50` 안의 루프 머리다. 원래 함수는 `goto`로 처리하고, 스텁은 아무도 부르지 않는 alias `sub_00022F7F`에서만 불린다. 영향이 없다.
  * `0x1783FE`는 stdcall 메서드 `sub_001783D0`(`ret 0xc`)의 경로로, `jmp [ecx+0x20]`으로 넘긴다. 스텁이면 esp가 12바이트 어긋났을 것이다.
  * `0x1296C7`은 가상 메서드 `0x129180`의 공통 마무리(`add esp, 0xc04; ret`)다. switch case 4에서 `je`로 닿는다. 스텁이면 프레임이 풀리지 않았을 것이다.
* **원인**:
  * 빈 구간으로 가는 `jcc`를 보는 `_pass_cond_branch_orphans`가 pass 6 안에서 돌아, 그 뒤에 생긴 본문을 보지 못했다.
  * alias 쪽 pass는 간접 `jmp`로 끝나는 블록을 거절했다.
* **고침**(xboxrecomp, [PR #200](https://github.com/sp00nznet/xboxrecomp/pull/200)에 덧붙임):
  * 늦은 pass가 `jcc`도 받는다. 받기 조건은 `_pass_cond_branch_orphans`와 같고, 간접 `jmp`로 끝나는 블록을 더했다.
  * 두 주소는 seed로도 넣었다(사용자 결정).
  * 스텁은 76개에서 74개, 함수는 9,213개가 됐다.
* **lifter 공백(같은 작업에서 드러남)**:
  * 복구된 switch case 함수 안의 중첩 switch는 case 진입점을 얻지 못했다.
  * 예: `0x129180` → case `0x1297EC` → 표 `0x12A5BC`의 8칸
  * 이웃 빈 구간의 본문이 진입점을 우연히 대신 만들어 주던 동안에만 동작했다. `jcc` 수정으로 그 구간이 한 alias가 되자 디스패치 항목 12개가 사라졌다.
  * `discover_jump_table_entries`가 새로 만든 case를 다시 검사하게 했다. 또 복구된 본문에는 "case가 모두 범위 안이면 건너뜀"을 적용하지 않는다.
  * 디스패치는 늘기만 한다(9,203 → 9,213, seed 포함).

## 3. 남은 76개의 분류 (정적)

(i058 뒤에는 74개다. `0x1783FE`, `0x1296C7`이 빠졌다.)

**확인됨**(i055, 정적 분류): 분류는 대상 주소의 위치 기준이다. "호출 수"는 그 스텁을 부르는 lift 코드의 위치 수다.

| 분류 | 개수 | 부르는 쪽(위치 수) |
|---|---|---|
| 다른 함수 본문 안 | 42 | alias 39, `gap_prologue` 7 |
| 명령 경계가 아님 | 28 | `gap_prologue` 15, alias 6, `call_target` 3, `cc_boundary` 2, `seed_vtable_thunk` 2 |
| 어느 섹션에도 없음(예: `0xEAF9ECB8`) | 4 | alias 2, `gap_prologue` 1, 없음 1 |
| 빈 구간, 명령 흐름 중간 | 1 | `gap_prologue` |
| ret/jmp 뒤, 정렬 안 됨 | 1 | alias |

* **다른 본문 안**:
  * **확인됨**: 부르는 쪽-대상 쌍 46개 중 35개는 대상이 부르는 쪽 시작보다 앞에 있다.
  * **추정**: 대부분은 alias 본문(함수 중간 진입점을 따로 번역한 것)이 원래 함수의 alias 시작 앞부분으로 되돌아가는 분기다. 원래 함수 본문에서는 같은 분기가 정상이고, alias로 들어온 실행만 스텁을 탄다.
  * **미확정**: C 함수 하나가 다른 C 함수 본문 중간으로 들어갈 수 없으므로 alias 구조의 한계다. 고치려면 원래 함수에 진입점을 여러 개 두는 번역이 필요하다.
* **명령 경계가 아님, 섹션 밖**:
  * **추정**: 데이터를 코드로 잘못 읽은 본문에서 나온 호출이다.
  * 예를 들어 `0x1C3F86`·`0x1C3F96`을 부르는 `sub_001C3F18`은 `.rdata`의 문자열 `"\Device\MU_0"`이다([unlifted-instructions §1](unlifted-instructions.md)). XPP 섹션의 2개도 같은 부류로 보인다.
* **미확정**: 실제 게임 진행(메뉴, 스테이지, 세이브)에서 어느 스텁이 실행되는지. §1의 보고로 확인한다.

---

# Unresolved stubs

An **unresolved stub** is an address that lifted code calls or jumps to but the disassembler did not find as a function. The generator emits a function that pops only the return address (and the arguments of a `ret N`). When a stub runs, whatever the original does at that address is skipped. The list is in `gen/jsrf_us/gen/recomp_stubs_unresolved.c` and can change with every regeneration.

## 1. Reporting at run time (from i055)

* Each stub calls `RECOMP_UNRESOLVED_STUB(va)`. reJSRF defines `RECOMP_STUB_REPORT`, so a stub reports once, the first time it is called:

  ```
  [recomp] unresolved stub <va> called; its code was skipped (returns to <ret>)
  ```

* **Confirmed** (i055, RTX 4090): no reports over 95 s of logos and demo.

## 2. Fixed: tail jumps from late bodies

* **Confirmed** (i055): 7 of the 83 stubs before i055 were real functions. All start at an aligned address after padding and are reached only by tail jumps.
* The jumps lie in bodies made after the tail-jump pass (pass 6) (`gap_prologue`, aliases), which no pass looked at.
* One of them, `0x00054750`, runs every demo frame: the handler `0x0005BB31` in the state table `0x1F9888` tail-jumps to it.
* The xboxrecomp fork's `_pass_late_tail_jumps` recovered the six reached by an unconditional `jmp` (0x54750, 0x75E90, 0xA5B60, 0xB8A60, 0xC67A0, 0x11B680) and `0x1295BC` as aliases, leaving 76 stubs.
* **Confirmed**: after regenerating, `0x54750` runs (more than 1,000 times in 95 s of the demo), and frames 300, 1,200, 2,400, 3,000, 3,600 and 4,800 are byte-identical.
* **Confirmed** (user play, i055 build): after New Game, the screen transition that follows the first dialogue did not work before and now does.
  * To tell which function did it, the scene was played (by the user) with test builds of the i055 code in which some functions were turned back into stubs:
    * with all seven turned back, the original symptom came back, and the only stubs reported were `0x54750` and `0xA5B60`
    * with only `0xA5B60` turned back, the symptom changed but the screen still did not move on
  * **Confirmed**: without `0xA5B60` the transition does not happen. It is not called in the demo and is first called in this scene (returning to `0x000A28C8`).
  * **Inferred**: the two test builds differ only in whether `0x54750` runs, so the changed symptom comes from `0x54750`.
* `0x22F30` is reached only by a conditional branch (`jcc`), so it is outside this pass; it is among the remaining 76, inside another body.

## 2a. A missed indirect-call target: `0x352D0`

* **Confirmed** (i055, New Game play log): `[recomp] indirect call to 000352D0 has no target; skipped (returns to 0002576A)`.
  * It is entry 10 of the function table `0x1EC288` (its first 34 entries are function pointers), called by `call edx` at `0x00025768`.
  * It is a real function (`push ebx; push esi; mov esi, ecx`, thiscall). An inline byte table sits right before it, and both lay inside the range of the alias `sub_000348A0` (ending at `0x35560`), so it had no dispatch entry.
  * The other 33 entries of that table and the 119 of the state table `0x1F9888` are all in the dispatch table.
* **Fixed**: an `observed` seed in `seed_functions.json`, the same shape as issue #19's `0x7BE30`. Regenerating adds one dispatch entry (9,203 functions) and changes nothing else.
* **Confirmed** (user play): with the seed, New Game through the same scene played normally, with no `[recomp]` reports (stubs or missed indirect calls).
* The call was skipped in the i055 build before the seed too, and in all three transition test builds alike, so it is unrelated to the transition.
* **Unresolved**:
  * what symptom its absence caused
  * why the disassembler missed a table entry inside an alias range (the conditions of the data-pointer pass)

## 2b. Conditional branches from late bodies (i058)

* **Confirmed** (static): three unclear stubs were checked.
  * `0x22F30` is a loop head inside `sub_00022E50`. The function handles it with a `goto`, and the stub is called only from the alias `sub_00022F7F`, which nothing calls. No effect.
  * `0x1783FE` is a path of the stdcall method `sub_001783D0` (`ret 0xc`) that forwards through `jmp [ecx+0x20]`. As a stub, esp would come back 12 off.
  * `0x1296C7` is the shared epilogue of the virtual method `0x129180` (`add esp, 0xc04; ret`), reached by `je` from switch case 4. As a stub, the frame would never be unwound.
* **Cause**:
  * `_pass_cond_branch_orphans`, which takes a `jcc` into a gap, runs inside pass 6 and never saw later bodies.
  * The alias pass refused blocks ending in an indirect `jmp`.
* **Fixed** (xboxrecomp, added to [PR #200](https://github.com/sp00nznet/xboxrecomp/pull/200)):
  * The late pass takes a `jcc` too, with `_pass_cond_branch_orphans`'s acceptance test extended to blocks ending in an indirect `jmp`.
  * Both addresses were also seeded (the user's decision).
  * Stubs went from 76 to 74, and functions to 9,213.
* **A lifter gap found on the way**:
  * A nested switch inside a recovered switch arm got no arm entries.
  * Example: `0x129180` → arm `0x1297EC` → the 8 arms of table `0x12A5BC`.
  * It worked only while a neighbouring gap body happened to supply the entries. Once the `jcc` fix made that region one alias, 12 dispatch entries disappeared.
  * `discover_jump_table_entries` now scans the arms it adds, and a recovered body no longer gets the "all arms in range" skip.
  * The dispatch table only grows (9,203 → 9,213, with the seeds).

## 3. The remaining 76 (static)

(74 after i058, with `0x1783FE` and `0x1296C7` gone.)

**Confirmed** (i055, static classification), by where the target lies; "callers" counts the lifted call sites:

* inside another function body: 42 (callers: aliases 39, `gap_prologue` 7)
* not on an instruction boundary: 28 (`gap_prologue` 15, aliases 6, `call_target` 3, `cc_boundary` 2, `seed_vtable_thunk` 2)
* outside every section (for example `0xEAF9ECB8`): 4 (aliases 2, `gap_prologue` 1, none 1)
* in a gap, mid-stream: 1 (`gap_prologue`)
* unaligned after ret/jmp: 1 (alias)

Notes:

* **Inside another body**:
  * **Confirmed**: in 35 of the 46 caller-target pairs the target lies before the caller's start.
  * **Inferred**: most are alias bodies (a function's interior entry translated separately) branching back to the part of their function before the alias start. In the function's own body the same branch is fine; only execution that entered through the alias takes the stub.
  * **Unresolved**: this is a limit of aliases, since one C function cannot enter the middle of another's body. A fix needs a translation that gives the original function several entry points.
* **Not on an instruction boundary, outside every section**:
  * **Inferred**: calls from bodies that are data decoded as code.
  * For example, `sub_001C3F18`, which calls `0x1C3F86` and `0x1C3F96`, is the `.rdata` string `"\Device\MU_0"` ([unlifted-instructions §1](unlifted-instructions.md)), and the two in the XPP section look the same.
* **Unresolved**: which stubs run in real play (menus, stages, saves); §1's reports will show.
