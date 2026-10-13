# lift되지 않은 명령

lifter가 번역하지 못한 명령은 `RECOMP_UNIMPL(text, va)`가 된다. 실행 중 처음 만나면 `[recomp] untranslated instruction at <va>: <text>`를 출력한다. 정적인 목록은 재생성할 때 `gen/jsrf_us/recomp-summary/summary.json`의 `unimplemented`에 쌓인다.

## 1. 남은 종류 (i048 이후)

* **확인됨**(summary): 34종, 약 160곳이다.
  * 포트 I/O: `out` 25, `in` 17, `outsb` 8, `insd` 6, `outsd` 3, `insb` 2
  * BCD·세그먼트·보호 모드: `arpl` 9, `daa` 8, `iretd` 8, `bound` 7, `aaa`/`das`/`aas`/`aad`/`aam`, `lcall`/`ljmp`, `les`/`lds`, `ltr`/`str`/`sldt`/`lldt`, `salc`, `into`
  * 그 밖: `hlt` 7, `sti` 7, `enter` 6, `cli` 4, `popfd` 4, `int1`/`int`, `pushfd`, `wbinvd`
* **추정**: 대부분은 데이터를 코드로 읽은 것이다. 32비트 Xbox 게임 코드에서 BCD·세그먼트 명령은 거의 나오지 않는다. 포트 I/O와 `cli`/`sti`/`wbinvd`는 XDK 라이브러리의 하드웨어 접근일 가능성이 크다. 그런 함수가 HLE로 대체되었다면 실행되지 않는다.
* **미확정**: 위치마다 어느 함수에 속하는지, 그 함수가 HLE로 대체되었는지, 도달할 수 있는지는 아직 분류하지 않았다.
* **확인됨**(실행): 오프닝 데모(프레임 3,000까지)에서 `untranslated instruction` 경고는 0건이다(v0.0.19 + i048).

## 2. MXCSR 검사 (`sub_0015F0F0`)

* **확인됨**(lift 코드): `sub_0015F0F0`(cdecl, 인자 4개)은 다음을 한다.
  1. `stmxcsr [0x265150]`로 MXCSR을 저장한다.
  2. RC 비트(13~14)를 검사한다.
  3. RC가 nearest가 아니면 `0x8000FFFF`를 돌려준다.
  * `0x0015F3A7`과 `0x0015F878`에서 부르고, 데모에서 매 프레임 실행된다.
* **미확정**: 이 함수가 무엇인지. XDK 심볼 DB에는 없다.
* **확인됨**(정적): 게임에는 `ldmxcsr`가 없다. 그래서 게스트 MXCSR은 리셋 값 0x1F80 그대로다. i048부터 `stmxcsr`는 그 값을 저장한다.

## 3. SSE 정수 변환

* **확인됨**(lift 코드): `cvtss2si` 6곳(`sub_0014C690`, `sub_0014CBD0` 3곳, `sub_0014CCD0`, `sub_00154C10`)과 `cvttss2si` 4곳(`sub_0014C6A0`, `sub_0015F6B0`, `sub_00192A80`, `sub_001A0F3D`)이 있다.
* i048 전에는 `cvtss2si`가 버림으로 번역되었다. 지금은 MXCSR RC(nearest-even)로 반올림한다.
* **확인됨**(덤프): 고친 뒤에도 로고·데모 프레임(300~3,000, 8장)은 바이트 단위로 같았다.
* **추정**: 데모 구간에서는 이 변환들에 들어오는 값의 소수부가 결과 화면에 영향을 주지 않았다. `sub_00154C10`은 데모 샘플에 자주 나오는 함수라 실행은 된다.

---

# Unlifted instructions

An instruction the lifter cannot translate becomes `RECOMP_UNIMPL(text, va)`, which prints `[recomp] untranslated instruction at <va>: <text>` the first time it runs. Regeneration collects the static list in `unimplemented` of `gen/jsrf_us/recomp-summary/summary.json`.

## 1. Remaining kinds (after i048)

* **Confirmed** (summary): 34 kinds at about 160 sites.
  * port I/O: `out` 25, `in` 17, `outsb` 8, `insd` 6, `outsd` 3, `insb` 2
  * BCD, segment and protected-mode instructions: `arpl` 9, `daa` 8, `iretd` 8, `bound` 7, `aaa`/`das`/`aas`/`aad`/`aam`, `lcall`/`ljmp`, `les`/`lds`, `ltr`/`str`/`sldt`/`lldt`, `salc`, `into`
  * others: `hlt` 7, `sti` 7, `enter` 6, `cli` 4, `popfd` 4, `int1`/`int`, `pushfd`, `wbinvd`
* **Inferred**: most are data read as code; BCD and segment instructions hardly appear in 32-bit Xbox title code. Port I/O and `cli`/`sti`/`wbinvd` are likely XDK library hardware access, which never runs where those functions are replaced by HLE.
* **Unresolved**: which function each site belongs to, whether that function is replaced by HLE, and whether it is reachable, are not classified yet.
* **Confirmed** (run): the opening demo (up to frame 3,000) prints no `untranslated instruction` warning (v0.0.19 plus i048).

## 2. The MXCSR check (`sub_0015F0F0`)

* **Confirmed** (lifted code): `sub_0015F0F0` (cdecl, four arguments) does the following.
  1. `stmxcsr [0x265150]` stores MXCSR.
  2. It tests the RC bits (13–14).
  3. Unless RC is nearest, it returns `0x8000FFFF`.
  * It is called from `0x0015F3A7` and `0x0015F878` and runs every frame of the demo.
* **Unresolved**: what the function is; it is not in the XDK symbol database.
* **Confirmed** (static): the title has no `ldmxcsr`, so the guest MXCSR stays at its reset value 0x1F80. From i048 on, `stmxcsr` stores that value.

## 3. SSE integer conversions

* **Confirmed** (lifted code): there are 6 `cvtss2si` sites (`sub_0014C690`, three in `sub_0014CBD0`, `sub_0014CCD0`, `sub_00154C10`) and 4 `cvttss2si` sites (`sub_0014C6A0`, `sub_0015F6B0`, `sub_00192A80`, `sub_001A0F3D`).
* Before i048, `cvtss2si` was translated as truncation; it now rounds by MXCSR RC (nearest-even).
* **Confirmed** (dumps): after the fix, logo and demo frames (300–3,000, eight of them) were byte-identical.
* **Inferred**: in the demo, the fractions these conversions see did not reach the picture. `sub_00154C10` appears often in demo samples, so the code does run.
