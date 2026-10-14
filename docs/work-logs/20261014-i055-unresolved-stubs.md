# Issue #55 작업 로그 — 미해결 스텁이 놓친 함수를 말없이 건너뛰는 문제

작업 지시: [20261014-i055](../work-orders/20261014-i055-unresolved-stubs.md) · 설계: [20261014-i055](../design/20261014-i055-unresolved-stubs.md) · 분석: [unresolved-stubs](../analysis/unresolved-stubs.md)

## 요약

"XBE를 C로 옮기는 작업이 완료되었는가"를 확인하다가, 미해결 스텁 83개가 실행되어도 보고하지 않는다는 것을 알았다. 그중 하나(`0x00054750`)는 데모의 매 프레임 실행되는 진짜 함수였고, 본문이 통째로 건너뛰어지고 있었다.

* **원인**: xboxrecomp 디스어셈블러의 pass 순서다. tail-jump pass 뒤에 생긴 본문의 tail jump를 아무 pass도 보지 않았다.
* **고친 것**:
  * xboxrecomp fork에 늦은 tail-jump pass를 더했다. 7개 함수를 되찾아 스텁이 83개에서 76개로 줄었다.
  * 스텁이 실행되면 보고하게 했다.
  * 남은 76개를 분류했다.
* **결과**: `0x54750`이 이제 실행된다. 데모 프레임 6장은 바이트 단위로 같았다.
* **upstream PR**: 준비만 하고 보류했다(사용자 결정). 초안은 아래에 있다.

## 과정

1. **실행되는 스텁 찾기**(i051 후속 확인, 임시 계측):
   * 생성된 스텁에 첫 호출 기록을 넣고 로고·데모를 120초 돌렸다.
   * `sub_00054750` 하나가 불렸다(반환 주소 `0x00051FF9`).
2. **원인 추적**:
   * `sub_00051FC0`이 상태 표 `0x1F9888`(119칸)로 처리 함수를 부른다. `sub_0005BB31`은 끝에서 `jmp 0x54750`으로 넘어간다.
   * `0x54750`은 nop 패딩 뒤 정렬된 함수 시작이지만 어느 함수에도 속하지 않았다.
   * 정적 분류로 같은 모양 7개를 찾았다. 점프 위치는 모두 `gap_prologue`(3)나 `tail_jump_alias`(4) 본문 안이었다. 이 본문들은 tail-jump pass 뒤에 생긴다.
3. **기준 프레임**: 바꾸기 전 빌드로 프레임 300·1,200·2,400·3,000·3,600·4,800을 두 번 덤프했다. 두 실행이 바이트 단위로 같았다.
4. **xboxrecomp fork**:
   * 기준 확인: fork의 `origin/main`은 upstream main(`193e299`)과 같았다. `_pass_branches_into_bodies`와 `_pass_alias_branch_orphans`는 아직 열린 upstream PR #200·#199에만 있다.
   * `fix/late-tail-jumps`(`58f7166`): PR #200 브랜치(`fix/cond-branch-into-body`) 위에 `_pass_late_tail_jumps`와 테스트 4개를 더했다.
   * `feat/stub-report`(`6a38c01`): upstream main 위에서 스텁에 `RECOMP_UNRESOLVED_STUB(va)`를 넣었다. `recomp_types.h`에서 `RECOMP_STUB_REPORT`일 때 `recomp_unresolved_stub_log(va)`를 부르고, 아니면 비운다. 스텁 한 줄을 `_stub_definition`으로 분리해 테스트했다.
   * `rejsrf`에 둘을 합쳤다(`ae96816`, `2e73f7c`).
     * 첫 병합은 `_pass_alias_branch_orphans` 호출 자리에서 충돌했다.
     * 설계대로 새 pass를 그 앞에 두었다.
   * upstream remote(`sp00nznet/xboxrecomp`)는 읽기용으로만 더했다.
5. **테스트 환경**:
   * 이 기계의 Python에 pytest가 없어 `pip install --user pytest`로 설치했다.
   * cp949 기본 인코딩 때문에 한 테스트가 깨져서 `PYTHONUTF8=1`로 실행했다.
   * C 컴파일러(clang/gcc)가 PATH에 없어, 매크로를 컴파일해 보는 테스트 2개는 건너뛰었다. 대신 실제 빌드로 확인했다(아래).
