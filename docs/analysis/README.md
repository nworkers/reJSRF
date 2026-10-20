# reJSRF 바이너리 분석 색인

이 디렉터리는 원본 JSRF XBE와 reJSRF 실행 결과에서 직접 확인한 프로젝트 고유 사실을 주제별로 누적한다. 시간순 작업 증거는 `docs/work-logs/`에 남기고, 여기에는 현재 유효한 결론을 통합한다.

표기 기준:

* **확인됨**: 정적 분석, 바이트 덤프, 반복 실행 또는 코드로 직접 검증
* **추정**: 관측·정황·외부 프로젝트의 보고와 일치하지만 우리 바이너리에서 더 확인해야 함
* **미확정**: 다음 분석에서 검증해야 할 가설 또는 blocker

```mermaid
flowchart LR
    XBE["XBE overview"] --> SECT["Sections & XDK libraries"]
    XBE --> KIMP["Kernel imports"]
    SECT --> FID["Function identification (xdk-symbols)"]
    KIMP --> KHLE["Kernel HLE (boot-frontier)"]
    FID --> XHLE["XDK HLE bindings"]
    XHLE --> D3D["D3D8 adapter/mode/caps answers (d3d8-adapter)"]
    XHLE --> DSND["DSOUND calls and semantics (dsound)"]
```

## 문서

* [JSRF XBE 개요: 빌드 변형, 섹션, XDK 라이브러리, 커널 import, 디스크 자산](jsrf-xbe-overview.md)
* [부팅 frontier: 재컴파일 코드가 어디까지 실행되는가](boot-frontier.md)
* [XDK 심볼 식별: 라이브러리별 함수, jsrf-recomp 대조, 부팅 중 D3D8 호출 순서](xdk-symbols.md)
* [D3D8 어댑터·모드·Caps 질의: 원본 라이브러리의 답, 호출 규약, 게임이 요청한 디바이스](d3d8-adapter.md)
* [DSOUND: 게임이 부르는 함수, 원본 의미, 효과 이미지, 부팅에서 본 호출](dsound.md)
* [프레임 시간: 오프닝 3D 데모의 한 프레임 구성, CPU 작업의 내역, 측정 시 주의점](frame-time.md)
* [lift되지 않은 명령: 남은 종류, 실제로 실행되는 것, MXCSR 검사](unlifted-instructions.md)
* [미해결 스텁: 실행 중 보고, 되찾은 함수, 남은 스텁의 분류](unresolved-stubs.md)
* [CRI 미들웨어: ADXM 스레드, SVM 잠금, 단일 코어 우선순위에 기댄 상호 배제](cri-middleware.md)
* [캐릭터 윤곽선: 윤곽선 정점 셰이더, 색과 두께를 정하는 상수, 데모에서 본 값](outlines.md)
* [함수 발견: 디스어셈블러가 놓치는 vtable 메서드 모양, 바이트 인덱스 표, 버전별 항목 수](function-discovery.md)
* [언어: US판·일본판의 언어 인덱스, 언어별 자산, 두 빌드의 코드 차이](language.md)

---

# reJSRF Binary Analysis Index

This directory accumulates project-specific facts confirmed directly from the original JSRF XBE and from running reJSRF, by topic. Chronological evidence stays in `docs/work-logs/`; current conclusions are consolidated here.

Labels:

* **Confirmed**: verified directly by static analysis, byte dumps, repeated runs or code
* **Inferred**: consistent with observation, circumstance or another project's report, but still to be checked on our binary
* **Unresolved**: a hypothesis or blocker for the next analysis

## Documents

* [JSRF XBE overview: build variants, sections, XDK libraries, kernel imports, disc assets](jsrf-xbe-overview.md)
* [Boot frontier: how far the recompiled code runs](boot-frontier.md)
* [XDK symbol identification: functions per library, comparison with jsrf-recomp, D3D8 call order during boot](xdk-symbols.md)
* [D3D8 adapter, mode and caps queries: the original library's answers, calling conventions, the device the game asks for](d3d8-adapter.md)
* [DSOUND: the functions the game calls, their original meaning, the effects image, calls seen during boot](dsound.md)
* [Frame time: what a frame of the opening 3D demo is made of, the CPU work, what to watch when measuring](frame-time.md)
* [Unlifted instructions: the remaining kinds, the ones that run, the MXCSR check](unlifted-instructions.md)
* [Unresolved stubs: reporting at run time, recovered functions, the remaining stubs](unresolved-stubs.md)
* [CRI middleware: ADXM threads, the SVM lock, exclusion that relies on single-core priorities](cri-middleware.md)
* [Character outlines: the outline vertex shaders, the constants that set color and thickness, values seen in the demo](outlines.md)
* [Function discovery: the vtable-method shapes the disassembler misses, byte index tables, entries per version](function-discovery.md)
* [Language: the language index of the US and Japanese builds, per-language assets, the code differences between the builds](language.md)
