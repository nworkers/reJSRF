# Issue #12 작업 로그 — D3D8 HLE 두 번째 묶음: 리소스 모델, 서피스, 상태, 첫 프레임을 향해 (진행 중)

작업 지시: [20261012-i012](../work-orders/20261012-i012-d3d8-surfaces-first-frame.md) · 설계: [20261012-i012](../design/20261012-i012-d3d8-surfaces-first-frame.md)

## 요약

D3D8 HLE에 Xbox 리소스 헤더 모델(`d3d8_resource`), 디바이스 서피스와 `GetBackBuffer`/`GetDepthStencilSurface`/`Clear`/`Swap`, 상태 setter 저장(`d3d8_state`), 리소스 생성·Lock·`Register`(`d3d8_create`), 셰이더 핸들(`d3d8_shader`), 그리기 스텁(`d3d8_draw`)을 더해 export가 8개에서 110개가 됐다. `rejsrf_d3d8_check`는 헤더 기반 케이스까지 13,557개를 원본과 대조해 차이 0이다. 부팅은 D3D8 초기화와 게임의 객체 생성을 지나 **커널 import 65 `IoCreateDevice`** 에서 멈춘다. 첫 `Swap`은 아직이다.

가장 큰 발견은 재컴파일 파이프라인 쪽이었다. 게임이 D3D 초기화 뒤 CRT 힙 안에서 무한 루프했는데, 원인은 xboxrecomp lifter가 `lock xadd`를 "플래그 미정의"로 취급해 바로 뒤의 `jne`(COM `Release`의 "0이면 파괴")가 항상 거짓이 되는 것이었다. 모든 `Release`가 객체를 파괴해 같은 객체가 두 번 해제되고 XAPI NT 힙의 자유 리스트가 순환했다. lifter를 고쳐 포크(`nworkers/xboxrecomp`, `fix/xadd-flags`)에 올리고 업스트림 PR [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198)로 보냈다(AGENTS.md의 업스트림 기여 규칙). submodule은 PR이 머지될 때까지 포크를 가리킨다.

## 과정

1. 리소스 모델: lift 본문에서 `D3DResource_AddRef/Release/GetType`, `Get2DSurfaceDesc`, XGRAPHC 인코더(`0x001BBAFB`)를 읽어 `Common`/`Format`/`Size` 비트와 `D3DSURFACE_DESC`(28바이트, `{Format, Type, Usage, Size, MultiSampleType, Width, Height}`)를 확정했다. 디바이스 서피스 헤더가 lift된 `XGSetSurfaceHeader`의 결과와 일치함을 도구로 확인했다.
2. 비교 도구가 잡은 차이: `CheckDeviceMultiSampleType`의 첫 표 조회는 `Windowed` 인자를 쓴다, `GetDesc`의 eax는 레벨 폭, 선형 헤더의 DMA 니블은 2, `Register`는 push buffer를 제외하고 마스크, `GetTexture2`는 인자 2개. 모두 측정값에 맞췼다. `scripts/recomp/check_hle_pops.py`로 export의 pop 개수와 원본 `ret n`을 대조한다.
3. 부팅이 `D3DDevice_SetRenderState_ZEnable` → `LightEnable`에서 멈추자 setter·생성·Lock·셰이더·그리기 스텁을 한 번에 더했다. 그 뒤 부팅이 멈추지 않고 CPU만 쓰는 상태가 되어 진단 도구를 만들었다: `REXBOX_HLE_TRACE`(XDK·커널 호출과 반환값), `REXBOX_WATCHDOG`(생성 코드를 `REJSRF_RECOMP_CALL_PROFILE`로 빌드한 직접 호출 프로파일 덤프, `REXBOX_WATCHDOG_DUMP`로 게스트 RAM 저장), `REXBOX_HEAP_CHECK`(NT 힙 자유 리스트 검사), `REXBOX_TRACE_MEMORY`, `RtlEnterCriticalSection` 대기 보고.
4. 메모리 덤프에서 힙 블록 체인은 온전하고 `FreeLists` 체인만 순환함을 보고, 힙 검사로 손상 시점이 세 번째 `free(0x36D490)`임을 특정했다. 호출 프로파일로 `Release`(`0x0015FD40`)가 `lock xadd` 뒤 `if (_flags)`로 lift된 것을 확인했고, lifter 소스에서 `"lock xadd"`가 `_FLAGS_UNDEFINED`에 있음을 찾았다.
5. lifter 수정: 원자 `xadd` 번역이 합을 `_fa/_fas`에 게시, `xadd`를 결과 스냅샷 setter로 등록, 0 기준 부호 조건 추가, `lock` 접두 mnemonic을 추적. 테스트 `tools/recomp/test_xadd_flags.py` 추가; xboxrecomp 테스트 509개 통과. 재생성(485초) 뒤 힙 멈춤이 사라졌다.
6. 부수 발견: 디스어셈블러가 `ret` 직후의 조건부 점프 대상을 별도 함수(`gap_prologue`)로 쪼개는 경우가 135개 있다. lifter가 조건부 tail call로 옮기므로 동작은 맞지만 기록해 둔다. XPP의 `0x001BD274`는 함수로 탐지되지 않아 간접 호출이 건너뛰어진다(seed 추가 대상).

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug 빌드 | 성공, 우리 타깃 경고 0 |
| Win32 Release 빌드 | 성공, 우리 타깃 경고 0 |
| `rexbox_kernel_probe` / `rejsrf_decomp_check` | 27/27, 6/6 |
| `rejsrf_d3d8_check` | 13,557 케이스, 차이 0 (Debug·Release) |
| `check_hle_pops.py` | 불일치 0 (수동 확인 대상 5개: lift 본문에 `ret`가 없는 함수) |
| xboxrecomp `pytest tools/recomp tools/disasm` | 509 passed, 38 skipped |
| 부팅 | 약 5초 뒤 `stopped at unimplemented kernel import 65 IoCreateDevice` |