6. **reJSRF**:
   * `rejsrf_recomp`에 `RECOMP_STUB_REPORT`를 정의했다.
   * `recomp_bridge.cpp`에 `recomp_unresolved_stub_log`를 더했다(주소마다 한 번, 반환 주소 포함).
7. **다시 생성**(1분 36초):
   * 디스어셈블러 함수는 9,043개에서 9,050개, 번역 함수는 9,193개에서 9,202개, 스텁은 83개에서 76개가 됐다.
   * 로그의 "76 tail-jump target(s) from late bodies"는 이 pass가 등록한 alias 수다. 그중 69개는 전에도 `_pass_alias_branch_orphans`가 같은 범위로 되찾던 것이다. 이번에는 새 pass가 먼저 잡았을 뿐이다. 새로 생긴 함수는 7개다.
   * 끝이 바뀐 함수는 2개(`0xA5AF0`, `0x11B660`)다. 둘 다 `ret`과 nop 패딩으로 끝나는데, 전에는 끝이 그 뒤의 놓친 함수까지 넘어가 측정됐다. 지금은 새 함수 시작에서 끊긴다. 맞는 경계다.
   * `0x22F30`은 조건 분기로만 닿아 남았다(설계상 무조건 `jmp`만 받음).
8. **실행 확인**:
   * 임시 계수(커밋하지 않음)로 `sub_00054750`이 95초 동안 1,000번 이상 불리는 것을 봤다.
   * 스텁 보고는 0건이었고, 프레임 6장은 기준과 바이트 단위로 같았다.
   * `0x54750`의 동작이 이 데모 화면에는 드러나지 않는다는 뜻이다.
   * 보고 경로가 링크된 것은 `rejsrf.map`의 `recomp_unresolved_stub_log`로 확인했다.
   * 끝에서 끝까지: 시작할 때 스텁 `sub_220FC7E7`을 한 번 직접 부르게 임시로 고쳐(커밋하지 않음) 실행하자 `[recomp] unresolved stub 220FC7E7 called; ...`가 나왔다.
9. **남은 76개 분류**([unresolved-stubs §3](../analysis/unresolved-stubs.md)):
   * 다른 본문 안 42, 명령 경계 아님 28, 섹션 밖 4, 기타 2
   * "다른 본문 안" 46쌍 중 35쌍은 alias 본문이 자기 시작 앞쪽으로 되돌아가는 분기다(**추정**: alias 구조의 한계).
10. **게임 진행 확인**(사용자): New Game 뒤 첫 대화가 끝난 다음의 화면 전환이 전에는 제대로 되지 않았는데, i055 빌드에서는 제대로 된다. 되찾은 7개 중 어느 함수 덕분인지는 따로 확인하지 않았다.
11. **원인 함수 가리기**(사용자 플레이):
    * 생성 코드를 `gen/jsrf_us_stubtest`로 복사했다. 일부 함수 본문 맨 앞에 `RECOMP_UNRESOLVED_STUB(va); g_esp += 4; return;`을 넣어 i055 전 스텁과 같게 했다. 이것을 별도 빌드(`build/win32_x86_stubtest`)로 만들었다.
    * 7개 모두 되돌림: 원래 증상이 재현됐다. 보고는 `0x54750`, `0xA5B60`이었다.
    * `0xA5B60`만 되돌림: 증상이 달라졌지만 해결되지 않았다.
    * 결론: `0xA5B60`이 필요하다(**확인됨**). 달라진 증상은 `0x54750` 때문이다(**추정**).
    * PowerShell의 `2>` 리디렉션은 로그를 UTF-16으로 저장한다. 그래서 처음에 `grep`이 보고를 찾지 못했다.
