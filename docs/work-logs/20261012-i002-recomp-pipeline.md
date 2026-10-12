# Issue #2 작업 로그 — 재컴파일 파이프라인 연결

작업 지시: [20261012-i002](../work-orders/20261012-i002-recomp-pipeline.md) · 설계: [20261012-i002](../design/20261012-i002-recomp-pipeline.md)

## 요약

US XBE를 xboxrecomp 파이프라인으로 C(함수 8,716개, 약 102만 줄)로 옮기고, Win32 x86에서 `rejsrf`에 링크했다. 생성 코드의 ABI는 새 타깃 `rexbox_recomp`가 맞춘다. 앱은 진입점부터 실행해 첫 미구현 커널 import인 **255 `PsCreateSystemThreadEx`** 에서 멈추고 이를 보고한다. i001에서 미확정이던 직접 호출의 우선순위는 생성 단계에서 해결했다. 디컴파일 함수가 `sub_X`를 정의하면 lifter가 그 함수를 `sub_X_gen`으로 내보내고, 직접 호출 21개 파일과 디스패치 테이블이 모두 디컴파일 버전으로 링크된다. 첫 디컴파일 함수 `sub_000128C0`은 lift 버전과 6개 케이스에서 레지스터와 메모리 전체가 일치했다.

## 과정

1. 파이프라인 기준선을 측정했다(설계 §2). 모든 단계가 출력 경로를 받아 submodule을 더럽히지 않는다. Windows Python이 `/e/...` 경로를 읽지 못해 `E:/...` 형식으로 넘겼다(스크립트는 `pathlib`로 해결).
2. 생성 코드만으로 시험 링크해 런타임이 제공해야 할 심볼 45개를 뽑았다. 첫 시도는 main이 없는 심볼(`sub_00148023`)만 참조해서 라이브러리를 끌어오지 않았다. 진입점의 실제 이름은 `xbe_entry_point`다.
3. lifter의 `--exclude-manual`과 `_gen` 래핑을 읽고, 직접 호출의 우선순위를 생성 단계에서 해결하기로 했다(실행 시간 비용 0).
4. `rexbox_recomp`: 전역 정의는 고정 템플릿 헤더를 include한 C 파일로 둬서 선언과 타입을 일치시켰다. MMX 타입이 `RECOMP_GENERATED_CODE` 아래에만 있어, 그 매크로를 켜고 레지스터 이름 매크로(`eax` 등)는 바로 해제했다.
5. 플랫폼 계약 `RunGuarded`/`RaiseStop`(Win32 SEH). `<excpt.h>`의 `exception_code` 매크로와 필드 이름이 충돌해 `fault_code`로 바꿨다.
6. 커널 thunk: 슬롯에 `0xFE000000 + ordinal × 4`를 쓰고 registry를 그 주소로 바인딩한다. 상수는 커널 HLE와 recomp 어댑터가 공유하므로 `rexbox/runtime/kernel_thunks.h`로 올렸다.
7. `rejsrf_game` ↔ `rejsrf_recomp` 정적 라이브러리 순환을 CMake에 양방향으로 선언했다.
8. `/W4`를 전역에서 우리 타깃 전용 함수로 옮기고, 생성 코드는 `/w /bigobj /MP`로 빌드한다.

## 검증

| 검증 | 결과 |
|---|---|
| `regenerate.py` 처음부터 실행 | 약 2분(디스어셈블 32초, lift 61초), `1 wrapped as sub_X_gen`, submodule 깨끗함 |
| Win32 x86 Debug 빌드(생성 코드 포함) | 성공, 우리 코드 경고 0 |
| Win32 x86 Release 빌드(생성 코드 포함) | 성공 1.7분, 경고 0 |
| `rejsrf default.xbe` (Debug, Release) | `run: stopped at unimplemented kernel import 255 PsCreateSystemThreadEx`, 약 1.4초 |
| `rejsrf default.xbe` (일본판) | JP 빌드는 lift 대상이 아니라고 알리고 실행하지 않음 |
| `rejsrf_decomp_check` (Debug, Release) | 6 cases, 0 differences, 종료 코드 0 |
| `dumpbin /symbols` | `rejsrf_game.lib`에 `_sub_000128C0` 정의, `rejsrf_recomp.lib`에는 `_sub_000128C0` 참조(UNDEF) 21개와 `_sub_000128C0_gen` 정의 1개 |
| `gen/` 없이 configure·빌드 | 성공, 앱은 "no recompiled code"로 로드 후 정지(CI 경로) |

CI는 submodule checkout을 켰으나 GitHub에서는 아직 실행되지 않았다.

## 디컴파일 함수 검증 기록

