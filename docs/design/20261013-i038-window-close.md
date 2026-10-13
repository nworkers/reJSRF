# 설계: 창을 닫으면 프로세스를 끝낸다 (issue #38)

선행: [i007 표시 계층](20261012-i007-d3d8-hle-first-batch.md), [i031 창 조작](20261013-i031-window-controls.md)

## 1. 요구사항과 원인

* 창의 X 버튼(또는 Alt+F4)으로 앱이 끝나지 않는다. 가이드는 창을 닫으면 끝난다고 적고 있었다.
* **확인됨**(코드):
  * SDL 백엔드는 `SDL_EVENT_WINDOW_CLOSE_REQUESTED`/`SDL_EVENT_QUIT`을 닫기 요청으로 알린다.
  * D3D8 HLE는 present 때 그것을 `close_requested`에 적고 로그를 남기지만, 이 플래그를 읽는 코드가 없다.
  * 디바이스 생성 때의 이벤트 처리는 닫기 결과를 버렸다.
  * `main`은 끝나지 않는 게스트 스레드를 `WaitForThreads()`에서 기다린다.

## 2. 설계

* 런타임에 종료 요청을 둔다: `runtime::SetExitHandler(handler)`, `runtime::RequestExit(code, reason)`. HLE 모듈은 프로세스를 직접 끝내지 않고 요청만 한다. 처리기가 없으면(도구) 로그만 남긴다.
* 앱은 `main` 시작에서 처리기를 등록한다. 처리기는 이유를 출력하고 stdout/stderr를 비운 뒤 `std::_Exit(code)`로 끝낸다. watchdog과 같은 방식이고, 게스트 스레드는 프로세스와 함께 끝난다. 창을 닫은 종료 코드는 0이다.
* D3D8 HLE는 디바이스 생성과 present 두 곳의 이벤트 처리를 `PumpEvents()` 하나로 모으고, 닫기 요청이면 `RequestExit(0, "the window was closed")`를 부른다.
* **한계**: 창은 디바이스를 만든 게스트 스레드의 것이라 이벤트는 그 스레드에서만 처리한다(Win32 메시지 큐는 스레드별, SDL도 같은 스레드를 요구한다). 게임이 화면을 넘기지 않는 동안에는 닫기가 다음 present까지 미뤄진다. 지금 부팅에는 present가 오래 끊기는 구간이 없다(프레임 로그). 필요해지면 표시 서비스 스레드로 옮긴다(i007 §2.5).

## 3. 검증

| 검증 | 기대 |
|---|---|
| 창 모드 실행 후 창에 `WM_CLOSE`(X 버튼과 같은 메시지) | 10초 안에 종료, `[app] the window was closed; exiting` |
| Debug/Release, 회귀 도구 | 경고 0, 회귀 없음 |

---

# Design: closing the window ends the process (issue #38)

Prerequisites: [i007 display layer](20261012-i007-d3d8-hle-first-batch.md), [i031 window controls](20261013-i031-window-controls.md)

## 1. Requirements and cause

* The window's X button (or Alt+F4) does not end the app, while the guide said closing the window does.
* **Confirmed** (code):
  * The SDL backend reports `SDL_EVENT_WINDOW_CLOSE_REQUESTED`/`SDL_EVENT_QUIT` as a close request.
  * The D3D8 HLE records it in `close_requested` at a present and logs it, but no code reads that flag.
  * Event handling at device creation dropped the close result.
  * `main` waits in `WaitForThreads()` for guest threads that never end.

## 2. Design

* The runtime gains an exit request, `runtime::SetExitHandler(handler)` and `runtime::RequestExit(code, reason)`. HLE modules ask rather than end the process; without a handler (tools) the request is only logged.
* The app installs the handler at the start of `main`. It prints the reason, flushes stdout and stderr and ends with `std::_Exit(code)`, as the watchdog does; the guest threads end with the process. Closing the window exits with 0.
* The D3D8 HLE handles events at device creation and at presents through one `PumpEvents()`, which calls `RequestExit(0, "the window was closed")` on a close request.
* **Limit**: the window belongs to the guest thread that created the device, so its events are handled on that thread only (Win32 message queues are per thread, and SDL wants the same thread). While the game presents nothing, a close waits for the next present. The current boot has no long gaps between presents (frame log). If one appears, the display moves to a service thread (i007 §2.5).

## 3. Verification

The table above applies: posting `WM_CLOSE` (the X button's message) to the window of a windowed run ends the process within 10 s with `[app] the window was closed; exiting`; Debug and Release with zero warnings and no regressions in the tools.
