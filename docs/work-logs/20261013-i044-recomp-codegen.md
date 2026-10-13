# Issue #44 작업 로그 — lift된 코드를 싸게 만드는 xboxrecomp 개선

작업 지시: [20261013-i044](../work-orders/20261013-i044-recomp-codegen.md) · 설계: [20261013-i044](../design/20261013-i044-recomp-codegen.md)

## 요약

xboxrecomp의 XMM 도우미를 SSE2 intrinsic으로 바꿨다. 데모 프레임에서 present 스레드의 CPU 시간이 약 12% 줄었다. 그리기 483개 구간 기준 15.2 → 14.2 ms, 다른 구간은 13~15% 줄었다.

메모리 기준을 함수마다 한 번 읽는 변경도 넣었지만, 그것만으로는 측정 가능한 차이가 없었다.

두 변경 모두 fork(`nworkers/xboxrecomp`)의 브랜치로 `rejsrf`에 합쳤다. upstream PR은 사용자 결정에 따라 내지 않았다.

화면은 v0.0.15와 픽셀 단위로 같다.

## 과정

1. **메모리 기준 먼저**: i042에서 본 재로드를 없애려고 `perf/mem-base-local`을 만들었다.
   * 번역된 함수가 진입 때 기준을 지역 상수로 복사한다.
   * 단위 테스트 2개를 더했다. `rejsrf`의 통과 테스트가 592개에서 594개가 됐다.
   * 재생성(lift 158초)과 빌드(약 17분) 뒤 기계어에서 재로드가 사라진 것을 확인했다.
   * v0.0.17과 번갈아 3회씩 실행하니 차이는 약 1%로 흔들림 안이었다.
2. **명령 단위 분석**: 효과가 작아 샘플을 명령 단위로 나눴다(임시 스크립트로 `dumpbin` 디스어셈블과 맞춤). 가장 뜨거운 명령은 스택 임시 `[ebp-20h]`를 읽는 `movaps`/`movups`였다.
   * `XMM_SCALAR` 하나가 임시를 0으로 채우고, 8바이트씩 쓴 뒤 16바이트로 읽는다. forwarding이 실패하는 패턴이다.
3. **XMM intrinsic**(`perf/xmm-intrinsics`): 자주 쓰는 도우미를 SSE2 intrinsic으로 바꿨다(설계 §3.1).
   * 같은 C 드라이버를 두 모드로 빌드해 비트 단위로 비교하는 `test_xmm_intrinsics`를 더했다.
   * 이 기계에는 clang/gcc가 없어 vcvars32 환경의 cl로 돌렸다.
   * `ANDN`을 `AND`로 바꾼 버그를 일부러 넣어 테스트가 잡는 것도 확인했다.
   * 재생성 뒤 같은 함수의 기계어에서 정렬 prologue, 쿠키, 임시 왕복이 모두 사라졌다(`movss xmm1,[...]` 바로 사용).
4. **측정**: v0.0.17, 메모리 기준, 메모리 기준 + XMM을 번갈아 3회씩 실행했다(설계 §4). XMM에서 네 구간 합이 -12%였다.
5. **화면**: 프레임 2,800 / 3,000 / 3,200 / 3,400의 덤프가 v0.0.15와 다른 픽셀 0개였다.
6. **다음 후보의 상한**: XMM 빌드를 다시 샘플링했다.
   * TLS 레지스터 칸 접근이 lift 코드의 21~25%다. 그중 호출 없는 leaf 함수 몫은 7.5%다.
   * 지역화의 상한은 leaf만이면 전체 CPU 약 2%, 전부면 약 7%다. 호출 지점 약 3만 7천 곳과 반환마다 동기화가 필요하다.
   * 간접 호출 조회(`recomp_lookup`)는 lift 코드의 약 2%다.
   * 이 둘은 하지 않았다.
7. 실행 파일을 다른 폴더에서 돌리다 xboxrecomp 작업 트리에 게임 HDD 폴더(`hdd/`)가 생겨 지웠다.

## 검증

