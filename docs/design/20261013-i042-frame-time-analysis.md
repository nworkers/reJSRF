# 설계: 그리기 밖의 프레임 시간 분석 (issue #42)

선행: [i040 그리기 경로 비용](20261013-i040-draw-performance.md)

## 1. 요구사항

* i040 뒤 데모 프레임에서 그리기 경로와 present 밖의 시간("other")이 약 12.7 ms였다.
* 이 시간을 나눈다. HLE 호출(export별)과 게임의 lift된 코드를 구분하고, lift 코드에서는 어느 함수가 왜 시간을 쓰는지 본다.
* 싸게 고칠 수 있는 것은 고치고, 같은 프레임에서 전후를 비교한다. 나머지는 발견 사항으로 남긴다.
* **제약**:
  * 실행 파일에 PDB가 없다.
  * 사용자 계정에 관리자 권한이 없어 ETW 기반 샘플링(WPR/xperf)을 쓸 수 없다.

## 2. 설계

### 2.1 present 대기를 따로 잰다

* `REXBOX_D3D8_PROFILE`에 `wait` 단계를 더한다.
  * present 간격 대기(`WaitForPresentInterval`)를 잰다.
  * `D3DDevice_BlockUntilVerticalBlank`의 대기도 잰다.
  * `present`는 대기를 뺀 나머지(덤프, 화면 내기, 창 이벤트)만 센다.
* 단계 시간은 **present하는 스레드에서만** 센다. 다른 스레드(게임의 vblank 스레드가 `BlockUntilVerticalBlank`를 부른다)가 더하던 시간을 막는다. 첫 `EndFrame`을 부른 스레드를 thread-local 표시로 기억한다.
* `cpu` 열을 더한다. present 스레드가 구간 동안 쓴 CPU 시간이다(플랫폼 계약 `ThreadCpuTimeNanoseconds`, Win32 `GetThreadTimes`). 벽시계 시간보다 다른 프로세스의 영향을 덜 받는다.

### 2.2 프로세스 안의 샘플링 프로파일러

* **플랫폼 계약** `platform/sampler.h`: `StartThreadSampling()`과 `StopThreadSampling(path)`.
  * Win32 구현은 도우미 스레드다. 약 1 ms마다 대상 스레드를 `SuspendThread`/`GetThreadContext`로 멈추고 명령 포인터를 읽은 뒤 `ResumeThread`한다.
  * 멈춘 동안에는 메모리를 할당하지 않는다. 대상 스레드가 힙 잠금을 쥐고 있을 수 있기 때문이다.
  * 명령 포인터가 exe 밖(시스템·드라이버 DLL)이면 스택 앞부분(512 워드)을 복사해 둔다. 거기서 처음 exe 코드를 가리키는 값을 호출자 추정으로 함께 적는다.
  * 결과는 "모듈, 모듈 안 오프셋, 샘플 수, [exe 안 호출자 오프셋]" 히스토그램 파일이다.
  * `timeBeginPeriod(1)`로 1 ms 간격을 얻는다(`winmm`).
* **켜기**: `REXBOX_SAMPLE=<first>-<last>[:<file>]`. present `<first>`에서 그 present 스레드의 샘플링을 시작하고, present `<last>`에서 멈춰 파일(기본 `rexbox_samples.txt`)에 쓴다.
* **이름 풀기**: MSVC 링크에 `/MAP`을 더해 `rejsrf.map`을 만든다(재컴파일 없이 재링크만, 정적 함수와 라이브러리:오브젝트 포함).
  * `scripts/perf/resolve_samples.py`가 exe 오프셋을 map의 함수로 묶는다.
  * DLL 오프셋은 `SysWOW64`의 DLL export 표에서 가장 가까운 아래 export로 묶는다.
  * 출력: 라이브러리·모듈별 비율, 상위 함수, DLL 함수별 호출자.
  * 샘플은 같은 빌드의 map으로 풀어야 한다.
* **버린 대안**:
  * ETW(WPR/xperf): 관리자 권한이 필요하다.
  * PDB를 만드는 Release 빌드: 생성 코드 전체를 다시 컴파일해야 한다.
  * DbgHelp로 프로세스 안에서 이름 풀기: map으로 충분하고 의존이 늘어난다.

### 2.3 작은 변경

