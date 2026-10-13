# Issue #42 작업 로그 — 그리기 밖의 프레임 시간 분석

작업 지시: [20261013-i042](../work-orders/20261013-i042-frame-time-analysis.md) · 설계: [20261013-i042](../design/20261013-i042-frame-time-analysis.md) · 결론: [frame-time](../analysis/frame-time.md)

## 요약

데모 프레임의 "other" 약 12.7 ms를 나눴다.

* **i040의 present 11 ms**: 거의 전부 vblank 대기였다.
* **present 스레드의 CPU 작업**(483 그리기 구간, 약 16.5 ms):
  * lift된 게임 코드 38%
  * D3D8 HLE 26%
  * GL 드라이버 16%
  * recomp·런타임 연결부 6%
  * 나머지 14%
* lift 코드의 비용은 수백 개 함수에 퍼져 있다. 원인은 코드 생성 방식이다(레지스터를 TLS 메모리에 두기, 메모리 기준 재로드).

이를 위해 다음 도구를 만들었다.

* 프로세스 안의 샘플링 프로파일러(`REXBOX_SAMPLE`)와 이름 풀기 스크립트
* 프로파일의 `wait` 단계와 `cpu` 열

작은 변경 두 가지(HLE 레지스트리 비트맵, draw마다의 보고 문자열 제거)를 넣었다. `/GS-`도 시험했다. 셋 다 CPU 시간에 측정 가능한 차이를 내지 못했다. `/GS-`는 넣지 않았다.

## 과정

1. **도구 선택**: 관리자 권한이 없어 WPR/xperf를 쓸 수 없었다. 실행 파일에는 PDB가 없었다. 그래서 프로세스 안 샘플러와 링커 map으로 했다(설계 §2.2).
2. **첫 샘플**: ntdll이 44.6%였고 대부분 `ZwDelayExecution`이었다. 이름 풀기 스크립트에 DLL export 해석을 더해 확인했다.
   * map을 다시 빌드한 뒤 예전 샘플을 새 map으로 풀어 이름이 어긋난 일이 있었다. 이후로는 샘플마다 같은 빌드의 map을 복사해 두었다.
3. **wait 단계**: 처음에는 wait가 프레임보다 커지고 other가 음수가 됐다. 게임의 다른 스레드가 `BlockUntilVerticalBlank`를 부르기 때문이었다. present 스레드만 세게 하자 wall ≈ 그리기 경로 + wait + other가 맞았다.
   * 그리기 483개 구간: wait 9.4 ms, other 13.5 ms.
4. **숨은 잠자기 확인**: 샘플의 잠자기 비율이 wait보다 커 보였다. `KeDelayExecutionThread`/`KeStallExecutionProcessor`에 임시 계측을 넣어 봤지만(커밋하지 않음) 이 구간에서는 불리지 않았다. 샘플을 뜬 실행의 wait(약 14 ms)와 잠자기 비율이 맞았다. 숨은 잠자기는 없었다.
5. **호출자 추정**: 샘플러가 DLL 샘플의 스택에서 exe 코드 주소를 찾게 했다.
   * 그리기 호출의 드라이버 시간은 잡혔다.
   * 깊은 DLL 호출에서는 오래된 스택 값을 잡았다. 예를 들어 `ZwWriteFile`의 호출자로 행렬 함수가 나왔다. 그래서 참고용으로 둔다.
6. **lift 코드 기계어**: `dumpbin`으로 `sub_001BAB20`을 봤다(analysis §3). 스택 정렬, `/GS` 쿠키, TLS 레지스터, `g_xbox_mem_offset` 재로드가 보였다.
7. **작은 변경과 측정**: 레지스트리 비트맵, `ReportOncePer`, 생성 코드 `/GS-`를 넣고 재 보니 483 그리기 구간이 35 ms에서 25 ms로 줄어 보였다. 그러나 바꾸지 않은 그리기 단계도 같이 줄었다.
   * v0.0.16을 worktree에서 빌드해 번갈아 실행했다. 시각에 따라 23~53 ms로 흔들렸다. VS Code가 새 worktree를 색인하며 CPU를 썼고, 클럭도 변했다.
   * 그래서 `cpu` 열을 더했다. 변경 없음, 레지스트리·보고, 거기에 `/GS-`를 더한 세 변형을 번갈아 3회씩 실행했다. 평균 16.28, 16.04, 16.15 ms로 흔들림 안이었다(analysis §4).
   * `/GS-`는 되돌렸다. 레지스트리 비트맵과 보고 정리는 샘플에서 보이던 반복 작업을 없애므로 남겼다.
   * 한 번은 백업 파일을 `Copy-Item`으로 되돌려 예전 수정 시각이 따라왔다. 그 때문에 헤더를 바꾼 오브젝트가 다시 컴파일되지 않아 실행 파일이 시작하자마자 죽었다. 파일 시각을 갱신하고 다시 빌드했다.
8. **화면 확인**: 프레임 덤프가 v0.0.15와 픽셀 단위로 같지 않았다. 장면은 같고 카메라 위치만 몇 픽셀 달랐다.
   * 이번 변경은 보고 문자열만 건드렸으므로 렌더링 차이가 아니다.
   * 데모가 빨라져 진행량이 달라진 것으로 판단했다(**추정**: 게임은 경과 vblank 수만큼 진행하고 상한이 있다, analysis §5).

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| 데모 프로파일(483 그리기 구간) | wall 27.9 = cpu 16.5 + wait 10.4 + 약 1 ms |
| 샘플링 | 잠자기 36.5% ≈ wait 10.4 / 27.9 ms |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 116/116, 48/48 |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 14,018/0, 6/0 |
| 프레임 2,800~3,400 | 같은 장면, 카메라만 몇 픽셀 다름(속도에 따른 진행량 차이) |

