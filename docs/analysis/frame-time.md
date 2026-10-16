# 프레임 시간: 오프닝 3D 데모의 한 프레임이 어디에 쓰이는가

오프닝 3D 데모(로고 뒤 어트랙트 데모)의 프레임 시간을 측정한 결론을 모은다. 측정 방법과 도구는 [설계 i042](../design/20261013-i042-frame-time-analysis.md), 과정은 [작업 로그 i042](../work-logs/20261013-i042-frame-time-analysis.md)에 있다.

환경: Win32 x86 Release, Intel Core i5-7200U(2코어 4스레드), Intel HD Graphics 620(OpenGL 3.3, 드라이버 26.20.100.7325), 숨김 창, 오디오 없음.

## 1. 한 프레임의 구성

* **확인됨**(`REXBOX_D3D8_PROFILE`, v0.0.16 이후 코드): 2,701~3,000프레임(그리기 483개)에서 한 프레임은 27.9 ms였다. 내역은 다음과 같다.

| 부분 | ms/프레임 | 내용 |
|---|---|---|
| present 스레드의 CPU 시간 | 16.5 | 게임 코드, HLE, GL 드라이버 |
| vblank 대기 | 10.4 | 늦은 프레임이 다음 vblank까지 기다림(`WaitForPresentInterval`) |
| 나머지 | 약 1 | 다른 스레드를 기다리거나 스케줄되지 않은 시간 |

* 그리기 경로(인덱스부터 GL 그리기까지)는 그중 약 8 ms다. 그리기도 대기도 아닌 HLE와 게임 코드("other")는 약 9.5 ms다.
* 프레임은 vblank(16.7 ms) 단위로 나간다. 그래서 CPU 작업이 16.7 ms를 조금만 넘어도 프레임은 33.3 ms가 된다. 60프레임을 내려면 present 스레드의 작업이 한 프레임에 16.7 ms 아래로 꾸준히 내려가야 한다.

## 2. CPU 작업의 구성 (샘플링)

* **확인됨**(`REXBOX_SAMPLE`, 같은 구간): 잠자기(`ZwDelayExecution`, vblank 대기)를 뺀 샘플 2,647개를 묶으면 다음과 같다.

| 묶음 | 비율 | ms/프레임(추정) |
|---|---|---|
| lift된 게임 코드(`rejsrf_recomp`) | 38% | 6.3 |
| D3D8 HLE | 26% | 4.2 |
| GL 드라이버(`ig9icd32`, `igc32`, `opengl32`) | 16% | 2.6 |
| recomp 브리지와 런타임(HLE 진입, 레지스터 접근, 레지스트리 조회) | 6% | 1.0 |
| 나머지(ntdll의 대기 외 부분, CRT, XInput HLE) | 14% | 2.3 |

* **lift된 코드**: 샘플이 216개 이상의 함수에 퍼져 있다. 상위 30개를 합쳐도 lift 코드의 약 40%다.
  * 가장 무거운 묶음은 0x1BA9C0~0x1BB330의 SSE 벡터·행렬 변환 함수들이다(`movss`/`shufps`/`mulps`).
  * 그 다음은 게임의 행렬·렌더 함수다(`sub_000B3E10`, `sub_000163D0`, `sub_0014DB70` 등).
* **D3D8 HLE**: 정점 범위 비교(`memcmp`, i040의 캐시)가 가장 크다. 작업의 약 8%, 1.3 ms다. 그 뒤는 정점 상수 비교(`SetArray4`), 상태 읽기(`TextureStateValue`), 텍스처 표본 해시(`SampleHash`)다.
* **GL 드라이버**: 그리기 호출 자체와 `ZwSetEvent`(드라이버가 작업 스레드를 깨우는 것으로 보임, **추정**)가 대부분이다.
* **호출자 추정의 한계**: 샘플러는 DLL 안의 샘플에 대해 스택에서 처음 만나는 exe 코드 주소를 호출자로 적는다. DLL 호출이 깊으면(예: `sleep_until` → msvcp → kernelbase → ntdll) 오래된 스택 값을 잡을 수 있다. 참고로만 쓴다.

## 3. lift된 코드의 기계어

