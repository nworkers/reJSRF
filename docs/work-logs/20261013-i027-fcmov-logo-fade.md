# Issue #27 작업 로그 — FCMOVcc 번역 누락: 로고 페이드가 끝나지 않음

작업 지시: [20261013-i027](../work-orders/20261013-i027-fcmov-logo-fade.md) · 설계: [20261013-i027](../design/20261013-i027-fcmov-logo-fade.md)

## 요약

부팅이 "Presented by SEGA" 로고에 머물던 원인은 xboxrecomp lifter가 x87 조건부 이동 `FCMOVcc`를 번역하지 않은 것이었다. 게임의 float min/max 도우미가 항상 1과 0을 돌려, 페이드 색이 매 프레임 0으로 돌아갔다. lifter에 `FCMOVcc`를 넣자 로고 시퀀스(SEGA → Smilebit → ADX → Dolby Digital → 낙서 경고문)가 감마 램프 페이드와 함께 끝까지 진행한다. 그 뒤 스테이지를 읽다가 게스트 메모리 부족으로 멈춘다(다음 frontier).

수정은 같은 성격의 열린 업스트림 PR [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198)(`xadd` 플래그)에 커밋 `01064f3b`로 더했다. PR 제목과 본문은 두 수정을 함께 설명하도록 고쳤다. 포크의 `rejsrf` 브랜치(`500de2b1`)에 합쳤고 submodule이 그것을 가리킨다.

## 과정

1. 프로파일 빌드에서 주 스레드가 프레임마다 부르는 게임 함수를 뽑아 장면 디스패처 `0x0007BDD0`을 찾았다. 게스트 RAM 덤프에서 vtable로 객체를 찾아, 장면이 상태 8에 머문다는 것을 보았다.
2. 상태 8 → 작업 관리자의 작업 `0x1DDA`(로고) → 로고 단계 2 → 페이드 작업의 완료 플래그로 대기 사슬을 따라갔다. 15초와 45초 덤프가 같았다.
3. 페이드 update는 프레임마다 불렸다. 생성 코드에 임시 출력을 넣어 `fadd` 뒤 색 0.0083이 update 끝에서 0이 되는 것을 확인했다. 그 사이의 `sub_000A4CF0`이 쓰는 min/max 도우미가 `FCMOVcc`를 주석으로만 갖고 있었다.
4. lifter: `lift_basic_block`에서 같은 조건 코드의 jcc 조건으로 `if (cond) st(0) = st(i);`를 내보내고, 추적된 플래그가 없으면 `_flags`를 쓴다. 테스트 `test_lifter_fcmov.py`를 더했다(수정 전 4건 실패).
5. 사용자 요청에 따라 번역 누락 PR을 묶어, 새 PR 대신 #198에 커밋을 더했다.

## 검증

| 검증 | 결과 |
|---|---|
| xboxrecomp `pytest tools/recomp tools/disasm` | 513 passed, 38 skipped(`PYTHONUTF8=1`; 한국어 로캘에서 기존 테스트 1건이 인코딩 문제로 변경 전후 모두 실패) |
| 생성 코드 | `FPU: fcmov` 주석 0건(두 곳이 `TEST_Z`/`TEST_NZ` 조건 이동) |
| Win32 Release 빌드 | 성공, 경고 0 |
| probe·비교 도구 | kernel 42/0, dsound 48/0, d3d8 81/0, decomp 6/0, d3d8 check 14,018/0, pop 불일치 0 |
| 부팅(숨김 창, frame dump) | 60~2,400프레임에서 로고 다섯 개와 페이드, 약 40초 뒤 게스트 메모리 부족으로 종료(코드 3) |

xboxrecomp 테스트에는 scratchpad의 가상 환경(pytest, capstone)을 썼다. 사용자 Python에는 pytest가 없다.

## 남은 일

* 게스트 메모리 부족(`NtAllocateVirtualMemory out of guest memory for 672 bytes`)과 그 뒤 간접 호출 0. 다음 task.
* 정점 셰이더 프로그램 그리기(i023 §9)가 이제 실제로 필요하다(`DrawIndexedVertices with a vertex-shader program (006DE001)`).

---

# Issue #27 work log — FCMOVcc not lifted: the logo fade never completes

Work order: [20261013-i027](../work-orders/20261013-i027-fcmov-logo-fade.md) · Design: [20261013-i027](../design/20261013-i027-fcmov-logo-fade.md)

## Summary

The boot stayed on the "Presented by SEGA" logo because the xboxrecomp lifter did not lift the x87 conditional moves `FCMOVcc`. The game's float min/max helpers always returned 1 and 0, so the fade color went back to 0 every frame. With `FCMOVcc` lifted, the logo sequence (SEGA → Smilebit → ADX → Dolby Digital → the anti-graffiti notice) runs to the end with its gamma-ramp fades. It then stops while loading the stage, when guest memory runs out (the next frontier).

The fix was added, as commit `01064f3b`, to the open upstream PR of the same kind, [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198) (`xadd` flags), whose title and body now describe both fixes. It is merged into the fork's `rejsrf` branch (`500de2b1`), which the submodule points at.

## Process

1. Listing the game functions the main thread calls every frame in a profiling build found the scene dispatcher `0x0007BDD0`. Locating its object by vtable in a guest RAM dump showed the scene stuck in state 8.
2. The wait chain went from state 8 to the task manager's task `0x1DDA` (the logo), then the logo's step 2, then the fade task's done flag. Dumps at 15 s and 45 s were identical.
3. The fade update ran every frame. Temporary prints in the generated code showed the color at 0.0083 after the `fadd` and back at 0 at the end of the update. The min/max helpers used by `sub_000A4CF0` in between held `FCMOVcc` only as comments.
4. Lifter: `lift_basic_block` emits `if (cond) st(0) = st(i);` with the condition of the jcc that has the same condition code, falling back to `_flags` without tracked flags. `test_lifter_fcmov.py` added; its 4 cases fail without the change.
5. As the user asked, translation-gap fixes are bundled: the commit went onto #198 instead of a new PR.

## Verification

The table in the Korean section applies: xboxrecomp pytest 513 passed, 38 skipped (`PYTHONUTF8=1`; on a Korean locale one existing test fails before and after the change because of an encoding issue); no `FPU: fcmov` left in the generated code (both sites are now `TEST_Z`/`TEST_NZ` conditional moves); the Win32 Release build with zero warnings; probes and comparison tools 42/0, 48/0, 81/0, 6/0, 14,018/0, 0 pop mismatches; a hidden-window boot whose frame dumps from 60 to 2,400 show the five logos with fades, ending after about 40 s for lack of guest memory (exit code 3). The xboxrecomp tests ran in a scratchpad virtual environment (pytest, capstone); the user's Python has no pytest.

## Remaining

* Guest memory running out (`NtAllocateVirtualMemory out of guest memory for 672 bytes`), followed by an indirect call to 0. Next task.
* Drawing with vertex-shader programs (i023 §9) is now needed for real (`DrawIndexedVertices with a vertex-shader program (006DE001)`).