## 남은 일

* xboxrecomp 코드 생성 개선(upstream): 메모리 기준을 함수마다 한 번 읽기, 기본 블록 안에서 레지스터를 지역 변수로 두기.
* D3D8: 쓰기 감시로 정점 범위·텍스처 변경을 감지한다(`memcmp` 1.3 ms).
* GL 호출 수 줄이기: 정점 상수를 uniform buffer로 보낸다.
* 프레임 덤프 비교는 같은 속도 조건에서 한다.

---

# Issue #42 work log — analysing the frame time outside drawing

Work order: [20261013-i042](../work-orders/20261013-i042-frame-time-analysis.md) · Design: [20261013-i042](../design/20261013-i042-frame-time-analysis.md) · Findings: [frame-time](../analysis/frame-time.md)

## Summary

The "other" of about 12.7 ms per demo frame is now split.

* **The 11 ms "present" of i040**: almost all of it was the vblank wait.
* **The presenting thread's CPU work** (483-draw window, about 16.5 ms):
  * lifted title code 38%
  * D3D8 HLE 26%
  * GL driver 16%
  * recomp and runtime glue 6%
  * the rest 14%
* The lifted code's cost spreads over hundreds of functions, and comes from how the code is generated (registers in TLS memory, reloads of the memory base).

Tools built for this:

* an in-process sampling profiler (`REXBOX_SAMPLE`) with a resolving script
* a `wait` phase and a `cpu` column in the profile

Two small changes went in (an HLE registry bitmap, no per-draw report strings), and `/GS-` was tried. None of the three made a measurable difference in CPU time, and `/GS-` was not adopted.

## Process

1. **Tools**: with no administrator rights, WPR/xperf were unavailable, and the executable has no PDB. That led to an in-process sampler and the linker map (design §2.2).
2. **First samples**: ntdll had 44.6%, mostly `ZwDelayExecution`, found after the resolving script learned DLL exports.
   * Once, old samples were resolved with a rebuilt map and the names came out wrong. From then on, each sample run kept a copy of its build's map.
3. **The wait phase**: at first, wait exceeded the frame and other went negative, because another title thread calls `BlockUntilVerticalBlank`. Counting only on the presenting thread made wall ≈ draw path + wait + other.
   * 483-draw window: wait 9.4 ms, other 13.5 ms.
4. **Hidden sleeps**: the sleep share in the samples looked larger than wait. Temporary counters in `KeDelayExecutionThread`/`KeStallExecutionProcessor` (not committed) showed neither was called in this window. The sleep share matched the wait of the sampled run (about 14 ms), so there were no hidden sleeps.
5. **Caller guesses**: the sampler now looks on the stack of a DLL sample for an address in the executable's code.
   * It found the driver time of the draw calls.
   * Behind deep DLL calls it picked stale stack values; for example, a matrix function came out as the caller of `ZwWriteFile`. So the guess is a hint only.
6. **Lifted machine code**: `dumpbin` of `sub_001BAB20` (analysis §3) shows stack alignment, a `/GS` cookie, TLS registers and reloads of `g_xbox_mem_offset`.
7. **Small changes and measurement**: with the registry bitmap, `ReportOncePer` and `/GS-` on the generated code, the 483-draw window seemed to drop from 35 to 25 ms. But the draw phases nobody had touched dropped too.
   * v0.0.16 was built in a worktree and the two were run alternately. The window ranged from 23 to 53 ms depending on the time: VS Code was indexing the new worktree, and clock speed varied.
   * So the profile gained a `cpu` column. Three variants (no change; registry and reports; those plus `/GS-`) ran alternately, three times each. Their means were 16.28, 16.04 and 16.15 ms, within the spread (analysis §4).
   * `/GS-` was reverted. The registry bitmap and report cleanup stay, because they remove repeated work the samples showed.
   * Once, backup files restored with `Copy-Item` kept their old modification times, so objects depending on a changed header were not recompiled, and the executable crashed at start. Touching the files and rebuilding fixed it.
8. **Picture check**: frame dumps were not identical to v0.0.15. The scene was the same, with the camera a few pixels off.
   * This change only touched report strings, so this is no rendering difference.
   * The faster demo advanced differently (**inferred**: the game advances by elapsed vblanks up to a cap; analysis §5).

## Verification

The table in the Korean section applies:

* Debug and Release builds with zero warnings on our targets.
* Demo profile of the 483-draw window: wall 27.9 = cpu 16.5 + wait 10.4 + about 1 ms.
* The sleep share in the samples (36.5%) matches wait.
* Kernel probe 56/56, D3D8 probe 116/116, DSOUND probe 48/48; d3d8 check 14,018/0, decomp check 6/0.
* Frames 2,800–3,400 show the same scenes, with the camera a few pixels off.

## Remaining

* xboxrecomp code generation (upstream): read the memory base once per function, and keep registers in locals within a basic block.
* D3D8: detect vertex-range and texture changes by write watching (the `memcmp` takes 1.3 ms).
* Fewer GL calls, starting with vertex constants in a uniform buffer.
* Compare frame dumps only at equal speed.
