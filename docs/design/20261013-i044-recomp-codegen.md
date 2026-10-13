# 설계: lift된 코드를 싸게 만드는 xboxrecomp 개선 (issue #44)

선행: [i042 프레임 시간 분석](20261013-i042-frame-time-analysis.md), [frame-time](../analysis/frame-time.md)

## 1. 요구사항

* i042에서 lift된 게임 코드가 데모 프레임 CPU 작업에서 가장 큰 몫(38%)이었다. 비용은 수백 개 함수에 퍼져 있었다.
* 그래서 함수 하나가 아니라 코드 생성 방식을 고친다.
* xboxrecomp 수정은 fork에서 한다. 변경마다 upstream main 기반 브랜치를 만들고 `rejsrf`에 합친다.
* **upstream PR은 모았다가 한 번에 낸다**(사용자 결정, 2026-10-13). 다시 말할 때까지 올리지 않는다.
* **XMM intrinsic은 우리 fork에만 둔다**(사용자 결정, 2026-10-13). upstream 헤더는 "게스트 의미를 드러내고 이식 가능하게 두려고 intrinsic이 아닌 칸 단위 C로 쓴다"고 밝히고 있어, 그 설계와 다른 변경은 upstream에 내지 않는다. 우리 브랜치에서는 그 주석이 두 경로를 모두 설명하게 고쳤다.
* 각 변경은 present 스레드 CPU 시간(빌드를 번갈아 실행)으로 재고, 생성 코드의 정확성(테스트, 회귀 도구, 화면)을 지킨다.

## 2. 찾은 비용 (기계어와 명령 단위 샘플)

* `REXBOX_SAMPLE`의 샘플을 lift 코드 범위의 `dumpbin` 디스어셈블과 맞춰, 샘플이 떨어진 명령을 분류했다.
* **v0.0.17**:
  * 메모리 접근 50%
  * SSE 메모리 연산 28%
  * 가장 뜨거운 명령들은 `[ebp-20h]` 같은 스택 임시를 읽는 `movaps`/`movups`였다.
* **원인 1 — XMM 도우미**:
  * `RecompXmm` union을 칸 단위 C로 만드는 도우미(`XMM_SCALAR`, `XMM_MUL`, `XMM_SHUFFLE` 등)를 MSVC가 다음 순서로 번역했다.
    1. 16바이트 스택 임시에 부분 저장(8바이트 두 번 또는 4바이트)한다.
    2. 곧바로 16바이트로 읽는다.
  * 이 순서는 store-to-load forwarding이 실패하는 패턴이다.
  * 정렬된 임시 때문에 함수마다 스택 정렬 prologue와 `/GS` 쿠키도 붙었다.
* **원인 2 — 메모리 기준 재로드**: `XBOX_PTR`는 전역 `g_xbox_mem_offset`을 더한다. volatile guest 쓰기가 그 전역과 겹칠 수 있다고 보므로 쓰기마다 다시 읽었다.
* **원인 3 — TLS 레지스터**: 게스트 레지스터가 TLS 메모리 칸이다. 같은 이유로 guest 쓰기 뒤마다 다시 읽는다.

## 3. 설계

### 3.1 XMM 도우미의 SSE2 intrinsic (`perf/xmm-intrinsics`)

* 컴파일러가 SSE2를 대상으로 하면(MSVC x86 기본 `/arch:SSE2`, `__SSE2__`, x64) 자주 쓰는 도우미를 intrinsic으로 구현한다. intrinsic은 그 명령 자체라서 결과가 게스트 하드웨어와 같다.
* intrinsic으로 바꾼 도우미:
  * 생성: zero, scalar, scalar double, scalar bits
  * guest 메모리: 16·8바이트 load/store. 사용 지점의 `XBOX_PTR`를 쓰도록 매크로로 둔다.
  * 산술·비트 연산: add/sub/mul/div/min/max, and/or/xor/andn
  * EQ/LT/LE/NEQ 비교, sqrt, `movmskps`
  * `shufps`: 선택자가 상수여야 하므로 매크로로 둔다. lifter는 항상 상수를 낸다.
  * unpack, `movlhps`/`movhlps`
