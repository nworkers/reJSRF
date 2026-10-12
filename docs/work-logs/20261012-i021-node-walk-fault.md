# Issue #21 작업 로그 — 장면 노드 순회 fault

작업 지시: [20261012-i021](../work-orders/20261012-i021-node-walk-fault.md) · 설계: [20261012-i021](../design/20261012-i021-node-walk-fault.md)

## 요약

`0x0FE80026` fault의 원인은 xboxrecomp 디스어셈블러였다. 다른 함수 본문 중간으로 가는 분기가 반환 주소만 pop하는 스텁으로 lift되어 스택이 어긋났다. 포크에서 고치고 업스트림 PR [sp00nznet/xboxrecomp#200](https://github.com/sp00nznet/xboxrecomp/pull/200)을 올렸다. 분기 대상 48개가 alias로 복구되어 미해결 스텁이 105개에서 83개로 줄었고, lift 함수는 9,193개다. 부팅은 이제 fault 없이 프레임 루프를 계속 돈다(헤드리스 5분). 그리기는 아직 받기만 하고, 게임은 사운드 버퍼 67개를 만들지만 `Play`는 부르지 않는다.

## 과정

1. `REXBOX_STOP_DUMP`로 fault 시점 게스트 메모리를 받아 스택의 반환 주소를 보았다. fault 함수는 재귀 노드 순회 `0x00011070`이고, 형제 포인터 `[esi+0x30]`을 따라가다 `esi = 0x0FE80022`에서 읽었다.
2. `esi` 손상인지 데이터 손상인지 가르려고 `build/abi`에 `REJSRF_RECOMP_ABI_CHECK=ON` 트리를 따로 만들었다. 첫 위반은 `call 0x0007E180`이 반환 주소를 pop하지 않은 것이었다.
3. lift 본문에서 `0x0007E180`의 `jcc` 네 개가 미해결 스텁 `sub_0007E255`(`g_esp += 4`)를 부르는 것을 확인했다. 실제 `0x0007E255`는 공유 꼬리 `pop esi; ret`다. `functions.json`에서 `0x0007E180`은 빈 구간의 alias, `0x0007E242`는 `gap_prologue` 함수였다.
4. 첫 수정(조건 분기 고아 패스에서 다른 본문 중간을 alias로)은 효과가 없었다. 그 패스는 `_pass_gap_prologues` 전에 돌아 그때는 `0x0007E255`가 같은 본문 안이었다. 둘째 수정(본문이 확정된 뒤의 새 패스)도 처음에는 효과가 없었다. 출발점 `0x0007E180`이 함수 본문이 아니라 alias였기 때문이다. 최종 수정 `_pass_branches_into_bodies`는 함수 본문과 alias 범위를 모두 출발 구간으로 보고, 새 alias가 생기면 다시 돈다.
5. xboxrecomp 테스트: 새 `test_branches_into_bodies.py` 9건, `pytest tools/disasm tools/recomp` 통과(한국어 코드 페이지에서는 기존 FPU 테스트 하나가 인코딩 때문에 실패하므로 `PYTHONUTF8=1`로 실행). 포크의 업스트림 기준 브랜치 `fix/cond-branch-into-body`(`65648d41`)에 커밋하고, `rejsrf`에 병합했다(`d79f8bb9`, `detect_all` 충돌은 두 패스를 모두 두고 새 패스를 먼저).
6. 부팅: fault가 사라졌다. 프레임 루프가 도는지 추적 없이 보려고 `REXBOX_FRAME_LOG=<n>`을 더했다. 5분에 약 18만 5천 present였다. 4번 실행 중 1번만 미션·스테이지·캐릭터 자원을 읽었고(데모 같은 경로로 추정), 그 실행에서 크기 0 `NtAllocateVirtualMemory` 실패가 한 번 있었다.
7. 정정: 앞서 `Play` 20번으로 센 것은 `Player` 경로가 섞인 잘못된 집계였다. `[dsound] Play` 줄은 0개다.

## 검증

| 검증 | 결과 |
|---|---|
| xboxrecomp `pytest tools/disasm tools/recomp` | 522 통과 |
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` | 42/42, 48/48 |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 6/6, 13,557/0 |
| `check_hle_pops.py` | 불일치 0 |
| 부팅(헤드리스, `REXBOX_AUDIO=none`) | 5분 동안 fault 없음, 프레임 루프 계속 |

## 남은 일

* 다음 task 후보: D3D8 그리기(정점·인덱스 버퍼, 셰이더, 텍스처, `Draw*`)와 첫 `Play`가 나오는 조건.
* 데모 경로에서 본 크기 0 `NtAllocateVirtualMemory` 실패.
* 미해결 스텁 83개, 업스트림 PR #198·#199·#200 머지 뒤 submodule 복귀.

---

# Issue #21 work log — the scene node walk fault

Work order: [20261012-i021](../work-orders/20261012-i021-node-walk-fault.md) · Design: [20261012-i021](../design/20261012-i021-node-walk-fault.md)

## Summary

The `0x0FE80026` fault came from the xboxrecomp disassembler: branches into the interior of another function were lifted as stubs that pop only a return address, which skewed the stack. It is fixed on the fork and sent upstream as [sp00nznet/xboxrecomp#200](https://github.com/sp00nznet/xboxrecomp/pull/200). 48 branch targets were recovered as aliases, the unresolved stubs went from 105 to 83, and 9,193 functions are lifted. Boot now keeps running its frame loop with no fault (5 minutes headless). Drawing is still accepted but not drawn, and the game creates 67 sound buffers but calls no `Play`.

## Process

1. A guest memory dump at the fault (`REXBOX_STOP_DUMP`) and the return addresses on the stack located the faulting function: the recursive node walk at `0x00011070`, which read from `esi = 0x0FE80022` while following the sibling pointer `[esi+0x30]`.
2. To tell a clobbered `esi` from bad data, a separate `REJSRF_RECOMP_ABI_CHECK=ON` tree was built in `build/abi`. The first violation was `call 0x0007E180` returning without popping its return address.
3. The lifted body showed `0x0007E180`'s four `jcc` calling the unresolved stub `sub_0007E255` (`g_esp += 4`), while the real `0x0007E255` is the shared tail `pop esi; ret`. In `functions.json`, `0x0007E180` was an alias in a gap and `0x0007E242` a `gap_prologue` function.
4. The first fix (aliasing other bodies' interiors in the conditional-branch orphan pass) had no effect: that pass runs before `_pass_gap_prologues`, when `0x0007E255` was still in the same body. The second (a new pass on final bodies) had none at first either, because the source `0x0007E180` was an alias, not a function body. The final `_pass_branches_into_bodies` takes both function bodies and alias ranges as sources and repeats when it adds aliases.
5. xboxrecomp tests: the new `test_branches_into_bodies.py` (9 cases); `pytest tools/disasm tools/recomp` passes (run with `PYTHONUTF8=1`, since one existing FPU test fails on the Korean code page because of file encoding). Committed on the fork's upstream-based branch `fix/cond-branch-into-body` (`65648d41`) and merged into `rejsrf` (`d79f8bb9`; the conflict in `detect_all` keeps both passes, the new one first).
6. Boot: the fault is gone. `REXBOX_FRAME_LOG=<n>` was added to see the frame loop without a trace: about 185,000 presents in 5 minutes. Only one run in four read mission, stage and character assets (a demo-like path, inferred), and that run had one zero-size `NtAllocateVirtualMemory` failure.
7. Correction: the earlier count of 20 `Play` calls was wrong; it matched `Player` paths. There are no `[dsound] Play` lines.

## Verification

The table in the Korean section applies: xboxrecomp's pytest 522 passing; Debug and Release builds with zero warnings on our targets; kernel probe 42/42, DSOUND probe 48/48; decomp check 6/6; d3d8 check 13,557/0; `check_hle_pops.py` 0 mismatches; a headless boot with `REXBOX_AUDIO=none` running 5 minutes with no fault and the frame loop going.

## Remaining

* Next task candidates: D3D8 drawing (vertex and index buffers, shaders, textures, `Draw*`) and what makes the first `Play` happen.
* The zero-size `NtAllocateVirtualMemory` failure seen on the demo path.
* The 83 unresolved stubs; return the submodule to upstream once PRs #198, #199 and #200 are merged.
