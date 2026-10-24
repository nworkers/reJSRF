# AGENTS.md

이 문서의 작업 규칙과 코딩 규칙은 [reexec/rePIU](https://github.com/reexec/rePIU)의 규칙을 바탕으로 하고, 이 프로젝트의 성격(Xbox XBE의 정적 재컴파일과 디컴파일)과 라이선스(MIT)에 맞게 고쳤다.

The working and coding rules here are based on [reexec/rePIU](https://github.com/reexec/rePIU), adapted to this project's nature (static recompilation and decompilation of an Xbox XBE) and license (MIT).

## 프로젝트 개요

이 프로젝트는 Xbox 게임 Jet Set Radio Future(JSRF)를 에뮬레이터 없이 Win32, Win64, Linux x86, Linux x64, Web, Android에서 네이티브로 실행하는 것을 목표로 한다.

방법은 두 가지를 함께 쓴다.

* **재컴파일(Recompile)**: 원본 XBE의 x86 코드를 기계적으로 C로 옮겨(lift) 호스트 컴파일러로 빌드한다. 이것이 게임 로직의 기본 실행 경로다.
* **디컴파일(Decompile)**: 옮겨진 함수를 사람이 읽을 수 있는 C/C++로 하나씩 대체한다. 대체한 함수는 원래 함수와 같은 동작임을 검증한다.

Xbox 커널, XDK 라이브러리(D3D8, DSOUND, XAPI, XInput), 하드웨어는 High Level Emulation(HLE) 계층으로 대체한다. HLE 계층은 이 게임과 분리해 다른 Xbox 타이틀에서도 재사용할 수 있게 만든다.

## Project Overview

This project aims to run the Xbox game Jet Set Radio Future (JSRF) natively on Win32, Win64, Linux x86, Linux x64, the web and Android, without an emulator.

Two methods are used together.

* **Recompile**: mechanically lift the original XBE's x86 code to C and build it with a host compiler. This is the default execution path of the game logic.
* **Decompile**: replace lifted functions one at a time with human-readable C/C++. Each replacement is verified to behave like the function it replaces.

The Xbox kernel, the XDK libraries (D3D8, DSOUND, XAPI, XInput) and the hardware are replaced by High Level Emulation (HLE) layers. The HLE layers are kept separate from this game so other Xbox titles can reuse them.

---

## 핵심 설계 목표

우선순위:

1. 원본 게임 로직을 보존한다. 재컴파일된 코드가 기준 구현이며, 디컴파일은 그 동작을 바꾸지 않는다.
2. HLE는 API 경계(커널 ordinal, XDK 라이브러리 함수)에서 한다. 하드웨어 레지스터 수준 에뮬레이션(NV2A, MCPX APU LLE)은 API 경계로 해결되지 않을 때만 쓴다.
3. 재사용 가능한 Xbox 런타임(`rexbox`)은 타이틀을 알지 못한다. 타이틀 고유 사항은 타이틀 프로파일과 `src/game/<title>/`에만 둔다.
4. Win32 x86을 먼저 완성하고, 이후 Win64, Linux x86/x64, Web, Android로 넓힌다. 처음부터 멀티플랫폼 공용 구조로 설계한다.
5. 에뮬레이터 소스(xemu, Cxbx-Reloaded)를 통합하지 않는다.
6. 게임 데이터(XBE, 자산, ISO)와 그것에서 기계적으로 생성한 코드는 저장소에 넣지 않는다.

## Primary Design Goals

Priority order:

1. Preserve the original game logic. The recompiled code is the reference implementation, and decompilation does not change its behavior.
2. Do HLE at API boundaries (kernel ordinals, XDK library functions). Use hardware-register emulation (NV2A, MCPX APU LLE) only where an API boundary cannot work.
3. The reusable Xbox runtime (`rexbox`) knows nothing about any title. Title specifics live only in the title profile and `src/game/<title>/`.
4. Finish Win32 x86 first, then widen to Win64, Linux x86/x64, the web and Android. Design shared multiplatform structures from the start.
5. Do not integrate emulator source code (xemu, Cxbx-Reloaded).
6. Never put game data (XBE, assets, ISO) or code mechanically generated from it in the repository.

---

## 참조 문서

아키텍처 변경 전에는 관련 문서를 먼저 읽고 갱신한다.

* `docs/PROJECT_CHARTER.md`
* `ARCHITECTURE.md`
* `docs/XBE_DESIGN.ko.md`
* `docs/XBE_DESIGN.en.md`
* `docs/CODING_STYLE.md`
* `THIRD_PARTY_NOTICES.md`
* `docs/design/`
* `docs/work-orders/`
* `docs/work-logs/`
* `docs/analysis/`
* `docs/kb/`
* `docs/guides/`
* 런타임·HLE·플랫폼에 닿는 작업이면 rexbox의 `third_party/rexbox/ARCHITECTURE.md`와 `third_party/rexbox/AGENTS.md`

## Reference Documents

Read and update the relevant documents before making architectural changes.

* `docs/PROJECT_CHARTER.md`
* `ARCHITECTURE.md`
* `docs/XBE_DESIGN.ko.md`
* `docs/XBE_DESIGN.en.md`
* `docs/CODING_STYLE.md`
* `THIRD_PARTY_NOTICES.md`
* `docs/design/`
* `docs/work-orders/`
* `docs/work-logs/`
* `docs/analysis/`
* `docs/kb/`
* `docs/guides/`
* For work touching the runtime, HLE or platform layer: rexbox's `third_party/rexbox/ARCHITECTURE.md` and `third_party/rexbox/AGENTS.md`

---

## 핵심 원칙

1. 사용자 요구사항을 받으면 바로 구현하지 말고 먼저 설계를 작성하거나 기존 설계를 갱신한다.
2. 설계가 정리되면 구현 계획서를 작성하거나 갱신한 뒤 구현을 시작한다.
3. 코드 변경이 있는 모든 작업은 작업 지시 문서와 작업 로그 문서를 남긴다.
4. 문서는 기본적으로 한국어를 먼저 쓰고 바로 아래에 영어 번역을 추가한다. 이 규칙은 `docs/` 아래 문서와 `README.md`, `ARCHITECTURE.md` 같은 저장소 문서에만 적용되며, 소스 코드 주석에는 적용하지 않는다.
5. 기존 코드, 자산, 문서의 의도를 확인한 뒤 수정하며, 큰 구조 변경은 근거를 문서에 남긴다.
6. 멀티플랫폼 공용 구조를 우선 설계하고, 플랫폼별 세부 사항은 분리한다.

## Core Principles

1. When a user requirement is received, do not implement immediately. First write a design or update the existing design.
2. After the design is clear, write or update the implementation work order before starting implementation.
3. Every task that changes code must leave a work-order document and a work-log document.
4. Documents should be written in Korean first, followed immediately by an English translation. This applies to documents under `docs/` and repository documents such as `README.md` and `ARCHITECTURE.md`; it does not apply to source-code comments.
5. Confirm the intent of existing code, assets, and documents before modifying them. Document the rationale for large structural changes.
6. Design shared multiplatform structures first, and keep platform-specific details separated.

---

## 응답 태도 규칙

* 모든 질문에 대한 답변은 존대말을 사용하고 예의 있는 태도를 유지한다.

## Response Tone Rules

* Use polite Korean speech and maintain a respectful attitude for all answers.

---

## 요구사항 처리 절차

1. 요구사항 접수
2. 관련 코드, 문서, 자산 맥락 확인
3. 설계 문서 작성 또는 갱신
4. 구현 계획서 작성 또는 갱신
5. 사용자 승인 또는 현재 진행 맥락 확인
6. 구현
7. 빌드 또는 범위에 맞는 검증
8. 작업 로그 작성

단, 요구사항이 단순 질문이거나 확인 요청이라면 해당 내용에 바로 응답하고 문서화 및 코드 구현은 하지 않는다.

## Requirement Handling Procedure

1. Receive the requirement.
2. Inspect the relevant code, documentation, and asset context.
3. Write or update the design document.
4. Write or update the implementation work order.
5. Confirm user approval or the current active context.
6. Implement.
7. Run a build or verification appropriate to the scope.
8. Write the work log.

If the requirement is a simple question or confirmation request, answer it directly without documentation or code implementation.

---

## 문서 작성 규칙

* 설계 문서는 `docs/design/` 아래에 둔다.
* 계획 문서는 `docs/work-orders/` 아래에 둔다.
* 작업 결과와 회고는 `docs/work-logs/` 아래에 둔다.
* 사용자가 직접 수행하는 검증·측정·운영 절차는 `docs/guides/` 아래에 둔다. 반복 수행 가능한 절차만 두고, 근거가 되는 작업 로그와 설계를 링크한다.
* 원본 XBE 분석으로 확인한 사실, 가설, 미해결 질문은 `docs/analysis/` 아래에 주제별로 둔다.
* 일반 기술 정의와 배경 지식은 `docs/kb/` 아래에 주제별로 두고, 외부 자료에서 왔으면 원문 링크를 단다.
* 프로젝트의 큰 방향성은 `docs/PROJECT_CHARTER.md`에 반영한다.
* 현재 구현되는 코드의 설계와 구조는 `ARCHITECTURE.md`에 지속적으로 반영한다.
* 원본 XBE 분석으로 확인한 구조와 설계는 `docs/XBE_DESIGN.ko.md`, `docs/XBE_DESIGN.en.md`에 누적 반영한다.
* 규칙이 바뀌면 `AGENTS.md`와 관련 문서를 함께 갱신한다.
* 작업 단위 문서 파일명은 `YYYYMMDD-i###-slug.md` 형식을 사용한다. `###`은 task의 GitHub issue 번호를 세 자리로 맞춘 값이다(예: issue #6 → `20261012-i006-slug.md`).
* Markdown 문서에서 구조, 관계, 흐름, 호출 순서, 상태 전이, 주소 변환을 도식화할 수 있으면 Mermaid를 적극적으로 사용한다.
* 세 개 이상의 구성요소·단계·분기·계층이 있으면 Mermaid 적용 가능성을 검토하고, 내용에 맞는 형식(flowchart, sequenceDiagram, stateDiagram, classDiagram 등)을 고른다.
* 단순 사실 하나나 한 단계 설명에는 도식을 강제하지 않는다.
* Mermaid 도식은 본문과 같은 사실 및 확인 상태를 표현해야 하며, 본문 설명을 완전히 대체하지 않는다.

## Documentation Rules

* Put design documents under `docs/design/`.
* Put work-order documents under `docs/work-orders/`.
* Put work results and retrospectives under `docs/work-logs/`.
* Put procedures the user runs themselves (verification, measurement, operations) under `docs/guides/`. Keep only repeatable procedures there, and link the design and work log they rest on.
* Put findings from the original XBE (confirmed behavior, hypotheses, unresolved questions) under `docs/analysis/`, organized by topic.
* Put generally applicable technical definitions and background under `docs/kb/`, organized by topic and linked to external sources when derived from them.
* Reflect the project's broad direction in `docs/PROJECT_CHARTER.md`.
* Continuously reflect the design and structure of currently implemented code in `ARCHITECTURE.md`.
* Accumulate original XBE analysis findings and design notes in `docs/XBE_DESIGN.ko.md` and `docs/XBE_DESIGN.en.md`.
* When rules change, update `AGENTS.md` and the related documents together.
* Use the `YYYYMMDD-i###-slug.md` filename format for task documents, where `###` is the task's GitHub issue number padded to three digits (for example issue #6 → `20261012-i006-slug.md`).
* Use Mermaid whenever structure, relationships, flows, call sequences, state transitions, or address translation can be visualized.
* Evaluate Mermaid whenever content has three or more components, steps, branches, or layers, and choose a fitting form (flowchart, sequenceDiagram, stateDiagram, classDiagram, ...).
* Do not force diagrams into single-fact or one-step explanations.
* Mermaid diagrams must express the same facts and confirmation status as the prose and must not replace the written explanation.

## 분석 및 지식 기반 유지 규칙

* 바이너리 분석이나 구현 검증으로 새로운 사실이 확인되거나 기존 결론이 바뀌면 같은 작업에서 관련 `docs/analysis/` 문서를 갱신한다.
* 작업 중 프로젝트 이해에 중요한 새로운 용어, 표준, 실행 형식, CPU 동작, XDK API가 등장하면 관련 `docs/kb/` 문서를 작성하거나 갱신한다.
* 외부 자료에서 얻은 기술 지식에는 가능한 한 권위 있는 출처 링크를 가까운 위치에 남긴다.
* `docs/analysis/`는 **확인됨**, **추정**, **미확정**을 구분한다.
* analysis 또는 kb 파일을 추가하거나 이름을 바꾸면 해당 디렉터리의 `README.md` 색인을 같은 작업에서 갱신한다.
* `docs/work-logs/`는 시간순 작업 증거로, `docs/analysis/`와 `docs/kb/`는 주제별 누적 문서로 유지한다.
* 다른 프로젝트(jsrf-recomp, xboxrecomp 등)에서 얻은 주장은 우리 바이너리에서 다시 확인하기 전까지 **추정**으로 적고 출처를 단다.

## Analysis and Knowledge Base Maintenance Rules

* When binary analysis or implementation verification confirms a new fact or changes an earlier conclusion, update the relevant `docs/analysis/` topic in the same task.
* When important new terminology, standards, executable formats, CPU behavior or XDK APIs arise, create or update the relevant `docs/kb/` topic.
* Place authoritative external links near derived technical knowledge.
* Distinguish **confirmed**, **inferred**, and **unresolved** statements in `docs/analysis/`.
* When adding or renaming an analysis or knowledge file, update that directory's `README.md` index in the same task.
* Keep `docs/work-logs/` as chronological evidence and `docs/analysis/` and `docs/kb/` as cumulative topic references.
* Record claims taken from other projects (jsrf-recomp, xboxrecomp, ...) as **inferred**, with their source, until they are re-checked on our binary.

---

## 레이어와 의존 규칙

* 레이어는 아래에서 위로 `platform` → `rexbox`(xbe, runtime, hle) → `game/<title>` → `app` 순서다. 각 레이어는 아래 레이어만 링크한다. CMake 타깃 그래프가 이 규칙을 강제한다.
* `platform`과 `rexbox`는 submodule `third_party/rexbox`([reexec/rexbox](https://github.com/reexec/rexbox), reCT3와 공유)에 있다. 이 저장소에는 `game/<title>`, `app`, 타이틀 도구만 둔다. 런타임·HLE·플랫폼을 고칠 일은 rexbox 저장소에서 그 규칙(rexbox `AGENTS.md`)대로 하고, 이 저장소는 submodule 커밋을 올려 받는다. `third_party/rexbox` 안을 이 저장소에서 직접 고치지 않는다.
* `rexbox`는 타이틀 이름, 타이틀 주소, `rejsrf` 헤더를 쓰지 않는다. 다른 타이틀이 그대로 가져다 쓸 수 있어야 한다. 타이틀이 런타임에 알려야 하는 것은 rexbox의 소비자 계약(rexbox `ARCHITECTURE.md` §2)으로 전한다.
* HLE 모듈은 런타임만 링크하고 다른 HLE 모듈을 링크하지 않는다(rexbox 규칙).
* 타이틀 코드(재컴파일·디컴파일 코드)는 HLE 모듈을 직접 부르지 않는다. HLE는 `HleRegistry`의 바인딩(게스트 주소 → HLE export)을 통해서만 연결된다.
* 플랫폼 계층은 런타임과 타이틀의 타입을 알지 못한다. 기본 값(주소, 크기, 버퍼, 콜백)으로만 호출된다.

## Layer and Dependency Rules

* Layers run bottom to top: `platform` → `rexbox` (xbe, runtime, hle) → `game/<title>` → `app`. Each layer links only the layers below it; the CMake target graph enforces this.
* `platform` and `rexbox` live in the submodule `third_party/rexbox` ([reexec/rexbox](https://github.com/reexec/rexbox), shared with reCT3). This repository holds only `game/<title>`, `app` and the title tools. Changes to the runtime, HLE or platform layer are made in the rexbox repository under its rules (rexbox's `AGENTS.md`), and this repository takes them by moving the submodule commit; never edit inside `third_party/rexbox` from here.
* `rexbox` never names a title, a title's addresses, or `rejsrf` headers. Another title must be able to take it as is. What a title must tell the runtime goes through rexbox's consumer contract (rexbox `ARCHITECTURE.md` §2).
* An HLE module links the runtime only, never another HLE module (a rexbox rule).
* Title code (recompiled and decompiled) never calls an HLE module directly. HLE is reached only through `HleRegistry` bindings (guest address → HLE export).
* The platform layer knows none of the runtime's or the title's types. It is called with plain values (addresses, sizes, buffers, callbacks).

---

## 구현 규칙

* 플랫폼(OS)·CPU 아키텍처 종속 코드는 rexbox의 플랫폼 계층(`third_party/rexbox/src/platform/<플랫폼>/[<아키텍처>/]`)에 둔다. 타이틀·앱·도구 디렉터리에는 플랫폼·아키텍처 하위 디렉터리를 두지 않는다.
* 게스트 주소는 항상 32비트 `GuestAddress`이며 호스트 포인터로 바꿀 때는 `GuestMemory::Translate`를 거친다(based 주소 모델). 32비트 호스트에서도 게스트 주소를 호스트 포인터로 직접 쓰지 않는다.
* 새 기능을 추가할 때는 테스트 전략 또는 최소 검증 절차를 문서에 함께 남긴다.
* 코드 수정이 있는 작업은 영향 범위에 맞는 빌드 검증을 수행하고, 불가능하면 이유를 작업 로그에 남긴다.
* 코딩 스타일 세부 규칙은 `docs/CODING_STYLE.md`에서 관리한다.
* 소스 코드 주석은 영어로만 작성한다. 세부 규칙은 `docs/CODING_STYLE.md`의 주석 언어 항목을 따른다.
* 큰 기능은 하나의 거대 파일에 누적하지 않는다. 독립적으로 이름 붙일 수 있는 하위 시스템은 전용 header/source로 분리하고, 통합 지점에는 orchestration과 adapter 코드만 남긴다.

## Implementation Rules

* Platform (OS) and CPU-architecture specific code belongs in rexbox's platform layer (`third_party/rexbox/src/platform/<platform>/[<arch>/]`). The title, app and tool directories have no platform or architecture subdirectories.
* A guest address is always a 32-bit `GuestAddress`, turned into a host pointer only through `GuestMemory::Translate` (the based address model). Even on a 32-bit host, guest addresses are not used directly as host pointers.
* When adding a feature, document the test strategy or minimum verification procedure.
* For tasks that modify code, run build verification appropriate to the impact. If verification is impossible, record the reason in the work log.
* Maintain detailed coding style rules in `docs/CODING_STYLE.md`.
* Write source-code comments in English only, following the comment-language section of `docs/CODING_STYLE.md`.
* Do not accumulate major features in one monolithic file. Give independently named subsystems their own header/source, leaving only orchestration or adapter code at the integration point.

---

## 게임 데이터와 생성 코드 규칙

* `roms/`, XBE, ISO, 게임 자산은 절대 커밋하지 않는다. 사용자가 자기 덤프를 제공한다.
* 재컴파일 파이프라인이 XBE에서 기계적으로 생성한 C 코드(`gen/`, `src/game/<title>/recomp/generated/`)는 커밋하지 않고 로컬에서 다시 만든다.
* 사람이 작성한 디컴파일 코드(`src/game/<title>/decomp/`)는 커밋한다. 원본 바이트를 그대로 옮긴 표나 자산 덤프는 디컴파일 코드에 넣지 않는다.
* 커밋 전에 `git status`로 게임 데이터가 staging되지 않았는지 확인한다.

## Game Data and Generated Code Rules

* Never commit `roms/`, XBEs, ISOs or game assets. Users supply their own dump.
* C code the recompilation pipeline generates mechanically from the XBE (`gen/`, `src/game/<title>/recomp/generated/`) is never committed; it is rebuilt locally.
* Human-written decompiled code (`src/game/<title>/decomp/`) is committed. Do not paste raw original bytes, tables or asset dumps into decompiled code.
* Before committing, check with `git status` that no game data is staged.

---

## 라이선스 규칙

* 프로젝트 라이선스는 `MIT License`다.
* 서드파티 오픈소스는 MIT, BSD, zlib, ISC, Apache-2.0, Boost 같은 허용적 라이선스만 빌드에 포함한다.
* GPL, LGPL, AGPL 같은 전염성 라이선스의 코드는 컴파일·링크하지 않고 저장소에 복사하지도 않는다. xboxrecomp의 LGPL 파일(`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`, 그것을 include하는 `include/xbox/xboxrecomp.h`)은 어떤 타깃의 소스 목록이나 include 경로에도 넣지 않는다. 우리 타깃은 생성 코드와 그 옆의 MIT 헤더(`gen/<변형>/gen/recomp_types.h`)만 컴파일한다. 이 규칙을 강제하는 CMake 가드는 없다([설계 i117](docs/design/20261024-i117-lifter-pipeline.md)).
* 전염성 라이선스 프로젝트(xemu, Cxbx-Reloaded 등)는 동작을 이해하는 지식 출처로만 참고하고, 코드를 옮기지 않는다.
* 라이선스가 명시되지 않았거나 LLM 기여를 금지한 프로젝트(예: Codeberg `KeybadeBlox/JSRF-Decompilation`)의 코드는 가져오지 않는다. 공개 문서의 사실은 출처를 달아 참고할 수 있다.
* 서드파티 코드를 가져오면 같은 작업에서 `THIRD_PARTY_NOTICES.md`를 갱신한다. 예외가 필요하면 먼저 문서화하고 사용자 확인을 받는다.
* 가져온 서드파티 코드에서 문제(버그, 이식성)를 찾으면 우리 사본만 고치지 않고 업스트림에 기여한다. rexbox의 문제는 rexbox 저장소에서 고친다. 특히 같은 호스트 계층을 쓰는 [reexec/re2DJ](https://github.com/reexec/re2DJ)(BSD-3-Clause)는 필요할 때 참고하고, 얻어온 코드의 수정은 re2DJ에 되돌려 보낸다.

## License Rules

* The project license is the `MIT License`.
* Only permissively licensed third-party code (MIT, BSD, zlib, ISC, Apache-2.0, Boost, ...) is built into the project.
* Copyleft code (GPL, LGPL, AGPL) is neither compiled, linked, nor copied into the repository. xboxrecomp's LGPL files (`src/apu/*`, `src/nv2a/nv2a_core.c`, `src/nv2a/nv2a_regs.h`, `src/nv2a/nv2a_state.h`, and `include/xbox/xboxrecomp.h`, which includes them) never go into any target's source list or include path. Our targets compile only the generated code and the MIT header next to it (`gen/<variant>/gen/recomp_types.h`). No CMake guard enforces this rule ([design i117](docs/design/20261024-i117-lifter-pipeline.md)).
* Copyleft projects (xemu, Cxbx-Reloaded, ...) are consulted only as sources of knowledge about behavior; their code is not carried over.
* Do not take code from projects with no stated license or that forbid LLM contributions (for example Codeberg `KeybadeBlox/JSRF-Decompilation`). Facts from their public documents may be used with attribution.
* When bringing in third-party code, update `THIRD_PARTY_NOTICES.md` in the same task. If an exception is needed, document it and confirm with the user first.
* When a problem (a bug, a portability issue) is found in third-party code we took, contribute the fix upstream instead of fixing only our copy. Problems in rexbox are fixed in the rexbox repository. In particular, consult [reexec/re2DJ](https://github.com/reexec/re2DJ) (BSD-3-Clause), which shares our host layer, when useful, and send fixes to code taken from it back to re2DJ.

---

## 작업 단위 규칙

* task는 GitHub issue로 만든다. task 번호는 GitHub issue 번호를 쓴다.
* 기존 task의 후속 작업(남은 검증, 후속 수정)은 새 issue를 만들지 않고 그 task의 issue에 붙인다.
* 커밋 제목 끝에 관련 issue 번호를 `(#6)`처럼 붙인다.
* 의미 있는 작업마다 하나의 작업 지시 문서를 만든다.
* 작업이 끝나면 대응되는 작업 로그를 남긴다.
* 설계 없이 바로 코드만 추가하지 않는다.
* 초기 구조 작업이라도 디렉터리 목적과 향후 확장 방향을 문서로 남긴다.

## Task Unit Rules

* A task is created as a GitHub issue, and the task number is the GitHub issue number.
* Follow-up work on an existing task (verification left open, a follow-up fix) goes to that task's issue rather than a new one.
* End commit titles with the related issue number, as in `(#6)`.
* Create one work-order document for each meaningful task.
* When the task is complete, leave the corresponding work log.
* Do not add code directly without a design.
* Even for initial structure work, document the directory purpose and future extension direction.

---

## Git 작업 규칙

* 사용자가 작업을 요청하면 먼저 현재 Git 브랜치명을 확인한다.
* 현재 브랜치가 `main`이면 사용자가 요청한 작업 내용을 바탕으로 작업용 브랜치를 새로 만든 뒤 작업한다.
* 현재 브랜치가 `main`이 아니면 새 브랜치를 만들지 않고 그 브랜치에서 이어서 작업한다. 다른 issue의 작업이어도 같다. 커밋마다 제목 끝에 해당 issue 번호를 붙여 구분한다.
* 작업 단위가 하나 끝날 때마다 관련 변경을 Git 커밋으로 남긴다.
* 프로젝트 버전은 저장소 루트의 `VERSION` 파일에서 `major.minor.patch` 형식으로 관리한다.
* 사용자가 머지를 요청하면 `main`에 머지하기 전에 patch 버전을 1 증가시킨다.
* 사용자가 minor 버전 증가를 요청하면 minor 버전을 1 증가시키고 patch 버전은 0으로 리셋한다.
* 사용자가 major 버전 증가를 요청하면 major 버전을 1 증가시키고 minor와 patch 버전은 0으로 리셋한다.
* 사용자가 머지를 요청하면 작업 브랜치를 원격에 push하고 GitHub pull request를 만든 뒤(머지 요청이 이 push와 PR 생성의 승인이다), 그 PR을 squash merge해 `main`에 넣는다. 머지한 뒤 `git pull`로 로컬 `main`을 원격과 같게 갱신한다.
* PR 본문에는 해결하는 issue를 `Closes #6`처럼 적는다. 여러 issue를 해결하면 issue마다 적는다.
* squash merge의 커밋 제목은 작업 브랜치 안의 커밋 제목들을 확인하고, 전체 변경 내용을 잘 표현하는 제목으로 만든다.
* `main`에 머지한 뒤에는 그 머지 커밋에 `VERSION`과 같은 값의 annotated tag를 `vmajor.minor.patch` 형식으로 붙인다.
* tag 메시지에는 해당 버전의 핵심 변경을 한 줄로 남긴다.
* tag는 로컬까지만 만들고 원격 push는 사용자가 직접 수행한다.
* 머지가 완료되면 현재 작업 브랜치를 삭제한다.
* 릴리스 노트(`docs/release-notes/v<version>.md`)에는 해결된 issue 번호와 그 커밋 ID를 함께 적는다.

## Git Workflow Rules

* When the user requests work, first check the current Git branch name.
* If the current branch is `main`, create a task branch based on the requested work before making changes.
* If the current branch is not `main`, keep working on it without creating a new branch, even for a different issue. Each commit title ends with its own issue number.
* Leave a Git commit for the related changes whenever one task unit is complete.
* Manage the project version in the repository-root `VERSION` file using `major.minor.patch`.
* When the user requests a merge, increment the patch version by 1 before merging into `main`.
* When the user requests a minor version bump, increment the minor version by 1 and reset the patch version to 0.
* When the user requests a major version bump, increment the major version by 1 and reset the minor and patch versions to 0.
* When the user requests a merge, push the task branch, open a GitHub pull request from it (the merge request approves this push and the pull request), and squash-merge it into `main`. Afterwards update local `main` with `git pull`.
* Name the issues the pull request resolves in its description, as in `Closes #6`, one per issue.
* For the squash merge's commit title, inspect the branch's commit titles and write one that describes the complete change.
* After merging into `main`, tag that merge commit with an annotated tag matching `VERSION`, in the form `vmajor.minor.patch`.
* Put a one-line summary of the version's key change in the tag message.
* Create tags locally only; the user pushes them to the remote.
* Delete the task branch after the merge is complete.
* Release notes (`docs/release-notes/v<version>.md`) record the resolved issue numbers together with their commit IDs.

---

## 아키텍처 규칙

* 재컴파일된 원본 코드가 항상 기본 실행 경로다. 디컴파일된 함수는 함수 단위로 그 경로를 대체하며, 언제든 재컴파일 버전으로 되돌릴 수 있어야 한다.
* 운영체제, XDK 라이브러리, 하드웨어 인터페이스만 HLE로 대체한다.
* 절대적으로 필요하지 않으면 게임플레이 로직을 새로 설계해 다시 쓰지 않는다. 디컴파일은 재작성이 아니라 같은 동작의 읽을 수 있는 표현이다.
* 모든 하위 시스템은 독립적으로 교체 가능해야 한다.

## Architecture Rules

* The recompiled original code is always the default execution path. Decompiled functions replace it function by function, and each can always be switched back to its recompiled version.
* Replace only the operating system, XDK library and hardware interfaces with HLE.
* Do not redesign and rewrite gameplay logic unless absolutely unavoidable. Decompilation is a readable expression of the same behavior, not a rewrite.
* Every subsystem should be replaceable independently.

---

## 개발 철학

여러 구현이 가능할 때는 원본 실행 파일과의 동작 일치를 보존하면서 리버스 엔지니어링 부담을 최소화하는 방법을 선택한다. 런타임 동작은 생성된 코드를 읽어서 추론하지 말고 실행해서 측정한다.

최적화보다 정확성을 우선한다.

## Development Philosophy

When multiple implementations are possible, choose the one that preserves behavioral parity with the original executable while minimizing reverse engineering effort. Measure runtime behavior by running it rather than inferring it from reading generated code.

Accuracy is preferred over optimization.
