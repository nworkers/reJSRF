# Issue #38 작업 로그 — 창을 닫으면 프로세스를 끝낸다

작업 지시: [20261013-i038](../work-orders/20261013-i038-window-close.md) · 설계: [20261013-i038](../design/20261013-i038-window-close.md)

## 요약

창의 X 버튼이나 Alt+F4로 앱이 끝나지 않던 것을 고쳤다. D3D8 HLE는 닫기 요청을 받아 플래그에 적고 로그만 남겼고, 그 플래그를 읽는 코드가 없었다. `main`은 끝나지 않는 게스트 스레드를 기다리고 있었다. 이제 닫기 요청은 런타임 종료 요청(`RequestExit`)이 되고, 앱이 등록한 처리기가 출력을 비운 뒤 프로세스를 끝낸다.

## 과정

1. 사용자 질문에서 원인을 코드로 확인했다(설계 §1). 디바이스 생성 때의 이벤트 처리가 닫기 결과를 버리는 것도 함께 찾았다.
2. `environment.h`에 `SetExitHandler`/`RequestExit`를 더했다. D3D8 HLE의 두 이벤트 처리 지점을 `PumpEvents()`로 모았다. `main`은 시작할 때 처리기를 등록한다(게스트 스레드보다 먼저).
3. 확인: Release를 창 모드로 실행하고 25초 뒤 창 핸들에 `WM_CLOSE`(X 버튼이 보내는 메시지)를 `PostMessage`했다. 프로세스가 10초 안에 끝났고 로그에 `[d3d8] window close requested`와 `[app] the window was closed; exiting`이 남았다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| 창 모드 + `WM_CLOSE` | 10초 안에 종료 |
| `rexbox_kernel_probe` / `rexbox_d3d8_probe` / `rexbox_dsound_probe` | 56/56, 81/81, 48/48 |
| `rejsrf_d3d8_check` | 14,018/0 |

## 남은 일

* 창 이벤트는 게임이 화면을 넘길 때만 처리된다. present가 오래 끊기는 구간이 생기면 표시 서비스 스레드로 옮긴다(설계 §2).

---

# Issue #38 work log — closing the window ends the process

Work order: [20261013-i038](../work-orders/20261013-i038-window-close.md) · Design: [20261013-i038](../design/20261013-i038-window-close.md)

## Summary

Closing the window with its X button or Alt+F4 now ends the app. The D3D8 HLE used to record the close request in a flag and log it, but no code read that flag, and `main` kept waiting for guest threads that never end. The close request is now a runtime exit request (`RequestExit`), and the handler the app installs flushes the output and ends the process.

## Process

1. The user's question led to the cause in the code (design §1), along with event handling at device creation that dropped the close result.
2. `environment.h` gained `SetExitHandler`/`RequestExit`; the D3D8 HLE's two event-handling points became one `PumpEvents()`; `main` installs the handler at its start, before any guest thread.
3. Check: a windowed Release run got `WM_CLOSE` (the message the X button sends) through `PostMessage` to its window after 25 s. The process ended within 10 s, logging `[d3d8] window close requested` and `[app] the window was closed; exiting`.

## Verification

The table in the Korean section applies: Debug and Release builds with zero warnings on our targets; the windowed run with `WM_CLOSE` ends within 10 s; kernel probe 56/56, D3D8 probe 81/81, DSOUND probe 48/48; d3d8 check 14,018/0.

## Remaining

* Window events are handled only when the game presents a frame. If a long stretch without presents appears, the display moves to a service thread (design §2).