* **확인됨**(`dumpbin`, `sub_001BAB20`): lift된 함수 하나에 다음이 붙는다.
  * 함수마다 16바이트 스택 정렬 prologue와 스택 쿠키(`/GS`)가 있다.
  * 게스트 레지스터는 TLS 구조체의 메모리 칸이다. 함수마다 TLS 기준을 한 번 구하고, 레지스터를 읽고 쓸 때마다 메모리에 접근한다.
  * `g_xbox_mem_offset`(게스트 메모리 기준)을 게스트 메모리에 쓸 때마다 다시 읽는다. volatile 쓰기가 그 전역과 겹칠 수 있다고 보기 때문이다.
  * `movss` 하나가 0으로 채운 임시 XMM을 거쳐 메모리로 돌아간다.
* **확인됨**(측정): 생성 코드에서 `/GS`를 끄는 것만으로는 차이가 없었다(§4).
* **추정**: 남은 비용의 큰 몫은 xboxrecomp의 코드 생성 방식이다(레지스터를 메모리에 두기, 메모리 기준 재로드).
* **확인됨**(i044): 가장 뜨거운 명령은 XMM 도우미가 만든 스택 임시를 부분 저장 뒤 16바이트로 읽는 `movaps`/`movups`였다(store-to-load forwarding 실패).
  * 도우미를 SSE2 intrinsic으로 바꾸자 present 스레드 CPU 시간이 약 12% 줄었다.
  * 그 intrinsic은 이제 선택 사항이다(CMake `REXBOX_RECOMP_XMM_SSE2`, 기본 `OFF`). 기본 빌드의 측정에는 이 이득이 들어 있지 않다([설계 i044 §3.4](../design/20261013-i044-recomp-codegen.md)).
  * 메모리 기준을 함수마다 한 번 읽는 것만으로는 차이가 없었다.
  * 그 뒤 lift 코드 샘플: guest 메모리 35%, TLS 레지스터 칸 21~25%, SSE 메모리 14%([설계 i044](../design/20261013-i044-recomp-codegen.md) §4).

## 4. 시도한 작은 변경과 효과

같은 시각에 변형 실행 파일을 번갈아 3회씩 실행해 present 스레드 CPU 시간(483 그리기 구간)을 비교했다.

| 변형 | 1회 | 2회 | 3회 | 평균 |
|---|---|---|---|---|
| 변경 없음 | 17.03 | 15.62 | 16.20 | 16.28 |
| HLE 레지스트리 비트맵 + draw마다의 보고 문자열 제거 | 16.88 | 15.42 | 15.83 | 16.04 |
| 위 + 생성 코드 `/GS-` | 17.08 | 15.42 | 15.94 | 16.15 |

* 차이는 실행 사이 흔들림(약 ±1 ms) 안에 있다.
* 레지스트리 비트맵과 보고 정리는 샘플에서 보이던 일(간접 호출마다 해시 조회, draw마다 `Format`)을 없애므로 유지한다.
* `/GS-`는 효과가 없어 넣지 않았다.

## 5. 측정할 때 주의할 것

* **기계 상태**: 같은 실행 파일의 같은 구간이 시각에 따라 35 ms와 25 ms로 나왔다. 클럭(터보·발열)과 다른 프로세스(예: VS Code의 색인) 탓이다. 그래서 다음을 지킨다.
  * 다른 빌드는 번갈아 여러 번 실행해 비교한다.
  * 벽시계 시간보다 `cpu` 열(present 스레드의 CPU 시간)을 본다.
* **프레임 덤프의 결정성**: 이전에는 같은 번호의 프레임 덤프가 실행마다 같았다(i040). 이번에는 카메라 위치가 몇 픽셀 달랐다. 데모가 빨라져 일부 프레임이 vblank 하나 안에 끝났기 때문이다.
  * **추정**: 게임은 경과한 vblank 수만큼 진행하고, 진행량에 상한이 있다. 모든 프레임이 상한 이상으로 늦을 때만 번호가 같은 프레임이 같아진다.
  * 렌더링 회귀는 같은 속도 조건에서 비교하거나 눈으로 확인한다.

## 6. 다음 후보