* **guest 메모리 접근**: 예전의 volatile 32비트 4번 대신 일반 포인터로 16·8바이트 한 번 접근한다(게스트 명령과 같은 크기). 컴파일러가 다른 비volatile 접근과 합치거나 순서를 바꿀 수 있다.
* **plain C로 남기는 것**:
  * RSQRT/RCP: 하드웨어 근사값의 비트가 CPU마다 다르다.
  * 가변 술어 비교
  * `RECOMP_XMM_PORTABLE`를 정의하면 전부 plain C다.
* `RecompXmm`은 8바이트 정렬을 유지한다. MSVC x86은 16바이트 정렬 타입을 값으로 넘기지 못하기 때문이다. 값은 비정렬 16바이트 이동으로 오간다.
* **검증**: `test_xmm_intrinsics`가 같은 드라이버를 두 방식으로 빌드한다. 모든 도우미를 부호 있는 0, 무한대, NaN, 비정규수, 일반값에 돌려 비트 단위로 비교한다. 컴파일러는 clang, gcc, 또는 PATH의 cl이다. 일부러 넣은 버그(`ANDN`을 `AND`로)를 잡는 것도 확인했다.

### 3.2 함수마다 메모리 기준을 한 번 읽기 (`perf/mem-base-local`)

* 번역된 청크는 `RECOMP_LOCAL_MEM_BASE`를 정의한다.
* 번역된 함수는 진입 때 `RECOMP_MEM_BASE_DECL`로 기준을 지역 상수에 복사한다.
* `XBOX_PTR`는 청크 안에서 그 지역 상수를 쓴다. 전환은 헤더의 모든 도우미 뒤에 있으므로 도우미는 전역을 그대로 쓴다.
* 기준은 메모리를 매핑한 뒤 바뀌지 않는다.

### 3.3 하지 않은 것

* **TLS 레지스터를 지역 변수로 두기**: 호출 지점(약 3만 7천 곳)과 반환마다 되쓰고 다시 읽어야 한다. 효과 상한은 §4의 측정으로 정했다.
* **간접 호출의 지점별 캐시**: `recomp_lookup`의 평면 테이블(약 7 MB) 캐시 미스. lift 코드의 약 2%다.

### 3.4 SSE2 intrinsic은 선택 사항 (후속, 2026-10-13)

* **사용자 결정(2026-10-13)**: §3.1의 SSE2 intrinsic은 기본으로 끄고 필요할 때만 켠다. 당분간 성능 작업은 HLE 쪽에 집중한다.
* fork 헤더는 바꾸지 않는다. reJSRF 빌드가 `RECOMP_XMM_PORTABLE`을 정의해 칸 단위 C(upstream 설계)를 고른다.
  * CMake 옵션 `REXBOX_RECOMP_XMM_SSE2`(기본 `OFF`)를 `rexbox_recomp`에 둔다. `rexbox_recomp`를 대상으로 두는 옵션이라 이름이 타이틀이 아닌 `REXBOX_`로 시작한다.
  * `OFF`이면 `rexbox_recomp`가 `RECOMP_XMM_PORTABLE`를 PUBLIC으로 정의한다. 그래서 `recomp_types.h`를 포함하는 모든 타깃(`rexbox_recomp`, `rejsrf_recomp`, `rejsrf_game`)이 같은 경로를 쓴다.
  * `ON`(`cmake -DREXBOX_RECOMP_XMM_SSE2=ON`)이면 v0.0.18과 같다.
* 헤더 내용이 그대로이므로 ABI 해시 검사(`src/game/jsrf/CMakeLists.txt`)와 생성 코드에는 영향이 없다. 옵션을 바꿔도 재생성할 필요 없이 다시 빌드만 하면 된다.
* 대가: §4의 XMM intrinsic 이득(present 스레드 CPU 약 -12%)을 기본 빌드에서 잃는다. `perf/mem-base-local`은 그대로 둔다.

## 4. 측정

