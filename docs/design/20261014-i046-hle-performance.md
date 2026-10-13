# 설계: 저사양 호스트를 위한 HLE 비용 줄이기 (issue #46)

선행: [frame-time](../analysis/frame-time.md), [설계 i042](20261013-i042-frame-time-analysis.md), [설계 i044 §3.4](20261013-i044-recomp-codegen.md)

## 1. 요구사항

* 개발 PC(Ryzen 5 5600X, RTX 4090)에서는 데모가 60 fps를 유지한다. 그래도 저사양 호스트에서 60 fps를 지키려면 CPU 작업을 줄여야 한다.
* lift 코드 생성은 당분간 손대지 않는다(i044 §3.4, SSE2 XMM 도우미는 선택 사항). 이번에는 HLE 쪽 비용을 줄인다.
* 그리기 결과와 동작은 바뀌지 않아야 한다. 변경 감지를 더 싸게 하더라도, 놓치는 변경이 지금보다 늘어서는 안 된다.

## 2. 측정 (기준)

환경: Win32 x86 Release(SSE2 XMM 끔), 숨긴 창, 오디오 없음, 프레임 2,701~3,000(그리기 483개).

* `REXBOX_D3D8_PROFILE`: present 스레드 CPU 5.3~5.7 ms/프레임. 그리기 경로는 약 2.3 ms다.
  * upload 0.92, setup 0.51, indices 0.32, constants 0.25 ms
  * 나머지 "other" 약 2.9 ms는 lift 코드와 그리기가 아닌 HLE 호출이다.
* `REXBOX_SAMPLE`(잠자기를 뺀 샘플 726개):
  * lift 코드 41%, D3D8 HLE 29%, GL 드라이버 약 6%
  * D3D8 안에서 가장 큰 것은 `Unchanged`(캐시한 정점·인덱스 범위의 `memcmp`)로, 전체 작업의 12%다.
  * 그 뒤로 `SampleHash`, `UniformCache::SetArray4`, 렌더 타깃 조회(`Color`, `std::map` 조회)가 있다.
  * `recomp_unimpl`도 보인다. 번역되지 않은 `stmxcsr`(0015F0F1)를 만날 때마다 mutex를 잡고 해시 집합을 조회한다.
* **비교량**(새 프로필 열 `compared`): 프레임마다 약 960개 범위, 4.3 MB를 비교한다. 바뀌어서 다시 올라가는 양은 2 KiB뿐이다.

| 범위 크기 | 범위 수/프레임 | KiB/프레임 |
|---|---|---|
| < 1 KiB | 487 | 153 |
| 1~4 KiB | 253 | 543 |
| 4~16 KiB | 166 | 1,249 |
| 16~64 KiB | 46 | 1,470 |
| 64~256 KiB | 9 | 906 |

* **비용 측정**(같은 PC, 32비트 프로세스):
  * `memcmp`(같은 내용): 1 KiB 0.40 µs, 16 KiB 2.0 µs, 64 KiB 5.3 µs
  * `GetWriteWatch`(리셋 포함): 16 KiB 범위 0.68 µs, 128 MiB 전체 146 µs
  * 리셋 뒤 페이지에 처음 쓰면 페이지당 약 0.6 µs가 더 든다(감시하지 않는 메모리는 0.01 µs).

## 3. 설계

```mermaid
flowchart LR
    W["게스트 코드·HLE의 쓰기"] -->|"OS가 쓴 페이지 기록"| P["platform: TakeWrittenPages"]
    P --> T["runtime: GuestMemory::WriteVersion<br/>페이지별 쓰기 버전"]
    T --> V["D3D8 정점·인덱스 범위(8 KiB 이상)"]
    V -->|"버전이 같으면"| S["비교 생략"]
    V -->|"버전이 새로우면"| M["memcmp(지금과 같음)"]
```

### 3.1 게스트 메모리 쓰기 버전

* **플랫폼 계약**(`include/rexbox/platform/virtual_memory.h`):
  * `AllocateVirtualMemory`에 `track_writes` 인자를 더한다. 켜면 OS가 쓰인 페이지를 기록한다(Win32 `MEM_WRITE_WATCH`).
  * `TakeWrittenPages(address, size, pages, capacity, count)`를 더한다. 마지막 조회 뒤 쓰인 페이지를 돌려주고 기록을 지운다(Win32 `GetWriteWatch`와 `WRITE_WATCH_FLAG_RESET`).
  * 기록할 수 없으면(다른 OS, 추적하지 않은 블록, 실패) false를 돌려준다. 호출자는 모든 페이지가 쓰였다고 본다.