1. **xboxrecomp 코드 생성**: XMM intrinsic과 메모리 기준 지역화는 i044에서 했다.
   * XMM intrinsic은 그 뒤 선택 사항이 됐다. 기본 빌드(CMake `REXBOX_RECOMP_XMM_SSE2=OFF`)는 칸 단위 C를 쓰므로 i044의 약 12%는 켰을 때만 얻는다([설계 i044 §3.4](../design/20261013-i044-recomp-codegen.md)).
   * 남은 후보는 두 가지다.
     * 게스트 레지스터를 지역 변수로 두기: 상한은 leaf 함수만이면 전체 CPU 약 2%, 전부면 약 7%다.
     * 간접 호출 지점별 캐시: 약 1%.
2. **D3D8 변경 감지**: 정점 범위 `memcmp`(1.3 ms)와 텍스처 표본 해시를 게스트 메모리의 쓰기 감시로 바꾼다(Windows `MEM_WRITE_WATCH`/`GetWriteWatch`). 그러면 바뀐 페이지가 있는 범위만 비교하면 된다.
   * **확인됨**(i046, Ryzen 5 5600X): 조회는 범위당 약 0.7 µs라 `memcmp` 약 6 KiB와 같다. 그래서 8 KiB 이상 정점·인덱스 범위에만 쓴다.
   * 비교량이 프레임당 4.3 MB에서 1.4 MB로, present 스레드 CPU가 11% 줄었다.
   * 텍스처 표본 해시는 조회보다 싸서 그대로 둔다([설계 i046](../design/20261014-i046-hle-performance.md)).
3. **GL 호출 수**: **확인됨**(i050): 그리기 483개 프레임의 GL 호출이 10,505번이었다. 정점 속성 끄기·켜기, 그리기마다 프레임버퍼 바인딩, 상수 run마다의 `glUniform4fv`가 대부분이었다. 바뀐 것만 보내게 해 4,123번(-61%), present 스레드 CPU 약 -5%(Intel HD 620)가 됐다. 범위마다 VAO를 두는 2차는 2,115번까지 줄였지만 CPU 차이가 흔들림 안이어서 되돌렸다(VAO 전환이 공짜가 아님, **추정**; [설계 i050](../design/20261014-i050-gl-calls.md) §2.4).
4. **처음 쓸 때의 셰이더 컴파일**: 프로그램을 처음 쓰는 그리기에서 컴파일·링크해 첫 데모 구간의 setup이 14.5 ms까지 오르고, Intel 셰이더 컴파일러(`igc32`)가 프레임 2,400~3,600 샘플의 5.8%였다. **확인됨**(i051): 데모의 프로그램은 57개다. 바이너리를 디스크에 캐시하자 두 번째 실행부터 약 2.4초가 약 0.13초가 됐고, 데모 시작의 가장 긴 프레임은 약 2.0초에서 0.9~1.1초가 됐다. 남은 끊김은 처음 쓰는 텍스처의 업로드로 본다(**추정**; [설계 i051](../design/20261014-i051-program-cache.md)). NVIDIA GeForce RTX 4090(드라이버 616.56)에서는 컴파일이 개당 약 0.6 ms라 바이너리 불러오기(약 0.8~1.2 ms)가 오히려 느리고, 첫 실행은 저장 때문에 개당 약 1.8 ms가 더 든다. **확인됨**(i051 후속, [작업 로그](../work-logs/20261014-i051-program-cache.md)). 캐시의 이득은 드라이버마다 다르다. 캐시는 모든 드라이버에서 기본으로 켠다. 이득이 큰 쪽(컴파일이 비싼 드라이버)의 효과가 손해 쪽(프로그램당 1~2 ms)보다 훨씬 크기 때문이다.
5. **게스트 파일 쓰기(`NtWriteFile`)**: i042 샘플에 `ZwWriteFile`이 보였지만 호출자 추정은 믿을 수 없었다.
   * **확인됨**(i051 후속, RTX 4090, 데모 90초, 임시 계측): 게임의 쓰기는 부팅 때 두 번뿐이다. 첫 프레임 약 1.1초 뒤 `Media\Mark\DEFAULT\JSRF_TEXS0.JTX`·`JSRF_TEXS1.JTX`를 1 MB씩 쓰고, 각 0.28 ms, 0.26 ms(`flush` 포함)가 든다.
   * 호출 스레드는 present 스레드지만 느린 프레임(프레임 2,427~2,462 등)과 겹치지 않는다. 데모의 끊김 원인이 아니다.
   * **추정**: 데모 중 샘플의 `ZwWriteFile`은 측정 도구의 stderr 출력이나 GL 드라이버 자체의 셰이더 캐시 쓰기다. i042 로그에 그 비율이 남아 있지 않다.
   * **미확정**: Intel 기계의 주기적인 83~100 ms 프레임은 이 기계에서 재현되지 않았다. 실제 게임 진행(세이브, 그래피티 편집)에서는 `NtWriteFile`이 호출마다 동기로 `flush()`하므로 다시 본다.

