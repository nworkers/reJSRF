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
* `0x22F30`은 조건 분기(`jcc`)로만 닿아서 이 pass의 대상이 아니다. 남은 76개 중 "다른 본문 안"에 들어 있다.

## 3. 남은 76개의 분류 (정적)

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
* `0x22F30` is reached only by a conditional branch (`jcc`), so it is outside this pass; it is among the remaining 76, inside another body.

## 3. The remaining 76 (static)

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
