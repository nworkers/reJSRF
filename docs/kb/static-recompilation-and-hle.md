# 정적 재컴파일, 디컴파일, HLE

## 용어

* **정적 재컴파일(static recompilation)**: 실행 전에 원본 기계어 전체를 다른 언어(여기서는 C)로 옮기고 호스트 컴파일러로 빌드하는 방식. 실행 중에 번역하는 JIT/동적 재컴파일과 다르다. 사례: [N64Recomp](https://github.com/N64Recomp/N64Recomp)(MIPS → C), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)(Xbox 360 PowerPC → C++), [xboxrecomp](https://github.com/sp00nznet/xboxrecomp)(원본 Xbox x86 → C).
* **lift**: 기계어 명령 하나하나를 같은 의미의 C 문장으로 옮기는 것. 레지스터는 전역 상태가 되고, 메모리 접근은 게스트 주소를 호스트 주소로 바꾸는 매크로가 된다.
* **디컴파일(decompilation)**: 기계어에서 사람이 읽을 수 있는 소스를 복원하는 것. **매칭 디컴파일**은 원래 컴파일러로 다시 컴파일했을 때 같은 바이트가 나오는 소스를 목표로 한다. reJSRF에서 디컴파일은 lift된 함수를 같은 동작의 읽을 수 있는 코드로 바꾸는 일이며, 바이트 일치는 요구하지 않는다.
* **HLE(High Level Emulation)**: 하드웨어나 OS를 레지스터·명령 수준에서 흉내 내지 않고, 그 위의 API(커널 함수, 라이브러리 함수)를 호스트 구현으로 바꾸는 것. 반대는 **LLE**(하드웨어 수준 에뮬레이션)다.

## x86 원본을 재컴파일할 때의 특징

* 게스트와 호스트가 같은 x86 계열이라도 명령을 그대로 실행하지 않는다. lift한 C는 호스트 아키텍처와 무관하게 컴파일되므로 ARM(Android)이나 wasm에도 쓸 수 있다.
* 어려운 점: 가변 길이 명령과 코드 안의 데이터(점프 테이블) 때문에 함수 경계를 찾기 어렵다. 간접 호출(가상 함수, 함수 포인터)은 실행 중 주소 → 함수 표(`recomp_lookup`)로 해결한다. x87 FPU의 스택과 반올림 모드, 플래그 계산을 정확히 옮겨야 한다. jsrf-recomp는 `frndint`가 x87 반올림 모드를 무시해 컷신 애니메이션이 깨진 사례를 보고했다.
* 생성 코드는 원본 바이트에서 기계적으로 파생되므로 배포하지 않고, 사용자가 자기 덤프로 다시 만든다.

## Xbox에서 HLE 경계가 갈리는 지점

* Xbox 타이틀은 XDK 라이브러리를 정적으로 링크한다. 그래서 PC처럼 DLL import 표를 가로채는 방식으로는 D3D8·DSOUND를 바꿀 수 없다. 함수 식별(시그니처, 심볼 이전)로 라이브러리 함수의 위치를 찾아 바인딩해야 한다. Cxbx-Reloaded의 HLE 데이터베이스도 같은 원리다.
* 커널만은 ordinal import 표(kernel thunk)로 연결되므로 슬롯 단위로 바꿀 수 있다.
* 라이브러리 수준 HLE를 하면 그 라이브러리가 하던 하드웨어 접근(NV2A push buffer, APU 레지스터)이 사라진다. 그 대가로 라이브러리 내부 상태를 게임이 직접 읽는 경우(정적 디바이스 객체의 필드 등)를 따로 맞춰야 한다.

---

# Static recompilation, decompilation and HLE

## Terms

* **Static recompilation**: translating all of the original machine code into another language (C here) ahead of time and building it with a host compiler, unlike JIT/dynamic recompilation, which translates while running. Examples: [N64Recomp](https://github.com/N64Recomp/N64Recomp) (MIPS → C), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) (Xbox 360 PowerPC → C++), [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) (original Xbox x86 → C).
* **Lift**: turning each machine instruction into a C statement with the same meaning. Registers become global state; memory accesses become macros that map a guest address to a host address.
* **Decompilation**: recovering human-readable source from machine code. **Matching decompilation** aims for source that recompiles with the original compiler to identical bytes. In reJSRF, decompiling means replacing a lifted function with readable code of the same behavior; byte identity is not required.
* **HLE (High Level Emulation)**: replacing the APIs above the hardware or OS (kernel functions, library functions) with host implementations, instead of imitating the hardware at the register or instruction level. The opposite is **LLE** (hardware-level emulation).

## Recompiling an x86 original

* Even though guest and host are both x86-family, the instructions are not run as they are. The lifted C compiles independently of the host architecture, so it serves ARM (Android) and wasm too.
* Difficulties: variable-length instructions and data inside code (jump tables) make function boundaries hard to find. Indirect calls (virtual functions, function pointers) resolve at run time through an address → function table (`recomp_lookup`). The x87 FPU's stack, rounding mode and flag computation must be carried over exactly; jsrf-recomp reported cutscene animation breaking because `frndint` ignored the x87 rounding mode.
* Generated code is derived mechanically from the original bytes, so it is not distributed; users rebuild it from their own dump.

## Where the HLE boundary falls on the Xbox

* Xbox titles link the XDK libraries statically, so D3D8 and DSOUND cannot be swapped by hooking a DLL import table as on the PC. Library functions must be located by function identification (signatures, symbol transfer) and bound. Cxbx-Reloaded's HLE database works on the same principle.
* Only the kernel is linked through an ordinal import table (the kernel thunks), so it can be replaced slot by slot.
* HLE at the library level removes the hardware access the library performed (NV2A push buffers, APU registers). In exchange, any place where the game reads the library's internal state directly (fields of a static device object, for example) must be matched separately.