6. **게임의 행렬 스택(MMATRIX) 디컴파일**: **확인됨**(i073, Intel HD 620, 프레임 2,700~3,300 샘플링).
   * lift된 게임 코드 샘플의 약 28%가 `MMATRIX` 섹션 함수 22개에 있었다(744개 / 전체 10,607개). 대기를 뺀 CPU의 약 9.5%다.
   * 21개를 SSE intrinsic으로 다시 쓰자(`src/game/jsrf/decomp/mmatrix.c`) 같은 구간 샘플이 142개 / 11,098개로 약 81% 줄었다. 프레임당 약 2 ms다.
   * 같은 시각 번갈아 잰 present 스레드 CPU는 부하가 적던 두 쌍에서 12.3~13.6 ms → 11.3 ms였다. 부하가 큰 한 쌍은 흔들림 안이었다([설계 i073](../design/20261016-i073-mmatrix.md)).
   * 다음 후보: 같은 방식으로, 호출이 잦고 짧은 다른 게임 함수(`sub_000B3E10`, `sub_000163D0`, `sub_0014DB70` 등)를 디컴파일한다.

---

# Frame time: where a frame of the opening 3D demo goes

This collects what was measured about the frame time of the opening 3D demo (the attract demo after the logos). The method and tools are in [design i042](../design/20261013-i042-frame-time-analysis.md), the process in [work log i042](../work-logs/20261013-i042-frame-time-analysis.md).

Environment: Win32 x86 Release, Intel Core i5-7200U (2 cores, 4 threads), Intel HD Graphics 620 (OpenGL 3.3, driver 26.20.100.7325), hidden window, no audio.

## 1. A frame

* **Confirmed** (`REXBOX_D3D8_PROFILE`, code after v0.0.16): in frames 2,701–3,000 (483 draws) a frame took 27.9 ms.
  * 16.5 ms is CPU time of the presenting thread: the title's code, the HLE and the GL driver.
  * 10.4 ms is the wait for a vertical blank: a late frame waits for the next blank (`WaitForPresentInterval`).
  * About 1 ms is time waiting for other threads or not scheduled.
* The draw path (from indices to the GL draw) is about 8 ms of that. The HLE calls that neither draw nor wait and the title's code ("other") take about 9.5 ms.
* Frames go out on vertical blanks (16.7 ms), so CPU work slightly over 16.7 ms makes a 33.3 ms frame. Sixty frames per second need the presenting thread's work steadily below 16.7 ms per frame.

## 2. The CPU work (sampling)

* **Confirmed** (`REXBOX_SAMPLE`, same window): without the sleep samples (`ZwDelayExecution`, the vblank wait), the 2,647 samples break down as follows.
  * lifted title code (`rejsrf_recomp`): 38%, about 6.3 ms per frame
  * D3D8 HLE: 26%, about 4.2 ms
  * GL driver (`ig9icd32`, `igc32`, `opengl32`): 16%, about 2.6 ms
  * recomp bridge and runtime (HLE entry, register access, registry lookups): 6%, about 1.0 ms
  * the rest (ntdll outside the wait, CRT, XInput HLE): 14%, about 2.3 ms
* **Lifted code**: the samples spread over more than 216 functions; the top 30 together are about 40% of the lifted code.
  * The heaviest group is the SSE vector and matrix transforms at 0x1BA9C0–0x1BB330 (`movss`/`shufps`/`mulps`).
  * Next come the title's matrix and render functions (`sub_000B3E10`, `sub_000163D0`, `sub_0014DB70` and others).
