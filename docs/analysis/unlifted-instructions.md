# lift되지 않은 명령

lifter가 번역하지 못한 명령은 `RECOMP_UNIMPL(text, va)`가 된다. 실행 중 처음 만나면 `[recomp] untranslated instruction at <va>: <text>`를 출력한다. 정적인 목록은 재생성할 때 `gen/jsrf_us/recomp-summary/summary.json`의 `unimplemented`에 쌓인다.

## 1. 남은 종류 (i048 이후)

* **확인됨**(summary): 34종, 156곳이다.
  * 포트 I/O: `out` 25, `in` 17, `outsb` 8, `insd` 6, `outsd` 3, `insb` 2
  * BCD·세그먼트·보호 모드: `arpl` 9, `daa` 8, `iretd` 8, `bound` 7, `aaa`/`das`/`aas`/`aad`/`aam`, `lcall`/`ljmp`, `les`/`lds`, `ltr`/`str`/`sldt`/`lldt`, `salc`, `into`
  * 그 밖: `hlt` 7, `sti` 7, `enter` 6, `cli` 4, `popfd` 4, `int1`/`int`, `pushfd`, `wbinvd`
* **확인됨**(정적 분류, i048 후속): 지금 구성에서 실행되는 위치는 0곳이다. 156곳은 다음 넷으로 나뉜다.

| 분류 | 위치 수 | 근거 |
|---|---|---|
| 함수 안에 박힌 switch 표 | 147 | `jmp dword ptr [r*4 + T]`의 주소 표나 `movzx r, byte ptr [r + T]`의 인덱스 표 바로 뒤에 있다. 표 시작과 그 위치 사이에 실제 분기 도착점이 없다. 예: CRT `memcpy`(`sub_0017CEC0`) 표 0x17CF9C의 `iretd` 6곳, `sub_00134B30` 표 0x134D40의 `out` 2곳 |
| DSOUND 전역 객체 | 4 | 0x1A7788(`popfd`, `hlt`, `aaa`)은 `mov ecx, 0x1a7788`로 `this`에 넣는 객체 주소다 |
| 문자열 | 2 | 0x1C3F18(`arpl`)은 `"\Device\MU_0"`이다. `MU_Init`이 `RtlInitAnsiString`에 넘긴다 |
| D3D 라이브러리의 실제 하드웨어 접근 | 3 | 아래 표 |

| 위치 | 명령 | 함수 | 실행되지 않는 이유 |
|---|---|---|---|
| 0x18E504 | `in al, dx` (포트 0x80C0, bit 5) | `sub_0018E500` | 부르는 쪽(`sub_00193C50` 등)의 주소가 HLE로 대체된 `CMiniport_InitHardware` 안에서만 쓰인다 |
| 0x19243A | `wbinvd` | `sub_00192090` | HLE로 대체된 `Direct3D_CreateDevice`만 부른다 |
| 0x194C09 | `out dx, al` | `CMiniport_InitHardware` 본문 | 함수 자체가 HLE로 대체되었다 |

* **방법**: 정적으로만 분류했고, 게임을 플레이하지 않았다.
  1. 진입점, 코드 즉시값으로 주소가 나오는 함수 312개, 시드 함수에서 출발해 직접 호출과 분기를 따라간다. HLE로 대체된 함수의 lift 본문은 따라가지 않는다. 그 결과 도달 가능한 함수는 2,126개다.
  2. 데이터 섹션(vtable, `.data`의 함수 포인터 배열)에만 주소가 있는 함수는 1에서 빠진다. 그래서 위치가 있는 67개 함수의 주소를 XBE 이미지 전체에서 32비트 값으로 찾았다. 28개가 나왔고, 대부분 진짜 함수(가상 함수 등)였다. 하지만 그 안의 위치는 모두 그 함수의 switch 표 안에 있었다.
  3. 각 위치가 표 안인지, 표 시작과 위치 사이에 실제 분기 도착점이 있는지 확인했다. 도착점이 걸린 2곳은 겹쳐 읽은 쓰레기 명령(0x2CC44 `jmp`)이거나 XGRPH의 바이트 표(0x1BC778)였다.
* **주의**:
  * `REXBOX_HLE_FALLBACK=lifted`(진단용)로 미구현 XDK 함수를 원본으로 돌리면 D3D의 하드웨어 접근 3곳에 닿을 수 있다.
  * D3D 함수를 HLE에서 lift 원본으로 되돌리면 다시 분류해야 한다.
  * 재생성할 때마다 summary 목록이 바뀔 수 있다.

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

* **Confirmed** (summary): 34 kinds at 156 sites.
  * port I/O: `out` 25, `in` 17, `outsb` 8, `insd` 6, `outsd` 3, `insb` 2
  * BCD, segment and protected-mode instructions: `arpl` 9, `daa` 8, `iretd` 8, `bound` 7, `aaa`/`das`/`aas`/`aad`/`aam`, `lcall`/`ljmp`, `les`/`lds`, `ltr`/`str`/`sldt`/`lldt`, `salc`, `into`
  * others: `hlt` 7, `sti` 7, `enter` 6, `cli` 4, `popfd` 4, `int1`/`int`, `pushfd`, `wbinvd`
* **Confirmed** (static classification, i048 follow-up): no site runs in the current configuration. The 156 sites fall into four groups.
  * **147, switch tables embedded in functions**: each site lies right after the address table of a `jmp dword ptr [r*4 + T]` or the index table of a `movzx r, byte ptr [r + T]`, with no real branch target between the table and the site. Examples: six `iretd` in the CRT `memcpy` (`sub_0017CEC0`) table at 0x17CF9C, and two `out` in the `sub_00134B30` table at 0x134D40.
  * **4, a DSOUND global object**: 0x1A7788 (`popfd`, `hlt`, `aaa`) is an object address put in `this` by `mov ecx, 0x1a7788`.
  * **2, a string**: 0x1C3F18 (`arpl`) is `"\Device\MU_0"`, passed by `MU_Init` to `RtlInitAnsiString`.
  * **3, real hardware access in the D3D library**, none of it run:
    * 0x18E504 `in al, dx` (port 0x80C0, bit 5) in `sub_0018E500`: its callers (`sub_00193C50` and others) have their address used only inside `CMiniport_InitHardware`, which is replaced by HLE.
    * 0x19243A `wbinvd` in `sub_00192090`: called only by `Direct3D_CreateDevice`, which is replaced by HLE.
    * 0x194C09 `out dx, al`: in the body of `CMiniport_InitHardware` itself, replaced by HLE.
* **Method**: static only; the game was not played.
  1. Follow direct calls and branches from the entry point, the 312 functions whose address appears as a code immediate, and the seed functions, without entering the lifted bodies of HLE-replaced functions. That reaches 2,126 functions.
  2. Functions whose address sits only in data (vtables, arrays of function pointers in `.data`) are missed by step 1. So the addresses of the 67 functions holding sites were searched for as 32-bit values in the whole XBE image. 28 turned up, mostly real functions (virtual methods and the like), but every site in them lies inside that function's switch table.
  3. For each site, check that it lies in a table with no real branch target between the table and the site. The two sites with one were overlapping garbage decodes (the `jmp` at 0x2CC44) or the XGRPH byte table at 0x1BC778.
* **Caveats**:
  * With `REXBOX_HLE_FALLBACK=lifted` (a diagnostic), unimplemented XDK functions run as originals and may reach the three D3D hardware accesses.
  * Moving a D3D function from HLE back to its lifted original means classifying again.
  * Each regeneration may change the summary list.

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