12. **`0x352D0`**: 1차 시험 로그에 `indirect call to 000352D0 has no target`이 있었다.
    * 표 `0x1EC288`의 10번 칸인 진짜 함수가 alias 범위 안에 묻혀 디스패치 항목이 없었다. i055 평소 빌드에서도 같다.
    * `observed` seed로 넣고 다시 생성했다. 디스패치 항목이 하나 늘었고(9,203), 스텁은 76개 그대로다.
    * 데모 프레임은 세 번 실행 중 첫 번째에서 3,000·4,800이 달랐다. 빌드 직후라 기계가 바빴던 탓으로 본다. 나머지 두 번은 6장 모두 기준과 같았다.

## 검증

| 검증 | 결과 |
|---|---|
| xboxrecomp `pytest tools/recomp tools/disasm` | `fix/late-tail-jumps` 519 통과·38 건너뜀, `feat/stub-report` 508 통과·40 건너뜀, `rejsrf` 541 통과·42 건너뜀 |
| 다시 생성 | 9,202 함수(실패 0), 스텁 76 |
| 로고·데모 95초 | `0x54750` 1,000번 이상 실행, 스텁 보고 0 |
| 프레임 덤프 6장 | 바꾸기 전과 바이트 단위로 같음 |
| Win32 Debug / Release 빌드 | 성공, 경고 0 |
| 게임 진행(사용자) | New Game 뒤 첫 대화 다음 화면 전환이 고쳐짐 |
| 원인 시험(사용자) | `0xA5B60`만 스텁: 해결 안 됨(증상 달라짐), 7개 모두 스텁: 원래 증상 |
| `0x352D0` seed 뒤 | 디스패치 +1(9,203), 데모 프레임 6장 같음(3회 중 2회, 1회는 흔들림), 회귀 도구 통과 |
| 회귀 도구(Debug, Release) | kernel 56/56, d3d8 116/116, dsound 48/48, d3d8 check 14,018/0, decomp check 6/0 |

## upstream PR 초안 (보류)

fork 브랜치를 push한 뒤 검토를 거쳐 올린다.