* **D3D8 HLE**: comparing vertex ranges (`memcmp`, the cache of i040) is the largest item, about 8% of the work or 1.3 ms. It is followed by comparing vertex constants (`SetArray4`), reading state (`TextureStateValue`) and texture sample hashes (`SampleHash`).
* **GL driver**: mostly the draw calls themselves and `ZwSetEvent` (**inferred**: the driver waking its worker thread).
* **Limit of the caller guess**: for a sample inside a DLL, the sampler records the first stack word pointing into the executable's code as the caller. Behind deep DLL calls (for example `sleep_until` → msvcp → kernelbase → ntdll), that can be a stale stack value, so the caller is only a hint.

## 3. The machine code of lifted functions

* **Confirmed** (`dumpbin`, `sub_001BAB20`): each lifted function carries the following.
  * Every function has a 16-byte stack-alignment prologue and a stack cookie (`/GS`).
  * Guest registers are memory slots in a TLS struct; a function looks up the TLS base once, then reads and writes memory for every register access.
  * `g_xbox_mem_offset` (the guest memory base) is read again after every store to guest memory, because the volatile store may alias that global.
  * A scalar `movss` goes through a zeroed XMM temporary in memory.
* **Confirmed** (measurement): turning `/GS` off for the generated code alone made no difference (§4).
* **Inferred**: a large share of the remaining cost comes from how xboxrecomp generates code (registers kept in memory, reloads of the memory base).
* **Confirmed** (i044): the hottest instructions were `movaps`/`movups` reading stack temporaries the XMM helpers had just written in parts, a failed store-to-load forward.
  * SSE2 intrinsics for the helpers cut the presenting thread's CPU time by about 12%.
  * Those intrinsics are now opt-in (CMake `REXBOX_RECOMP_XMM_SSE2`, default `OFF`); measurements of the default build do not include that gain ([design i044 §3.4](../design/20261013-i044-recomp-codegen.md)).
  * Reading the memory base once per function alone made no difference.
  * Afterwards lifted-code samples are 35% guest memory, 21–25% TLS register slots and 14% SSE memory ([design i044](../design/20261013-i044-recomp-codegen.md) §4).

## 4. Small changes tried, and their effect

Variant executables were run alternately, three times each, close together in time. The comparison uses the presenting thread's CPU time in the 483-draw window.

| Variant | Run 1 | Run 2 | Run 3 | Mean |
|---|---|---|---|---|
| No change | 17.03 | 15.62 | 16.20 | 16.28 |
| HLE registry bitmap + no per-draw report strings | 16.88 | 15.42 | 15.83 | 16.04 |
| The above + `/GS-` on the generated code | 17.08 | 15.42 | 15.94 | 16.15 |

* The differences are within the run-to-run spread (about ±1 ms).
* The registry bitmap and the report cleanup remove work the samples showed (a hash lookup on every indirect call, `Format` on every draw), so they stay.
* `/GS-` had no effect and was not adopted.

## 5. Measuring