* **HLE 레지스트리 조회**: lift 코드의 간접 호출은 매번 `recomp_lookup_manual` → `HleRegistry::Lookup`(해시 맵)을 거친다. 0x01000000 아래 주소는 바인딩된 주소의 비트맵으로 먼저 거른다. 그 위(커널 thunk)는 지금처럼 맵을 찾는다.
* **draw마다의 보고 문자열**: `ReportOnce(Format(...))`는 이미 보고한 경우에도 문자열을 만든다. `ReportOncePer(site, value, text)`는 (호출 위치, 값)으로 거르고, 처음일 때만 문구를 만든다.
* **측정 뒤 버림**: 생성 코드의 `/GS-`(스택 쿠키 끄기). 기계어에는 쿠키가 보였지만 CPU 시간에 차이가 없었다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 데모 구간의 프로파일 | wait와 other가 나뉘고, wall ≈ cpu + wait |
| 샘플링 | 라이브러리별·함수별 구성, 잠자기 비율이 wait와 맞음 |
| 변형 실행 파일 번갈아 실행 | 작은 변경의 효과를 CPU 시간으로 비교 |
| Debug/Release, 회귀 도구 | 경고 0, 회귀 없음 |

---

# Design: analysing the frame time outside drawing (issue #42)

Prerequisite: [i040 draw-path cost](20261013-i040-draw-performance.md)

## 1. Requirements

* After i040, the time of a demo frame outside the draw path and present ("other") was about 12.7 ms.
* Split it: the HLE calls (by export) and the title's lifted code; for the lifted code, which functions take the time and why.
* Fix what is cheap and compare before and after on the same frames; record the rest as findings.
* **Constraints**:
  * The executable has no PDB.
  * The user account has no administrator rights, so ETW-based sampling (WPR/xperf) is not available.

## 2. Design

### 2.1 Measuring the present wait separately

* `REXBOX_D3D8_PROFILE` gains a `wait` phase.
  * It times the present-interval wait (`WaitForPresentInterval`).
  * It also times the wait in `D3DDevice_BlockUntilVerticalBlank`.
  * `present` keeps the rest (dump, presenting, window events).
* Phases count **on the presenting thread only**, so time other threads add stays out (the title's vblank thread calls `BlockUntilVerticalBlank`). The thread that first calls `EndFrame` is marked thread-locally.
* A `cpu` column gives the presenting thread's CPU time over the window (platform contract `ThreadCpuTimeNanoseconds`, Win32 `GetThreadTimes`). Other processes disturb it less than wall time.

### 2.2 An in-process sampling profiler

* **Platform contract** `platform/sampler.h`: `StartThreadSampling()` and `StopThreadSampling(path)`.
  * The Win32 implementation is a helper thread. About every millisecond it stops the target thread with `SuspendThread`/`GetThreadContext`, reads the instruction pointer and calls `ResumeThread`.
  * Nothing is allocated while the target is stopped, because it may hold the heap lock.
  * Outside the executable (system and driver DLLs), the top 512 stack words are copied. The first one that points into the executable's code is recorded as the guessed caller.
  * The result is a histogram file: module, offset in the module, samples, and the optional caller offset in the executable.
  * `timeBeginPeriod(1)` gives the 1 ms interval (`winmm`).
* **Switching it on**: `REXBOX_SAMPLE=<first>-<last>[:<file>]`. Sampling of the presenting thread starts at present `<first>` and stops at present `<last>`, writing the file (default `rexbox_samples.txt`).
* **Names**: MSVC links with `/MAP` and writes `rejsrf.map` (a relink, no recompilation; static functions and library:object included).
  * `scripts/perf/resolve_samples.py` maps executable offsets to map functions.
  * DLL offsets map to the nearest lower export in the DLL's export table under `SysWOW64`.
  * Output: shares by library or module, the top functions, and DLL functions by caller.
  * Samples must be resolved with the map of the same build.
* **Rejected**:
  * ETW (WPR/xperf): needs administrator rights.
  * A Release build with a PDB: recompiles all generated code.
  * In-process symbolization with DbgHelp: the map is enough, and DbgHelp adds a dependency.

### 2.3 Small changes

* **HLE registry lookup**: every indirect call of the lifted code goes through `recomp_lookup_manual` → `HleRegistry::Lookup` (a hash map). Addresses below 0x01000000 are first checked against a bitmap of bound addresses; addresses above (kernel thunks) still use the map.
* **Per-draw report strings**: `ReportOnce(Format(...))` builds its strings even when already reported. `ReportOncePer(site, value, text)` filters on (call site, value) and builds the message only the first time.
* **Measured and dropped**: `/GS-` (no stack cookies) for the generated code. The cookies show in the machine code, but CPU time did not change.

## 3. Verification

The table above applies: the demo profile splits wait from other, with wall ≈ cpu + wait; sampling gives the composition by library and function, with the sleep share matching wait; alternating variant executables compare the small changes by CPU time; Debug and Release build with zero warnings, and the tools show no regressions.
