# 함수 발견: xboxrecomp 디스어셈블러가 JSRF에서 놓치는 모양

xboxrecomp의 `tools.disasm`이 JSRF US `default.xbe`에서 찾는 함수·alias 항목의 수와, 버전에 따라 놓치는 모양을 모은다. 수치는 모두 같은 seed(`src/game/jsrf/recomp/seed_functions.json`, `seed_pointer_targets.json`)로 잰 것이다. 근거: [설계 i093](../design/20261020-i093-xboxrecomp-v0.15.md), [issue #93](https://github.com/nworkers/reJSRF/issues/93).

## 1. 버전별 항목 수 (확인됨)

| xboxrecomp | 항목 수 | 비고 |
|---|---|---|
| v0.13.1 + 포크 `rejsrf` `1991221` | 9,063 | 당시 핀 |
| v0.14.0 | 9,133 | #93. 113개 vtable 메서드 누락 |
| v0.15.0 `6376eb8` | 9,372 | 같은 113개 중 10개만 항목. 41개는 어떤 본문에도 안 덮임 |
| v0.15.0 + 설계 i093 4절 (trial 3) | 9,481 | 113개 중 111개 항목. v0.15.0이 받던 자료 항목 8개도 거절 |
| fork `rejsrf` `5aae5df`(위와 같은 디스어셈블러) 재생성 | 9,481 (lift 9,526, 수동 223 포함) | 미해결 스텁 74 → 28 ([unresolved-stubs §4](unresolved-stubs.md)) |
| fork `rejsrf` `6acdc0c`(v0.16.0 병합, i115) 재생성 | 9,297 (lift 9,359, 수동 223 포함) | 현재 핀. 미해결 스텁 28 → 19 ([unresolved-stubs §5](unresolved-stubs.md)). 미국판·일본판 같음 |

## 2. vtable 메서드가 버려지는 모양 (확인됨)

v0.14.0부터 표 항목(`_pass_data_ptr_targets`)은 빈 구간에서 **닫힌 CFG**(`probes_as_callback_body`)여야 한다. JSRF의 메서드는 다음 모양이라 거절됐다.

| 모양 | 수 | 예 |
|---|---|---|
| guarded vcall tail: `test eax,eax; je done; …; mov eax,[ecx]; jmp [eax+N]` | 89 | `0x45DB0`, `0x162490`, `0x1780D0`, `0x7BDD0`, `0x1773E0` |
| 다른 메서드로 가는 dispatcher: `mov eax,[ecx+N]; jmp [eax*4+T]`. 표 뒤의 바이트 인덱스 표가 가짜 4번째 arm(`0x20200`)으로 측정됨 | 9 | `0x70C90`, `0x100770`, `0x101180` |
| 오류 경로가 아직 모르는 helper로 `jcc` 뒤 `jmp` | 8 | `0x72760`→`0x72830`, `0x71100`→`0x6FE40` |
| 부모 ebp를 읽는 EH funclet | 1 | `0x17E580` |

* 모두 16바이트 정렬이고 패딩 뒤에서 시작한다. 참조하는 표는 `.rdata`의 vtable(`0x1CA498`…`0x1E4328`)이다.
* **확인됨**: 세 데모 정지 주소는 모두 첫 모양이다. 없으면 vtable 호출이 미해결 간접 호출이 되어 데모가 `0x1773E0` 뒤 null 호출로 fault한다(#93).

## 3. 바이트 인덱스 표 (확인됨)

MSVC 두 단계 switch는 dword 표 바로 뒤에 바이트 인덱스 표를 둔다(`movzx eax, byte ptr [eax + T2]`). 그 바이트는 작은 arm 번호(`00 02 02 00`)라 dword로 읽으면 .text 안 주소다.

* `resync_jump_tables`가 이를 arm으로 센다(2절 둘째 줄).
* 표 항목 pass가 바이트 표 자체를 코드로 받아들일 수 있다. `0xEC090`(`movzx` at `0xEBE81`), `0xFFB60`, `0x11A1DC`가 그랬다. 참조는 `movzx`의 displacement로, 그 메서드가 아직 빈 구간이라 word로 스캔된다.
* 거절 규칙: `engine.byte_tables`에 있거나 첫 바이트가 `0x0F` 미만이면 함수가 아니다. 이 규칙은 v0.15.0 그대로가 받아들이던 자료 항목 8개(`0x20F04`, `0x37434`, `0xD7EA0`, `0xE0A08`, `0xF0D9C`, `0x10C254`, `0x112441`, `0x115F88`)도 거른다.

## 3a. v0.16.0의 패딩 뒤 함수 (확인됨, i115)

* v0.16.0(#239)은 `ret` 뒤 패딩 다음의 시작을 다시 판단한다. 미국판에서 v0.15의 `gap_prologue` 178개와 `tail_jump_alias` 20개가 함수에서 빠져 앞 함수 본문에 들어갔다. 직접 호출되는 것은 없었고, 이미지 안에 포인터로 나오는 14개도 대부분 바로 뒤 switch 점프 표의 항목이다(예: `0x765FA`). 새 함수는 14개, 끝이 바뀐 함수는 2,097개다.
* 이름으로 쓰는 225개(seed, 디컴파일 함수, 추가 심볼, 프로파일)는 모두 남는다.
* **확인됨**: 반대 방향의 오판이 하나 있다. `0x115F80`은 `ret 4`와 nop 패딩 뒤의 점프 표(`0x115F57`, `0x115F7C`)와 바이트 인덱스 표인데, v0.16.0이 `gap_prologue` 함수로 읽었다. 그 안의 가짜 `jl 0x115FE5`가 새 미해결 스텁이 된다. 자료라 실행되지 않는다. upstream에 알릴 후보다(보류 중인 결정에 따라 PR은 내지 않음).

## 4. 미확정

* `0x129180`: i058의 사례. `gap_prologue` `0x129367`이 빈 구간을 잘라 switch 표가 밖에 놓인다. seed로 유지.
* `0x17E580`: EH funclet. 표(`0x1E4328`)가 가리키지만 호출 규약이 다르다. 실행 중 닿는지 미확인.
* **확인됨**(재생성 뒤): lifter는 `0x70C90`의 표 `0x70E10`을 arm 3개(`0x70CB1`, `0x70E01`, `0x70E08`)로 번역하고 나머지는 `RECOMP_ITAIL`로 보낸다. 바이트 표 word `0x20200`은 arm이 아니다.

---

# Function discovery: the shapes xboxrecomp's disassembler misses on JSRF

Entry counts (functions and aliases) that xboxrecomp's `tools.disasm` finds on JSRF US `default.xbe`, and the shapes each version misses. Every number uses the same seeds (`src/game/jsrf/recomp/seed_functions.json`, `seed_pointer_targets.json`). Evidence: [design i093](../design/20261020-i093-xboxrecomp-v0.15.md), [issue #93](https://github.com/nworkers/reJSRF/issues/93).

## 1. Entries per version (confirmed)

| xboxrecomp | Entries | Note |
|---|---|---|
| v0.13.1 + fork `rejsrf` `1991221` | 9,063 | the pin then |
| v0.14.0 | 9,133 | #93: 113 vtable methods missing |
| v0.15.0 `6376eb8` | 9,372 | only 10 of the same 113 are entries; 41 are covered by no body |
| v0.15.0 + design i093 section 4 (trial 3) | 9,481 | 111 of the 113; also refuses eight data entries v0.15.0 accepted |
| fork `rejsrf` `5aae5df` (same disassembler), regenerated | 9,481 (lift 9,526 with 223 manual) | unresolved stubs 74 → 28 ([unresolved-stubs §4](unresolved-stubs.md)) |
| fork `rejsrf` `6acdc0c` (v0.16.0 merged, i115), regenerated | 9,297 (lift 9,359 with 223 manual) | current pin; unresolved stubs 28 → 19 ([unresolved-stubs §5](unresolved-stubs.md)); US and Japanese the same |

## 2. How vtable methods get dropped (confirmed)

Since v0.14.0 a table entry (`_pass_data_ptr_targets`) in a gap must be a **closed CFG** (`probes_as_callback_body`). JSRF's methods have these shapes and were refused.

| Shape | Count | Examples |
|---|---|---|
| Guarded vcall tail: `test eax,eax; je done; …; mov eax,[ecx]; jmp [eax+N]` | 89 | `0x45DB0`, `0x162490`, `0x1780D0`, `0x7BDD0`, `0x1773E0` |
| Dispatcher into other methods: `mov eax,[ecx+N]; jmp [eax*4+T]`, with the byte index table after the table measured as a false fourth arm (`0x20200`) | 9 | `0x70C90`, `0x100770`, `0x101180` |
| An error path reaching an unknown helper by a `jmp` after a `jcc` | 8 | `0x72760`→`0x72830`, `0x71100`→`0x6FE40` |
| An EH funclet reading the parent's ebp | 1 | `0x17E580` |

* All are 16-byte aligned and start after padding. The referencing tables are `.rdata` vtables (`0x1CA498`…`0x1E4328`).
* **Confirmed**: the three demo stop addresses are all the first shape. Without them the vtable calls become unresolved indirect calls, and the demo faults after a null call past `0x1773E0` (#93).

## 3. Byte index tables (confirmed)

MSVC's two-level switch packs a byte index table right after the dword table (`movzx eax, byte ptr [eax + T2]`). Its bytes are small arm numbers (`00 02 02 00`), which read as a dword are an address inside .text.

* `resync_jump_tables` counts it as an arm (section 2, second row).
* The table-entry pass can accept the byte table itself as code: `0xEC090` (`movzx` at `0xEBE81`), `0xFFB60` and `0x11A1DC` did. The reference is the `movzx`'s displacement, scanned as a word while its method is still a gap.
* Refusal rule: an address in `engine.byte_tables`, or whose first byte is below `0x0F`, is not a function. The rule also drops eight data entries plain v0.15.0 accepted (`0x20F04`, `0x37434`, `0xD7EA0`, `0xE0A08`, `0xF0D9C`, `0x10C254`, `0x112441`, `0x115F88`).

## 3a. v0.16.0's functions past padding (confirmed, i115)

* v0.16.0 (#239) judges starts after a `ret` and its padding anew. In the US build, v0.15's 178 `gap_prologue` and 20 `tail_jump_alias` starts left the function list and joined the preceding bodies. None is called directly, and the 14 that appear as pointers in the image are mostly entries of the switch jump table right after them (`0x765FA`, for example). 14 functions are new and 2,097 changed their end.
* All 225 named addresses (seeds, decompiled functions, extra symbols, the profile) remain.
* **Confirmed**: one misjudgement goes the other way. `0x115F80` is a jump table (`0x115F57`, `0x115F7C`) with a byte index table after a `ret 4` and nop padding, which v0.16.0 reads as a `gap_prologue` function; its false `jl 0x115FE5` becomes a new unresolved stub. Being data, it never runs. A candidate to tell upstream (no PR, by the standing decision).

## 4. Unresolved

* `0x129180`: i058's case. `gap_prologue` `0x129367` cuts its gap and leaves the switch tables outside. Kept as a seed.
* `0x17E580`: an EH funclet. A table (`0x1E4328`) names it but its calling convention differs. Whether it runs is unverified.
* **Confirmed** (after regeneration): the lifter translates `0x70C90`'s table `0x70E10` as three arms (`0x70CB1`, `0x70E01`, `0x70E08`) with `RECOMP_ITAIL` for the rest; the byte-table word `0x20200` is not an arm.