| 구간(그리기 수) | v0.0.17 | + 메모리 기준 | + XMM intrinsic |
|---|---|---|---|
| w10 (483) | 15.24 | 15.38 | 14.17 |
| w11 (372) | 14.79 | 15.07 | 12.88 |
| w12 (277) | 10.99 | 10.47 | 9.41 |
| w13 (321) | 12.07 | 12.40 | 10.31 |

(present 스레드 CPU ms/프레임, 같은 시각 번갈아 3회 평균)

* XMM intrinsic으로 네 구간 합이 53.1 ms에서 46.8 ms가 됐다(-12%).
* 메모리 기준만으로는 흔들림 밖의 차이가 없다. 그래도 XMM 매크로가 사용 지점의 지역 기준을 쓰게 해 주므로 둘을 함께 둔다.
* XMM 뒤 lift 코드 샘플: guest 메모리 35%, TLS 레지스터 칸 21~25%, SSE 메모리 14%.
* 레지스터 칸 샘플 중 호출 없는 leaf 함수 몫은 lift 코드의 7.5%다. 지역화의 상한은 다음과 같다.
  * leaf 함수만: 전체 CPU 약 2%
  * 전부: 전체 CPU 약 7%(프레임당 약 1 ms)

## 5. 검증

| 검증 | 기대 |
|---|---|
| xboxrecomp 단위 테스트, `test_xmm_intrinsics`(cl) | 통과 |
| 재생성·Debug/Release 빌드, 회귀 도구 | 경고 0, 회귀 없음 |
| 프레임 2,800~3,400 | v0.0.15와 같음 |

---

# Design: cheaper lifted code through xboxrecomp (issue #44)

Prerequisites: [i042 frame-time analysis](20261013-i042-frame-time-analysis.md), [frame-time](../analysis/frame-time.md)

## 1. Requirements

* i042 found lifted title code to be the largest share (38%) of a demo frame's CPU work, spread over hundreds of functions.
* So the fix goes into how the code is generated, not into one function.
* xboxrecomp changes are made on the fork, one upstream-based branch per change, merged into `rejsrf`.
* **Upstream PRs are collected and submitted together later** (user decision, 2026-10-13). Nothing is sent upstream until the user says so.
* **The XMM intrinsics stay on our fork only** (user decision, 2026-10-13). The upstream header says it writes the helpers as lane-wise C rather than intrinsics, to keep the guest semantics explicit and the header portable; a change against that design is not proposed upstream. On our branch, that comment now describes both paths.
* Each change is measured by the presenting thread's CPU time (builds run alternately), and the generated code must stay correct (tests, regression tools, picture).

## 2. Costs found (machine code and per-instruction samples)

* `REXBOX_SAMPLE` samples were matched to a `dumpbin` disassembly of the lifted-code range, and the sampled instructions were classified.
* **v0.0.17**:
  * memory accesses: 50%
  * SSE memory operations: 28%
  * The hottest instructions were `movaps`/`movups` reading stack temporaries such as `[ebp-20h]`.
* **Cause 1 — XMM helpers**:
  * The helpers build a `RecompXmm` union lane by lane in C (`XMM_SCALAR`, `XMM_MUL`, `XMM_SHUFFLE` and others). MSVC turns that into the following sequence:
    1. partial stores (two 8-byte or four 4-byte) to a 16-byte stack temporary,
    2. an immediate 16-byte load from it.
  * That sequence fails store-to-load forwarding.
  * The aligned temporaries also gave every function a stack-alignment prologue and a `/GS` cookie.
* **Cause 2 — memory-base reloads**: `XBOX_PTR` adds the global `g_xbox_mem_offset`, which was reloaded after every volatile guest store because the store may alias it.
* **Cause 3 — TLS registers**: guest registers are TLS memory slots, reloaded after guest stores for the same reason.

## 3. Design

### 3.1 SSE2 intrinsics for the XMM helpers (`perf/xmm-intrinsics`)

