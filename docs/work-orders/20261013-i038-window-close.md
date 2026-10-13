# 작업 지시: 창을 닫으면 프로세스를 끝낸다 (issue #38)

설계: [20261013-i038](../design/20261013-i038-window-close.md)

1. `runtime::SetExitHandler`/`RequestExit`(`environment.h`).
2. D3D8 HLE의 이벤트 처리를 `PumpEvents()`로 모으고 닫기 요청이면 `RequestExit`.
3. 앱 `main` 시작에서 처리기 등록(출력 비움, `std::_Exit`).
4. 창 모드 실행에 `WM_CLOSE`를 보내 확인, Debug/Release 빌드, 회귀 도구.
5. 문서: `ARCHITECTURE.md`(표시 계약), 가이드(닫기와 그 한계), 작업 로그.

---

# Work order: closing the window ends the process (issue #38)

Design: [20261013-i038](../design/20261013-i038-window-close.md)

(1) `runtime::SetExitHandler`/`RequestExit` (`environment.h`). (2) The D3D8 HLE's event handling goes through `PumpEvents()`, which calls `RequestExit` on a close request. (3) The app installs the handler at the start of `main` (flush the output, `std::_Exit`). (4) Check by posting `WM_CLOSE` to a windowed run; build Debug and Release; run the regression tools. (5) Documents: `ARCHITECTURE.md` (display contract), the guide (closing and its limit), the work log.