* **Machine state**: the same executable gave 35 ms and 25 ms for the same window at different times of day, because of clock speed (turbo, heat) and other processes (for example VS Code indexing). So:
  * Compare builds by alternating runs, several times each.
  * Prefer the `cpu` column (the presenting thread's CPU time) over wall time.
* **Determinism of frame dumps**: earlier, frame dumps with the same number were identical between runs (i040). This time the camera position differed by a few pixels, because the demo is now fast enough that some frames finish within one vblank.
  * **Inferred**: the game advances by the vertical blanks that passed, up to a cap, so frames with the same number match only when every frame is at least that late.
  * Compare rendering at equal speed, or by eye.

## 6. Next candidates

1. **xboxrecomp code generation**: the XMM intrinsics and the local memory base were done in i044.
   * The XMM intrinsics later became optional: the default build (CMake `REXBOX_RECOMP_XMM_SSE2=OFF`) uses the lane-wise C, so i044's roughly 12% applies only when they are switched on ([design i044 §3.4](../design/20261013-i044-recomp-codegen.md)).
   * Two candidates remain:
     * guest registers in locals: at most about 2% of total CPU for leaf functions only, about 7% for everything
     * per-site indirect-call caches: about 1%
2. **D3D8 change detection**: replace the vertex-range `memcmp` (1.3 ms) and the texture sample hash with write watching on guest memory (Windows `MEM_WRITE_WATCH`/`GetWriteWatch`), so only ranges with written pages are compared.
   * **Confirmed** (i046, Ryzen 5 5600X): a query costs about 0.7 µs per range, as much as a `memcmp` of about 6 KiB, so only vertex and index ranges of 8 KiB or more use it.
   * The bytes compared per frame fell from 4.3 MB to 1.4 MB, and presenting-thread CPU by 11%.
   * The texture sample hash costs less than a query and stays ([design i046](../design/20261014-i046-hle-performance.md)).
3. **Number of GL calls**: **Confirmed** (i050): a 483-draw frame made 10,505 GL calls, mostly attribute disables and enables, a framebuffer bind per draw and one `glUniform4fv` per constant run. Sending only what changes left 4,123 (-61%) and cut presenting-thread CPU by about 5% (Intel HD 620). A second pass with a VAO per range reached 2,115 calls but stayed within the CPU noise and was reverted (a VAO switch is not free, **inferred**; [design i050](../design/20261014-i050-gl-calls.md) §2.4).
4. **Shader compilation on first use**: programs are compiled and linked on the first draw that needs them; setup in the first demo window reaches 14.5 ms, and the Intel shader compiler (`igc32`) was 5.8% of the samples over frames 2,400–3,600. **Confirmed** (i051): the demo uses 57 programs. With their binaries cached on disk, from the second run on they take about 0.13 s instead of about 2.4 s, and the longest frame at the start of the demo fell from about 2.0 s to 0.9–1.1 s. The rest of that stall is taken to be textures uploaded on first use (**inferred**; [design i051](../design/20261014-i051-program-cache.md)). On an NVIDIA GeForce RTX 4090 (driver 616.56), compiling takes about 0.6 ms a program, so loading a binary (about 0.8–1.2 ms) is slower, and a first run costs about 1.8 ms more a program for the stores. **Confirmed** (i051 follow-up, [work log](../work-logs/20261014-i051-program-cache.md)). The cache's gain depends on the driver. The cache is on by default on every driver, since its gain where compiling is expensive far outweighs its cost elsewhere (1–2 ms a program).
5. **Guest file writes (`NtWriteFile`)**: `ZwWriteFile` showed in the i042 samples, but its guessed caller could not be trusted.
   * **Confirmed** (i051 follow-up, RTX 4090, 90 s of the demo, temporary instrumentation): the title writes only twice, at boot. About 1.1 s after the first frame it writes `Media\Mark\DEFAULT\JSRF_TEXS0.JTX` and `JSRF_TEXS1.JTX`, 1 MB each, taking 0.28 ms and 0.26 ms (with the `flush`).
   * The calls are on the presenting thread but do not meet the slow frames (frames 2,427–2,462 and others). They are not the cause of the demo's stutter.
   * **Inferred**: the `ZwWriteFile` in samples during the demo is the measuring tools' stderr output or the GL driver writing its own shader cache. The i042 log does not keep its share.
   * **Unresolved**: the periodic 83–100 ms frames on the Intel machine did not reproduce here. In real play (saves, graffiti editing) `NtWriteFile` flushes synchronously on every call, so it is to be looked at again.
6. **Decompiling the title's matrix stack (MMATRIX)**: **Confirmed** (i073, Intel HD 620, sampling frames 2,700–3,300).
   * About 28% of the lifted title code's samples sat in 22 functions of the `MMATRIX` section (744 of 10,607 in all), about 9.5% of non-wait CPU.
   * With 21 of them rewritten with SSE intrinsics (`src/game/jsrf/decomp/mmatrix.c`), the same window drew 142 of 11,098 samples, about 81% fewer, or about 2 ms per frame.
   * Presenting-thread CPU, measured alternately, went from 12.3–13.6 ms to 11.3 ms in the two lightly loaded pairs; a heavily loaded pair stayed within the noise ([design i073](../design/20261016-i073-mmatrix.md)).
   * Next: decompile other short, often-called title functions the same way (`sub_000B3E10`, `sub_000163D0`, `sub_0014DB70` and others).