* **런타임**(`GuestMemory`):
  * 페이지마다 쓰기 버전을 둔다. 128 MiB / 4 KiB = 32,768개, `uint64_t`라 256 KiB다.
  * `WriteVersion(address, length)`는 범위를 덮는 페이지를 조회한다. 쓰인 페이지에는 새 버전을 매기고, 범위 안 페이지의 가장 큰 버전을 돌려준다.
  * 추적할 수 없으면 부를 때마다 새 버전을 돌려준다. 그러면 호출자는 언제나 지금처럼 비교한다.
  * mutex로 보호한다. 여러 게스트 스레드가 HLE를 부를 수 있기 때문이다.
* **정확성**:
  * 조회가 페이지를 리셋하는 시점보다 뒤에 일어난 쓰기는 다음 조회에서 잡힌다. 그래서 버전이 같다는 것은 지난 확인 뒤 그 페이지들에 쓰기가 없었다는 뜻이다.
  * 리셋한 뒤 데이터를 복사하는 사이에 쓰기가 끼어들면, 다음 조회에서 다시 잡혀 한 번 더 비교한다(올리는 것이 아니라 비교).
  * 두 범위가 한 페이지를 나눠 쓰면, 한쪽의 조회가 다른 쪽에도 새 버전으로 보인다. 결과는 한 번 더 비교하는 것뿐이다.
  * OS는 쓴 주체를 가리지 않는다. 그래서 HLE가 `Translate` 포인터로 쓴 것도 잡힌다.
* **비용**:
  * 조회한 페이지에서만, 리셋 뒤 첫 쓰기가 페이지당 약 0.6 µs 더 든다.
  * 조회는 범위당 약 0.7 µs다. 그래서 작은 범위에는 쓰지 않는다(§3.2).

### 3.2 D3D8 변경 감지에 쓰기

* **정점·인덱스 범위**: `kTrackedBytes`(8 KiB) 이상인 범위만 쓰기 버전을 쓴다.
  * 조회 0.7 µs는 `memcmp` 약 6 KiB에 해당한다. 8 KiB 이상은 프레임당 약 130개 범위에 3 MB(비교량의 70%)다.
  * 버전이 지난 확인 때와 같으면 비교하지 않는다. 새로우면 지금처럼 `memcmp`하고, 다르면 올린다.
  * 작은 범위는 지금처럼 `memcmp`한다.
* **텍스처에는 쓰지 않는다**(구현 뒤 측정으로 결정): 프레임마다 한 번 하는 표본 해시(`SampleHash`, level 0) 앞에서 쓰기 버전을 보게 해 보았다.
  * 조회(`ZwGetWriteWatch`) 샘플이 46개로, 25개였던 정점·인덱스만일 때보다 21개 늘었다. 줄어든 해시 샘플은 약 15개였다.
  * 텍스처 수가 많고, 표본 해시는 256개 DWORD만 읽어 조회 한 번보다 싸다. 그래서 텍스처는 지금처럼 해시만 쓴다.

### 3.3 정점 셰이더 상수 버전

* `g_vertex_constants`에 쓰는 곳은 `SetVertexShaderConstant`와 `WriteViewportConstants` 둘뿐이다. 두 곳에서 상수 버전을 올린다.
* `UniformCache::SetArray4`는 마지막으로 보낸 버전을 기억한다. 버전이 같으면 192개 상수(3 KiB)를 비교하지 않는다.

### 3.4 한 번만 기록하는 진단의 빠른 경로

* `recomp_unimpl`, `recomp_icall_*_log`는 `FirstTimeAt`으로 같은 지점을 한 번만 기록한다. 매 호출이 mutex와 해시 집합 조회를 거친다.
* 스레드마다 이미 본 지점을 64칸 직접 사상 표에 기억한다. 표에 있으면 잠그지 않고 돌아간다.
* D3D8의 `FirstReport`(`ReportOncePer`)도 같다. 데모에서는 아직 구현하지 않은 상태(텍스처 좌표 생성, 텍스처 변환, 조명)를 그리기마다 보고하려 한다. 그때마다 mutex를 잡고 `std::set<pair>`를 조회한다(샘플의 `_Find_lower_bound`). 스레드별 64칸 표를 둔다.
* `SetGammaRamp`는 단위 램프가 아니면 호출마다 trace 문자열 두 개를 만들었다(trace가 꺼져 있어도). trace가 켜졌을 때만 만든다.

### 3.5 렌더 타깃 조회