* `sub_000128C0` (thiscall, `ret 4`): `this->table_098[index]`, 음수면 0. 비교: `rejsrf_decomp_check`에서 index 0, 1, 7, 255, -1, INT32_MIN. 초기 상태가 같을 때 8개 범용 레지스터와 16 MiB 게스트 메모리 전체가 같다.

## 남은 일

* 다음 task: 커널 HLE 최소 집합. `PsCreateSystemThreadEx`부터 시작해 새 스레드의 시작 함수로 들어가고, 데이터 export(`KeTickCount` 등)를 게스트 메모리에 둔다(설계 §5).
* Win32 based 모델에서 64 MiB 밖 게스트 주소가 wrap되는 문제(`docs/analysis/boot-frontier.md`).
* jsrf-recomp의 JSRF seed(스레드 시작점, 간접 호출 대상 등) 도입은 실제로 놓친 함수가 확인될 때 한다.

---

# Issue #2 work log — connecting the recompilation pipeline

Work order: [20261012-i002](../work-orders/20261012-i002-recomp-pipeline.md) · Design: [20261012-i002](../design/20261012-i002-recomp-pipeline.md)

## Summary

The US XBE was lifted to C by the xboxrecomp pipeline (8,716 functions, about 1.02 M lines) and linked into `rejsrf` on Win32 x86, with the new `rexbox_recomp` target matching the generated code's ABI. The app runs from the entry point, stops at the first unimplemented kernel import, **255 `PsCreateSystemThreadEx`**, and reports it. The direct-call priority left unresolved in i001 is settled at generation time: when a decompiled function defines `sub_X`, the lifter emits that function as `sub_X_gen`, and the direct calls in 21 files and the dispatch table all link to the decompiled version. The first decompiled function, `sub_000128C0`, matched its lifted version on all registers and all memory in 6 cases.

## Process

1. Measured the pipeline baseline (design §2). Every step takes an output path, so the submodule stays clean. Windows Python cannot read `/e/...` paths, so `E:/...` was passed (the script uses `pathlib`).
2. A trial link of the generated code alone listed the 45 symbols the runtime must supply. The first attempt referenced only a symbol that does not exist (`sub_00148023`) and pulled nothing from the library; the entry point is really `xbe_entry_point`.
3. Read the lifter's `--exclude-manual` and `_gen` wrapping and chose to resolve the direct-call priority at generation time (zero run-time cost).
4. `rexbox_recomp`: the globals are defined in a C file that includes the pinned template header, so definitions and declarations match exactly. The MMX type exists only under `RECOMP_GENERATED_CODE`, so that macro is set and the register-name macros (`eax`, ...) are undefined right after.
5. Platform contract `RunGuarded`/`RaiseStop` (Win32 SEH). `<excpt.h>`'s `exception_code` macro clashed with a field name, renamed `fault_code`.
6. Kernel thunks: slots get `0xFE000000 + ordinal × 4` and the registry binds that address. The constant is shared by the kernel HLE and the recomp adapter, so it moved up to `rexbox/runtime/kernel_thunks.h`.
7. Declared the `rejsrf_game` ↔ `rejsrf_recomp` static-library cycle to CMake in both directions.
8. Moved `/W4` from global to a function for our targets; generated code builds with `/w /bigobj /MP`.

## Verification

The table in the Korean section applies: `regenerate.py` from scratch in about two minutes with one function wrapped as `sub_X_gen` and a clean submodule; Debug and Release builds with generated code and zero warnings in our code (Release 1.7 min); the app stops at kernel import 255 `PsCreateSystemThreadEx` in about 1.4 s; the JP build is reported as not the lifted variant and not run; `rejsrf_decomp_check` reports 6 cases and 0 differences with exit code 0 in both configurations; `dumpbin /symbols` shows `_sub_000128C0` defined in `rejsrf_game.lib` while `rejsrf_recomp.lib` holds 21 UNDEF references to it and the single `_sub_000128C0_gen` definition; configuring and building without `gen/` succeeds and the app stops after load (the CI path). CI now checks out submodules but has not yet run on GitHub.

## Decompiled function verification record

* `sub_000128C0` (thiscall, `ret 4`): `this->table_098[index]`, 0 for a negative index. Compared in `rejsrf_decomp_check` for index 0, 1, 7, 255, -1 and INT32_MIN: from the same initial state, all eight general registers and the whole 16 MiB of guest memory are identical.

## Remaining

* Next task: a minimal kernel HLE, starting with `PsCreateSystemThreadEx` to enter the new thread's start routine, and placing the data exports (`KeTickCount`, ...) in guest memory (design §5).
* The Win32 based model wraps guest addresses beyond 64 MiB (`docs/analysis/boot-frontier.md`).
* jsrf-recomp's JSRF seeds (thread starts, indirect call targets, ...) are adopted once a function is shown to be missed.