## 남은 일

* `IoCreateDevice` 등 XAPI 장치 초기화가 요구하는 커널 서비스(커널 HLE 후속; 별도 issue), `0x001BD274` seed, 그리고 첫 `D3DDevice_Swap`까지.
* xboxrecomp#198이 머지되면 submodule을 업스트림으로 되돌린다.
* 그리기(정점 버퍼·셰이더·렌더 상태 → GL)는 세 번째 묶음.

---

# Issue #12 work log — D3D8 HLE, second batch: resource model, surfaces, state, toward the first frame (in progress)

Work order: [20261012-i012](../work-orders/20261012-i012-d3d8-surfaces-first-frame.md) · Design: [20261012-i012](../design/20261012-i012-d3d8-surfaces-first-frame.md)

## Summary

The D3D8 HLE gained the Xbox resource-header model (`d3d8_resource`), the device surfaces with `GetBackBuffer`/`GetDepthStencilSurface`/`Clear`/`Swap`, stored state setters (`d3d8_state`), resource creation, locking and `Register` (`d3d8_create`), shader handles (`d3d8_shader`) and drawing stubs (`d3d8_draw`): 110 exports instead of 8. `rejsrf_d3d8_check` now compares 13,557 cases, header-based ones included, with 0 differences. Boot passes D3D8 initialization and the game's object setup and stops at **kernel import 65 `IoCreateDevice`**; the first `Swap` is not reached yet.

The main finding was in the recompilation pipeline. The game spun inside the CRT heap after D3D initialization because the xboxrecomp lifter treated `lock xadd` as leaving undefined flags, so the `jne` right after it (COM `Release`'s "destroy at zero") was always false; every `Release` destroyed its object, the same object was freed twice, and XAPI's NT heap free list became circular. The lifter fix is on the fork `nworkers/xboxrecomp` (`fix/xadd-flags`) and was submitted upstream as [sp00nznet/xboxrecomp#198](https://github.com/sp00nznet/xboxrecomp/pull/198) (AGENTS.md's upstream-contribution rule); the submodule points at the fork until it is merged.

## Process

1. Resource model: the lifted `D3DResource_AddRef/Release/GetType`, `Get2DSurfaceDesc` and the XGRAPHC encoder (`0x001BBAFB`) fixed the `Common`/`Format`/`Size` bits and the 28-byte `D3DSURFACE_DESC` (`{Format, Type, Usage, Size, MultiSampleType, Width, Height}`); the tool confirms the device's surface headers equal the lifted `XGSetSurfaceHeader`'s.
2. Differences the tool caught and the HLE now matches: `CheckDeviceMultiSampleType` indexes its first table lookup with the `Windowed` argument, `GetDesc` leaves the level width in eax, linear headers carry DMA nibble 2, `Register` masks every type but push buffers, `GetTexture2` takes two arguments. `scripts/recomp/check_hle_pops.py` compares export pop counts with the originals' `ret n`.
3. When boot stopped at `D3DDevice_SetRenderState_ZEnable` and then `LightEnable`, the setters, creation, locking, shader and drawing stubs were added together. Boot then ran without stopping, which led to the diagnostics: `REXBOX_HLE_TRACE` (XDK and kernel calls with results), `REXBOX_WATCHDOG` (direct-call profile from generated code built with `REJSRF_RECOMP_CALL_PROFILE`, plus `REXBOX_WATCHDOG_DUMP` for the guest RAM), `REXBOX_HEAP_CHECK` (NT heap free-list validation), `REXBOX_TRACE_MEMORY`, and the `RtlEnterCriticalSection` wait report.
4. The memory dump showed an intact block chain but a circular `FreeLists` chain; the heap check placed the corruption at the third `free(0x36D490)`; the call profile showed `Release` (`0x0015FD40`) lifted as `if (_flags)` after `lock xadd`, and the lifter source had `"lock xadd"` in `_FLAGS_UNDEFINED`.
5. Lifter fix: the atomic `xadd` lowering publishes the sum into `_fa/_fas`, `xadd` joins the result-snapshot setters, zero-relative signed conditions are answered, and the `lock` spellings are tracked. `tools/recomp/test_xadd_flags.py` added; 509 xboxrecomp tests pass. After regeneration (485 s) the heap hang is gone.
6. Side findings: the disassembler splits 135 functions at a conditional-jump target that follows a `ret` (`gap_prologue`); the lifter emits conditional tail calls, so behavior is preserved, but it is recorded. `0x001BD274` in XPP is not detected as a function, so an indirect call to it is skipped (seed needed).

## Verification

The table in the Korean section applies: Debug and Release builds with zero warnings on our targets; kernel probe 27/27, decomp check 6/6; `rejsrf_d3d8_check` 13,557 cases with 0 differences in both configurations; `check_hle_pops.py` 0 mismatches (5 functions whose lifted body has no `ret` are checked by hand); xboxrecomp's pytest 509 passed, 38 skipped; boot stops after about 5 s at `stopped at unimplemented kernel import 65 IoCreateDevice`.

## Remaining

* The kernel services XAPI's device initialization needs (`IoCreateDevice` and what follows; kernel HLE follow-up, separate issue), the `0x001BD274` seed, and reaching the first `D3DDevice_Swap`.
* Return the submodule to upstream once xboxrecomp#198 is merged.
* Drawing (vertex buffers, shaders, render state → GL) is the third batch.