* `BindRenderTarget`는 그리기마다 `std::map` 조회 두 번(프레임버퍼, 색 표면)을 한다.
* 마지막 (색, 깊이) 쌍과 그 결과(프레임버퍼, 크기)를 기억한다. 표면과 프레임버퍼 표는 커지기만 하고 지우지 않으므로, 한 번 찾은 결과는 계속 유효하다.
* `glBindFramebuffer` 호출은 그대로 둔다. 다른 경로가 프레임버퍼를 바꾸기 때문이다.

### 3.6 하지 않은 것

* **모든 범위에 쓰기 감시**: 범위당 조회 0.7 µs × 960개 = 0.67 ms로, 지금의 `memcmp`보다 비싸다.
* **프레임마다 한 번 전체 조회**: 146 µs로 싸지만, 프레임 안에서 그리기 사이에 일어난 쓰기를 놓친다.
* **`D3DVertexBuffer_Lock`으로 쓰기 추적**: Xbox 게임은 잠근 포인터를 계속 들고 쓸 수 있어 안전하지 않다.

## 4. 검증

| 검증 | 기대 |
|---|---|
| 회귀 도구(probe 3종, decomp check, d3d8 check) | 실패 0 |
| 데모 실행, 로고 프레임 덤프 | 기준과 같음 |
| 프로필 `compared` | 8 KiB 이상 범위의 비교가 사라짐 |
| 데모 프레임 덤프 | 같은 장면. 빨라진 만큼 진행량이 달라 카메라가 다를 수 있음([frame-time §5](../analysis/frame-time.md)) |
| present 스레드 CPU(기준과 번갈아 3회) | 줄어듦 |

* 다른 OS: `TakeWrittenPages`가 false이므로 지금처럼 동작한다(Win32 외 플랫폼은 아직 빌드 대상이 아니다).

---

# Design: cheaper HLE work for low-end hosts (issue #46)

Prerequisites: [frame-time](../analysis/frame-time.md), [design i042](20261013-i042-frame-time-analysis.md), [design i044 §3.4](20261013-i044-recomp-codegen.md)

## 1. Requirements

* On the development PC (Ryzen 5 5600X, RTX 4090) the demo holds 60 fps. To hold it on low-end hosts too, the CPU work must shrink.
* Lifted-code generation is left alone for now (i044 §3.4; the SSE2 XMM helpers are opt-in). This time the HLE's cost comes down.
* Pictures and behavior must not change. Cheaper change detection must not miss more changes than it does now.

## 2. Measurements (baseline)

Environment: Win32 x86 Release (SSE2 XMM off), hidden window, no audio, frames 2,701–3,000 (483 draws).

* `REXBOX_D3D8_PROFILE`: presenting-thread CPU is 5.3–5.7 ms per frame. The draw path is about 2.3 ms of it.
  * upload 0.92, setup 0.51, indices 0.32, constants 0.25 ms
  * The remaining "other", about 2.9 ms, is lifted code and HLE calls that do not draw.
* `REXBOX_SAMPLE` (726 samples without sleeping):
  * lifted code 41%, D3D8 HLE 29%, GL driver about 6%
  * Inside D3D8 the largest item is `Unchanged` (the `memcmp` of cached vertex and index ranges): 12% of all work.
  * After it come `SampleHash`, `UniformCache::SetArray4`, and the render-target lookups (`Color`, a `std::map` lookup).
  * `recomp_unimpl` shows too: every time it meets the untranslated `stmxcsr` (0015F0F1), it takes a mutex and looks up a hash set.
* **Bytes compared** (new profile column `compared`): about 960 ranges and 4.3 MB per frame. Only 2 KiB of it changed and was uploaded again.
  * The table in the Korean section gives the sizes: below 1 KiB, 487 ranges and 153 KiB; 1–4 KiB, 253 and 543 KiB; 4–16 KiB, 166 and 1,249 KiB; 16–64 KiB, 46 and 1,470 KiB; 64–256 KiB, 9 and 906 KiB.
* **Costs measured** (same PC, 32-bit process):
  * `memcmp` of equal data: 1 KiB 0.40 µs, 16 KiB 2.0 µs, 64 KiB 5.3 µs
  * `GetWriteWatch` with reset: 0.68 µs for a 16 KiB range, 146 µs for all 128 MiB
  * The first write to a page after a reset costs about 0.6 µs more (0.01 µs on unwatched memory).

## 3. Design

The diagram in the Korean section applies: writes by guest code and the HLE are recorded per page by the OS (platform `TakeWrittenPages`); the runtime turns them into per-page write versions (`GuestMemory::WriteVersion`); the D3D8 vertex and index ranges of 8 KiB or more skip their comparison when the version has not moved, and fall back to `memcmp` as now when it has.

### 3.1 Guest memory write versions