* Where the compiler targets SSE2 (MSVC x86's default `/arch:SSE2`, `__SSE2__`, x64), the common helpers are intrinsics. An intrinsic is the instruction itself, so its result is the guest hardware's.
* Helpers that become intrinsics:
  * construction: zero, scalar, scalar double, scalar bits
  * guest memory: 16- and 8-byte loads and stores, as macros so they use the use site's `XBOX_PTR`
  * arithmetic and bitwise: add/sub/mul/div/min/max, and/or/xor/andn
  * EQ/LT/LE/NEQ compares, sqrt, `movmskps`
  * `shufps`, as a macro because its selector must be a constant, which the lifter always emits
  * unpack, `movlhps`/`movhlps`
* **Guest memory access**: one 16- or 8-byte access through a plain pointer (the guest instruction's size) instead of four volatile 32-bit accesses; the compiler may combine or reorder it with other non-volatile accesses.
* **Kept in plain C**:
  * RSQRT/RCP: their hardware approximations differ in bits between CPUs.
  * the variable-predicate compare
  * Defining `RECOMP_XMM_PORTABLE` keeps everything plain C.
* `RecompXmm` keeps its 8-byte alignment, because MSVC x86 cannot pass 16-byte aligned types by value; values cross with unaligned 16-byte moves.
* **Verification**: `test_xmm_intrinsics` builds one driver both ways and compares every helper bit for bit on signed zeros, infinities, NaNs, denormals and ordinary values. The compiler is clang, gcc, or cl on PATH. The test was checked to catch a planted bug (`ANDN` as `AND`).

### 3.2 Reading the memory base once per function (`perf/mem-base-local`)

* Translated chunks define `RECOMP_LOCAL_MEM_BASE`.
* Each translated function copies the base into a local constant on entry (`RECOMP_MEM_BASE_DECL`).
* Inside a chunk, `XBOX_PTR` uses that local. The switch comes after every helper in the header, so the helpers keep the global.
* The base does not move after the memory is mapped.

### 3.3 Not done

* **Guest registers in locals**: they would have to be written back and reloaded around about 37,000 call sites and every return. §4 measured how much it could gain at most.
* **Per-site caches for indirect calls**: cache misses in `recomp_lookup`'s flat table (about 7 MB), about 2% of lifted code.

### 3.4 SSE2 intrinsics are opt-in (follow-up, 2026-10-13)

* **User decision (2026-10-13)**: the SSE2 intrinsics of §3.1 are off by default and turned on only when needed. For now, performance work concentrates on the HLE.
* The fork's header is unchanged. The reJSRF build defines `RECOMP_XMM_PORTABLE`, which selects the lane-wise C (the upstream design).
  * The CMake option `REXBOX_RECOMP_XMM_SSE2` (default `OFF`) sits on `rexbox_recomp`. It targets `rexbox_recomp`, so its name starts with `REXBOX_`, not with a title.
  * When it is `OFF`, `rexbox_recomp` defines `RECOMP_XMM_PORTABLE` as PUBLIC, so every target that includes `recomp_types.h` (`rexbox_recomp`, `rejsrf_recomp`, `rejsrf_game`) takes the same path.
  * `ON` (`cmake -DREXBOX_RECOMP_XMM_SSE2=ON`) is the same as v0.0.18.
* The header's content does not change, so neither the ABI hash check (`src/game/jsrf/CMakeLists.txt`) nor the generated code is affected. Changing the option needs a rebuild, not a regeneration.
* The cost: the default build gives up the XMM intrinsics' gain of §4 (about -12% presenting-thread CPU). `perf/mem-base-local` stays.

## 4. Measurement

The table in the Korean section applies: presenting-thread CPU ms per frame, mean of three alternating runs at the same time of day.

* The XMM intrinsics took the sum of four windows from 53.1 ms to 46.8 ms (-12%).
* The memory-base change alone is within the noise. It stays, because it lets the XMM macros use the use site's local base.
* After XMM, lifted-code samples are 35% guest memory, 21–25% TLS register slots, and 14% SSE memory.
* The leaf functions (no calls) hold register-slot samples worth 7.5% of lifted code. That bounds register localization:
  * leaf functions only: about 2% of total CPU
  * everything: about 7% (about 1 ms per frame)

## 5. Verification

The table above applies: xboxrecomp unit tests and `test_xmm_intrinsics` (cl) pass; regeneration and Debug/Release builds have zero warnings and the regression tools pass; frames 2,800–3,400 match v0.0.15.
