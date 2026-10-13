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

1. **xboxrecomp 코드 생성**: XMM intrinsic과 메모리 기준 지역화는 i044에서 했다. 남은 후보는 두 가지다.
   * 게스트 레지스터를 지역 변수로 두기: 상한은 leaf 함수만이면 전체 CPU 약 2%, 전부면 약 7%다.
   * 간접 호출 지점별 캐시: 약 1%.
2. **D3D8 변경 감지**: 정점 범위 `memcmp`(1.3 ms)와 텍스처 표본 해시를 게스트 메모리의 쓰기 감시로 바꾼다(Windows `MEM_WRITE_WATCH`/`GetWriteWatch`). 그러면 바뀐 페이지가 있는 범위만 비교하면 된다.
3. **GL 호출 수**: 정점 상수를 uniform buffer 하나로 보내고, 상태 묶음을 줄여 드라이버 시간(2.6 ms)을 줄인다.

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

1. **xboxrecomp code generation**: the XMM intrinsics and the local memory base were done in i044. Two candidates remain:
   * guest registers in locals: at most about 2% of total CPU for leaf functions only, about 7% for everything
   * per-site indirect-call caches: about 1%
2. **D3D8 change detection**: replace the vertex-range `memcmp` (1.3 ms) and the texture sample hash with write watching on guest memory (Windows `MEM_WRITE_WATCH`/`GetWriteWatch`), so only ranges with written pages are compared.
3. **Number of GL calls**: send vertex constants as one uniform buffer and reduce state groups, to cut driver time (2.6 ms).
