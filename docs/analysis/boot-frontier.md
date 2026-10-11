# 부팅 frontier

재컴파일된 US 빌드를 진입점부터 실행했을 때 어디까지 가는지 기록한다. 작업마다 갱신하며, 지난 frontier는 아래 이력에 남긴다.

This records how far the recompiled US build gets when run from its entry point. It is updated by each task; earlier frontiers stay in the history below.

## 현재 / Current (issue #2, 2026-10-12)

| 항목 / Item | 값 / Value |
|---|---|
| 생성 코드 / Generated code | xboxrecomp v0.13.1, seed 없음 / no seeds, 함수 8,716개 / functions |
| 실행 결과 / Outcome | **미구현 커널 import에서 정지 / stopped at an unimplemented kernel import: 255 `PsCreateSystemThreadEx`** |
| 시간 / Time | 약 1.4초(프로세스 전체, Debug) / about 1.4 s (whole process, Debug) |
| 레지스터 / Registers | esp `03FFFF9C`, ebp `03FFFFC8`, 나머지 0 / others 0 |

* **확인됨**: 진입점 `0x00148023`에서 커널 호출 전까지 fault 없이 실행된다. 첫 커널 호출은 `PsCreateSystemThreadEx`이다.
* **추정**: XAPI의 시작 코드가 게임의 메인 스레드를 시스템 스레드로 만드는 지점이다(Xbox 타이틀의 일반적인 시작 순서). 다음 task는 이 호출을 구현해 새 스레드의 시작 함수로 들어가는 것이다.
* **확인됨 (Win32 한계)**: 32비트 호스트에서 `XBOX_PTR(addr) = addr + base`는 32비트로 wrap된다. 64 MiB 밖의 게스트 주소(커널 영역 `0x80000000` 이상, NV2A MMIO `0xFD000000` 등)는 fault 없이 호스트의 다른 메모리를 가리킬 수 있다. 커널 HLE task에서 이 범위의 접근을 처리할 방법(예: 해당 게스트 주소 범위를 별도 매핑하거나 접근 지점을 HLE로 막기)을 정해야 한다(**미확정**).

* **Confirmed**: execution runs from the entry point `0x00148023` to its first kernel call without a fault; that call is `PsCreateSystemThreadEx`.
* **Inferred**: this is XAPI's startup creating the game's main thread as a system thread (the usual Xbox title start sequence). The next task implements the call and enters the new thread's start routine.
* **Confirmed (Win32 limit)**: on a 32-bit host `XBOX_PTR(addr) = addr + base` wraps at 32 bits, so guest addresses beyond 64 MiB (the kernel range from `0x80000000`, NV2A MMIO at `0xFD000000`, ...) can reach other host memory without faulting. The kernel HLE task must decide how such accesses are handled (for example mapping those guest ranges separately, or stopping them at HLE call sites) (**unresolved**).

## 이력 / History

* issue #2: 첫 실행. `PsCreateSystemThreadEx`. / First run: `PsCreateSystemThreadEx`.