* **Platform contract** (`include/rexbox/platform/virtual_memory.h`):
  * `AllocateVirtualMemory` gains a `track_writes` argument. When it is set, the OS records written pages (Win32 `MEM_WRITE_WATCH`).
  * New: `TakeWrittenPages(address, size, pages, capacity, count)`. It returns the pages written since the last query and clears the record (Win32 `GetWriteWatch` with `WRITE_WATCH_FLAG_RESET`).
  * When nothing can be recorded (another OS, an untracked block, a failure), it returns false, and the caller treats every page as written.
* **Runtime** (`GuestMemory`):
  * A write version per page: 128 MiB / 4 KiB = 32,768 of them, 256 KiB as `uint64_t`.
  * `WriteVersion(address, length)` queries the pages covering the range. Written pages get a new version, and the largest version in the range is returned.
  * Without tracking, every call returns a new version, so callers always compare as they do now.
  * A mutex guards it, since several guest threads can call the HLE.
* **Correctness**:
  * A write after the query resets the page is caught by the next query. So an unchanged version means no write to those pages since the last check.
  * A write between the reset and the copy is caught again by the next query, which then compares once more (it compares; it does not upload).
  * When two ranges share a page, one range's query shows the other a new version too. The result is one more comparison.
  * The OS does not care who writes, so HLE writes through `Translate` pointers are caught too.
* **Cost**:
  * Only on queried pages, the first write after a reset costs about 0.6 µs more per page.
  * A query is about 0.7 µs per range, so small ranges do not use it (§3.2).

### 3.2 Using it for D3D8 change detection

* **Vertex and index ranges**: only ranges of at least `kTrackedBytes` (8 KiB) use write versions.
  * A 0.7 µs query costs as much as a `memcmp` of about 6 KiB. Ranges of 8 KiB or more are about 130 ranges and 3 MB per frame (70% of the bytes compared).
  * If the version is the one seen at the last check, there is no comparison. If it is newer, `memcmp` runs as now, and a difference uploads.
  * Smaller ranges keep `memcmp`.
* **Not for textures** (decided by measuring after implementing it): the once-per-frame sampled hash (`SampleHash`, level 0) was made to look at the write version first.
  * Query samples (`ZwGetWriteWatch`) rose to 46, 21 more than the 25 with vertex and index ranges only, while hash samples fell by about 15.
  * There are many textures, and the sampled hash reads only 256 DWORDs, which costs less than one query. Textures keep the hash alone.

### 3.3 Vertex shader constant version

* Only `SetVertexShaderConstant` and `WriteViewportConstants` write `g_vertex_constants`. Both raise a constant version.
* `UniformCache::SetArray4` remembers the version it last sent. When it is unchanged, the 192 constants (3 KiB) are not compared.

### 3.4 A fast path for log-once diagnostics

* `recomp_unimpl` and `recomp_icall_*_log` log each site once through `FirstTimeAt`. Every call takes a mutex and looks up a hash set.
* Each thread keeps the sites it has seen in a 64-slot direct-mapped table. A site in the table returns without locking.
* D3D8's `FirstReport` (`ReportOncePer`) is the same: in the demo, states not implemented yet (texture coordinate generation, texture transforms, lighting) are reported on every draw, each time taking a mutex and looking up a `std::set<pair>` (the samples' `_Find_lower_bound`). It gets a per-thread 64-slot table too.
* `SetGammaRamp` built two trace strings on every call with a non-identity ramp, even with tracing off. It now builds them only when tracing.

### 3.5 Render-target lookup

* `BindRenderTarget` does two `std::map` lookups per draw (the framebuffer and the color surface).
* It remembers the last (color, depth) pair and its results (framebuffer, size). The surface and framebuffer tables only grow and are never erased, so a result once found stays valid.
* The `glBindFramebuffer` call stays, since other paths change the framebuffer.

### 3.6 Not done

* **Write watching for every range**: 0.7 µs per query × 960 ranges = 0.67 ms, more than the `memcmp` now.
* **One whole-space query per frame**: cheap at 146 µs, but it misses writes between draws within a frame.
* **Tracking writes through `D3DVertexBuffer_Lock`**: an Xbox title may keep a locked pointer and write through it later, so it is not safe.

## 4. Verification

The table in the Korean section applies: the regression tools (three probes, decomp check, d3d8 check) have 0 failures; the demo runs and the logo frame dumps match the baseline; the profile's `compared` loses the ranges of 8 KiB or more; demo frame dumps show the same scene, though the camera may differ because a faster build advances the game differently ([frame-time §5](../analysis/frame-time.md)); presenting-thread CPU, measured alternately with the baseline three times, goes down.

* Other OSes: `TakeWrittenPages` returns false, so they behave as now (platforms other than Win32 are not build targets yet).