**PR 1** — `nworkers:fix/late-tail-jumps` → `sp00nznet:main` (PR #200 위에 쌓임; #200이 먼저 머지되어야 함)

* 제목: `disasm: recover tail-jump targets of bodies made after the tail-jump pass`
* 본문:
  > `_pass_tail_jump_targets` only sees the bodies pass 5 built. Functions made later by `_pass_gap_prologues`, and alias bodies, can tail-jump into a gap, and nothing looks at those jumps: `_pass_branches_into_bodies` takes only targets inside another body, and `_pass_alias_branch_orphans` needs a short probe to reach a `ret`, which a long function never does. The target became an unresolved-target stub that pops only a return address, so the whole function never ran.
  >
  > `_pass_late_tail_jumps` takes, on the final bodies and gap aliases, every unconditional `jmp` that leaves its body for a decoded instruction no body claims, and registers it as an alias entry (no candidate, so it cannot cut a body short). Same evidence as pass 6.
  >
  > Jet Set Radio Future: 7 new functions, unresolved stubs 83 → 76. One of them (0x00054750, tail-jumped to from a state handler in a 119-entry table) runs every frame of the attract demo and was being skipped. Frames identical before/after in the demo; in play, the screen transition after the first dialogue of a new game, which used to fail, now works.
  >
  > Tests: `tools/disasm/test_late_tail_jumps.py` (4).

**PR 2** — `nworkers:feat/stub-report` → `sp00nznet:main`

* 제목: `recomp: unresolved-target stubs can report that they ran`
* 본문:
  > A stub stands in for code the disassembler did not find, so a stub that runs means the original executed something the build skipped -- silently, unlike an unresolved indirect call, which `recomp_icall_fail_log` reports.
  >
  > Each stub now calls `RECOMP_UNRESOLVED_STUB(va)` before it returns. With `RECOMP_STUB_REPORT` defined, `recomp_types.h` maps it to `recomp_unresolved_stub_log(va)`, which the project implements; without it the macro is empty, so existing projects build and behave as before.
  >
  > Tests: `tools/recomp/test_stub_report.py` (stub text; the macro compiled both ways when a C compiler is on PATH).

## 남은 일

* fork 브랜치 push. submodule이 가리키는 `2e73f7c`가 원격에 있어야 하므로 이 작업을 머지할 때 한다. upstream PR은 사용자 검토 뒤 올린다.
* 실제 게임 진행에서 스텁 보고를 모은다.
* alias 본문이 자기 시작 앞으로 되돌아가는 분기(35쌍)는 진입점을 여러 개 두는 번역이 필요하다. 별도 issue 후보다.
* `0x22F30`처럼 조건 분기로만 닿는 함수는 이 pass가 받지 않는다. 함수 경계를 자르지 않는 근거가 더 필요하다.

---

# Issue #55 work log — unresolved stubs silently skip functions the disassembler missed

Work order: [20261014-i055](../work-orders/20261014-i055-unresolved-stubs.md) · Design: [20261014-i055](../design/20261014-i055-unresolved-stubs.md) · Analysis: [unresolved-stubs](../analysis/unresolved-stubs.md)

## Summary

Checking whether the XBE → C lift was complete showed that the 83 unresolved stubs say nothing when they run. One of them (`0x00054750`) was a real function that runs every demo frame, with its whole body being skipped.

* **Cause**: the order of the xboxrecomp disassembler's passes. No pass looked at the tail jumps of bodies made after the tail-jump pass.
* **Fixed**:
  * A late tail-jump pass in the xboxrecomp fork recovered 7 functions, taking the stubs from 83 to 76.
  * Stubs now report when they run.
  * The remaining 76 are classified.
* **Result**: `0x54750` now runs, and six demo frames are byte-identical.
* **Upstream PRs**: prepared and held (the user's decision); drafts are in the Korean section.

## Process

1. **Finding stubs that run** (temporary instrumentation):
   * First-call logging was added to the generated stubs, and the logos and demo ran for 120 s.
   * One stub, `sub_00054750`, was called (return address `0x00051FF9`).
2. **Tracing the cause**:
   * `sub_00051FC0` calls a handler through the 119-entry state table `0x1F9888`, and `sub_0005BB31` ends with `jmp 0x54750`.
   * `0x54750` is an aligned function start after nop padding, belonging to no function.
   * Static classification found seven of the same shape. Every jump lies in a `gap_prologue` (3) or `tail_jump_alias` (4) body, bodies that are made after the tail-jump pass.
3. **Baseline frames**: frames 300, 1,200, 2,400, 3,000, 3,600 and 4,800 were dumped twice with the build before the change; the two runs were byte-identical.
4. **xboxrecomp fork**:
   * Base check: the fork's `origin/main` equals upstream main (`193e299`); `_pass_branches_into_bodies` and `_pass_alias_branch_orphans` exist only in the open upstream PRs #200 and #199.
   * `fix/late-tail-jumps` (`58f7166`): `_pass_late_tail_jumps` and four tests, on PR #200's branch (`fix/cond-branch-into-body`).
   * `feat/stub-report` (`6a38c01`): on upstream main, each stub calls `RECOMP_UNRESOLVED_STUB(va)`. In `recomp_types.h` it calls `recomp_unresolved_stub_log(va)` under `RECOMP_STUB_REPORT` and is empty otherwise. The stub line moved into `_stub_definition`, which is tested.
   * Both merged into `rejsrf` (`ae96816`, `2e73f7c`).
     * The first merge conflicted at the `_pass_alias_branch_orphans` call.
     * As designed, the new pass goes before it.
   * The upstream remote (`sp00nznet/xboxrecomp`) was added for fetching only.
5. **Test environment**:
   * This machine's Python had no pytest, so it was installed with `pip install --user pytest`.
   * One test broke on the cp949 default encoding, so the suite ran with `PYTHONUTF8=1`.
   * No C compiler (clang/gcc) is on PATH, so the two tests that compile the macro were skipped; the real build checked it instead (below).
6. **reJSRF**:
   * `RECOMP_STUB_REPORT` is defined on `rejsrf_recomp`.
   * `recomp_bridge.cpp` gained `recomp_unresolved_stub_log` (once per address, with the return address).
7. **Regeneration** (1 min 36 s):
   * Disassembler functions went from 9,043 to 9,050, translated functions from 9,193 to 9,202, and stubs from 83 to 76.
   * The log's "76 tail-jump target(s) from late bodies" counts the aliases this pass registered. 69 of them were recovered before by `_pass_alias_branch_orphans` with the same extents; the new pass only gets there first. Seven functions are new.
   * Two functions changed their end (`0xA5AF0`, `0x11B660`). Both end in `ret` and nop padding, and their ends used to be measured on into the missed function after them; now they stop at its start. The new boundaries are right.
   * `0x22F30` remains, reached only by a conditional branch (by design, only an unconditional `jmp` is taken).
8. **Runs**:
   * A temporary counter (not committed) showed `sub_00054750` called more than 1,000 times in 95 s.
   * There were no stub reports, and the six frames were byte-identical to the baseline.
   * So what `0x54750` does does not show in this demo's frames.
   * That the reporting path is linked was checked through `recomp_unresolved_stub_log` in `rejsrf.map`.
   * End to end: with a temporary change (not committed) calling the stub `sub_220FC7E7` once at startup, the run printed `[recomp] unresolved stub 220FC7E7 called; ...`.
9. **The remaining 76** ([unresolved-stubs §3](../analysis/unresolved-stubs.md)):
   * 42 inside another body, 28 off an instruction boundary, 4 outside every section, 2 others
   * 35 of the 46 caller-target pairs inside another body are an alias body branching back before its own start (**inferred**: a limit of aliases)

10. **Play check** (user): after New Game, the screen transition after the first dialogue did not work before and works in the i055 build. Which of the seven recovered functions fixed it was not checked separately.
11. **Telling which function** (user play):
    * The generated code was copied to `gen/jsrf_us_stubtest`. Some function bodies were made to start with `RECOMP_UNRESOLVED_STUB(va); g_esp += 4; return;`, the stub of before i055, and this was built separately (`build/win32_x86_stubtest`).
    * All seven turned back: the original symptom returned; the reports were `0x54750` and `0xA5B60`.
    * Only `0xA5B60` turned back: the symptom changed but was not resolved.
    * So `0xA5B60` is required (**confirmed**), and the changed symptom comes from `0x54750` (**inferred**).
    * PowerShell's `2>` redirection writes the log as UTF-16, which is why `grep` first found no reports.
12. **`0x352D0`**: the first test log had `indirect call to 000352D0 has no target`.
    * A real function, entry 10 of the table `0x1EC288`, lay inside an alias range and had no dispatch entry; the i055 build is the same.
    * It was added as an `observed` seed and the code regenerated: one more dispatch entry (9,203), still 76 stubs.
    * In the first of three runs, demo frames 3,000 and 4,800 differed, taken to be the machine being busy right after a build; the other two runs matched the baseline in all six.

## Verification

* xboxrecomp `pytest tools/recomp tools/disasm`: `fix/late-tail-jumps` 519 passed, 38 skipped; `feat/stub-report` 508 passed, 40 skipped; `rejsrf` 541 passed, 42 skipped.
* Regeneration: 9,202 functions (0 failed), 76 stubs.
* Logos and demo for 95 s: `0x54750` ran more than 1,000 times; no stub reports.
* Six frame dumps: byte-identical to before.
* Play (user): the screen transition after the first dialogue following New Game is fixed.
* Cause test (user): with only `0xA5B60` as a stub, not resolved (the symptom changed); with all seven, the original symptom.
* After the `0x352D0` seed: one more dispatch entry (9,203); demo frames identical in two of three runs (one run jittered); regression tools pass.
* Win32 Debug and Release builds succeed with zero warnings.
* Regression tools in both configurations: kernel 56/56, D3D8 116/116, DSOUND 48/48, d3d8 check 14,018/0, decomp check 6/0.

## Remaining

* Push the fork branches. The submodule points at `2e73f7c`, which must be on the remote, so this happens when the task is merged; the upstream PRs wait for the user's review.
* Collect stub reports from real play.
* Alias bodies branching back before their own start (35 pairs) need a translation with several entry points; a candidate for its own issue.
* A function reached only by conditional branches, like `0x22F30`, is not taken by this pass; it needs more evidence that does not cut function boundaries.
