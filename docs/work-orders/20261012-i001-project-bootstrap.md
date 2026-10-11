# 작업 지시: 프로젝트 부트스트랩 (issue #1)

설계: [20261012-i001](../design/20261012-i001-project-bootstrap.md)

1. 저장소 초기화: `main`에 MIT `LICENSE`, `.gitignore`(게임 데이터·생성 코드·빌드 제외), `.gitattributes`. 작업 브랜치 `i001-project-bootstrap`.
2. 규칙 문서: rePIU를 바탕으로 `AGENTS.md`, `CLAUDE.md`, `docs/CODING_STYLE.md`를 만들고 프로젝트 성격(재컴파일·디컴파일, 레이어, 게임 데이터, MIT·copyleft 규칙)에 맞게 고친다.
3. 외부 참조 조사: xboxrecomp, jsrf-recomp, 디컴파일 프로젝트, 에뮬레이터의 내용과 라이선스. xboxrecomp를 submodule로 고정한다.
4. 코드 골격(Win32 우선): `rexbox_platform`(가상 메모리), `rexbox_xbe`(XBE 파서), `rexbox_runtime`(GuestMemory, XBE 배치, HleRegistry), HLE 모듈 5개, `rejsrf_game`(프로파일), `rejsrf`(앱), `rexbox_xbe_info`(도구). CMake 타깃 그래프로 레이어 의존을 강제하고 LGPL 가드를 둔다.
5. XBE 분석: 두 XBE를 읽어 빌드 변형, 섹션, XDK 라이브러리, 커널 import를 `docs/analysis/`에 기록하고 1차 타깃을 정한다.
6. 문서: `README.md`, `ARCHITECTURE.md`, `docs/PROJECT_CHARTER.md`, `docs/XBE_DESIGN.*`, `docs/kb/`, `docs/guides/`, `THIRD_PARTY_NOTICES.md`, `CREDITS.md`.
7. CI: Win32 x86 Debug/Release 빌드.
8. 검증: 설계 §10.

완료 기준: Win32에서 경고 없이 빌드되고, 앱이 사용자 XBE를 식별·배치하며, 다음 task(재컴파일 파이프라인 연결)를 시작할 수 있는 규칙과 구조가 문서로 남아 있다.

---

# Work order: project bootstrap (issue #1)

Design: [20261012-i001](../design/20261012-i001-project-bootstrap.md)

(1) Initialize the repository: MIT `LICENSE`, `.gitignore` (excluding game data, generated code and builds) and `.gitattributes` on `main`; task branch `i001-project-bootstrap`. (2) Rule documents: `AGENTS.md`, `CLAUDE.md` and `docs/CODING_STYLE.md` based on rePIU, adapted to this project (recompilation and decompilation, layers, game data, MIT and copyleft rules). (3) Survey external references (xboxrecomp, jsrf-recomp, decompilation projects, emulators) and their licenses; pin xboxrecomp as a submodule. (4) Code skeleton, Win32 first: `rexbox_platform` (virtual memory), `rexbox_xbe` (XBE parser), `rexbox_runtime` (GuestMemory, XBE placement, HleRegistry), five HLE modules, `rejsrf_game` (profile), `rejsrf` (app), `rexbox_xbe_info` (tool); the CMake target graph enforces the layer dependencies, with an LGPL guard. (5) XBE analysis: read both XBEs, record build variants, sections, XDK libraries and kernel imports in `docs/analysis/`, and choose the primary target. (6) Documents: `README.md`, `ARCHITECTURE.md`, `docs/PROJECT_CHARTER.md`, `docs/XBE_DESIGN.*`, `docs/kb/`, `docs/guides/`, `THIRD_PARTY_NOTICES.md`, `CREDITS.md`. (7) CI: Win32 x86 Debug/Release builds. (8) Verify: design §10.

Done when it builds on Win32 without warnings, the app identifies and places the user's XBE, and the rules and structure needed to start the next task (connecting the recompilation pipeline) are documented.