| 검증 | 결과 |
|---|---|
| xboxrecomp 단위 테스트(`rejsrf`) | 594 통과, 41 건너뜀 |
| `test_xmm_intrinsics`(cl, vcvars32) | 통과; 심은 버그는 실패로 잡힘 |
| 재생성, Win32 Debug / Release 빌드 | 성공, 경고 0 |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe`(Debug, Release) | 56/56, 116/116, 48/48 |
| `rejsrf_d3d8_check` / `rejsrf_decomp_check`(Debug, Release) | 14,018/0, 6/0 |
| `check_hle_pops.py` | 불일치 0 |
| CPU 시간(설계 §4) | XMM으로 -12% |
| 프레임 2,800~3,400 | v0.0.15와 다른 픽셀 0개 |

## 남은 일

* 레지스터 지역화(상한 전체 CPU 약 7%)와 간접 호출 지점별 캐시(약 1%)는 효과 대비 위험을 보고 결정한다.
* fork의 `perf/mem-base-local`, `perf/xmm-intrinsics`는 사용자가 말할 때 다른 수정과 함께 upstream에 낸다.

---

# Issue #44 work log — cheaper lifted code through xboxrecomp

Work order: [20261013-i044](../work-orders/20261013-i044-recomp-codegen.md) · Design: [20261013-i044](../design/20261013-i044-recomp-codegen.md)

## Summary

xboxrecomp's XMM helpers are now SSE2 intrinsics. The presenting thread's CPU time per demo frame fell by about 12%: from 15.2 to 14.2 ms in the 483-draw window, and by 13–15% in the other windows.

Reading the memory base once per function went in as well, but alone it made no measurable difference.

Both changes are branches on the fork (`nworkers/xboxrecomp`) merged into `rejsrf`. As the user decided, no upstream PR was opened.

The picture is identical to v0.0.15 pixel for pixel.

## Process

1. **Memory base first**: `perf/mem-base-local` removes the reloads i042 found.
   * Translated functions copy the base into a local constant on entry.
   * Two unit tests were added; `rejsrf` went from 592 to 594 passing tests.
   * After regenerating (158 s of lifting) and building (about 17 minutes), the reloads were gone from the machine code.
   * Three alternating runs against v0.0.17 differed by about 1%, within the noise.
2. **Per-instruction analysis**: with so little gained, the samples were split per instruction (a temporary script matching them to a `dumpbin` disassembly). The hottest were `movaps`/`movups` reading the stack temporary `[ebp-20h]`.
   * A single `XMM_SCALAR` zeroes a temporary, writes it 8 bytes at a time and reads it back 16 bytes wide, a pattern that fails store forwarding.
3. **XMM intrinsics** (`perf/xmm-intrinsics`): the common helpers became SSE2 intrinsics (design §3.1).
   * `test_xmm_intrinsics` builds one C driver both ways and compares the results bit for bit.
   * With no clang or gcc on this machine, it ran with cl in a vcvars32 environment.
   * A planted bug (`ANDN` as `AND`) made the test fail, as it should.
   * After regenerating, the same function lost its alignment prologue, cookie and temporaries: `movss xmm1,[...]` is used directly.
4. **Measurement**: v0.0.17, memory base, and memory base + XMM ran alternately three times each (design §4). With XMM, the sum of four windows was 12% lower.
5. **Picture**: dumps of frames 2,800, 3,000, 3,200 and 3,400 differed from v0.0.15 in 0 pixels.
6. **Bounds of the next candidates**: the XMM build was sampled again.
   * TLS register-slot accesses are 21–25% of lifted code; the leaf functions (no calls) hold 7.5%.
   * Localizing registers would gain at most about 2% of total CPU (leaf functions only) or 7% (everything), with synchronization at about 37,000 call sites and every return.
   * The indirect-call lookup (`recomp_lookup`) is about 2% of lifted code.
   * Neither was done.
7. A run from another folder created the game's HDD folder (`hdd/`) inside the xboxrecomp work tree; it was deleted.

## Verification

The table in the Korean section applies:

* xboxrecomp unit tests on `rejsrf`: 594 passed, 41 skipped.
* `test_xmm_intrinsics` passes with cl and fails on the planted bug.
* Regeneration and Win32 Debug/Release builds succeed with zero warnings.
* In both Debug and Release: kernel probe 56/56, D3D8 probe 116/116, DSOUND probe 48/48, d3d8 check 14,018/0, decomp check 6/0.
* 0 pop mismatches.
* CPU time -12% with XMM.
* Frames 2,800–3,400 identical to v0.0.15.

## Remaining

* Register localization (at most about 7% of CPU) and per-site indirect-call caches (about 1%) wait for a decision weighing gain against risk.
* The fork's `perf/mem-base-local` and `perf/xmm-intrinsics` go upstream together with the other fixes when the user says so.
