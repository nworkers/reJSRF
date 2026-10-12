# Issue #31 작업 로그 — 창 제목과 크기·전체화면 조작

작업 지시: [20261013-i031](../work-orders/20261013-i031-window-controls.md) · 설계: [20261013-i031](../design/20261013-i031-window-controls.md)

## 요약

게임 창의 제목을 rePIU 형식으로 바꿨다: `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 60.0`. 창은 640x480의 2배(1280x960)로 열린다. 종횡비를 지키며 크기를 바꾸거나 최대화할 수 있다. Alt+1/2/3은 1~3배 고정 배율이다. 더블클릭과 Alt+Enter는 테두리 없는 전체화면을 켜고 끈다.

## 과정

1. rePIU의 `BuildWindowTitle`과 이벤트 처리(Alt+Enter, 더블클릭, Alt+1~4, 데스크톱 해상도 전체화면)를 참고했다.
2. 표시 계약에 `DisplayConfig::scale`과 `SetDisplayTitle`을 더했다. 이름·버전·빌드는 앱이 주고, 표시 계층이 GL 버전과 FPS를 붙인다.
3. 빌드 일시는 CMake 스크립트가 빌드마다 헤더를 만들되, 내용이 같으면 쓰지 않게 했다. 처음에 쓴 `file(CONFIGURE)`는 매번 파일 시각을 바꿨다.
4. 변경 없는 빌드에서도 `main.cpp`가 다시 컴파일되는 것을 보았다. MSBuild 추적 로그(`CL.read.*.tlog`)에 NVIDIA 드라이버 캐시(`DXCACHE\*.nvph`, `nvdrsdb.bin`)가 입력으로 기록되어 있었다. `rejsrf_game`과 SDL의 tlog에도 같은 기록이 있다. 이 PC의 드라이버가 컴파일러 프로세스에 붙어 생기는 환경 문제이고, 이번 변경과는 무관하다.
5. 키보드 입력은 키 상태를 읽으므로, Alt가 눌린 동안 Enter를 START로 보내지 않게 했다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug/Release 빌드 | 성공, 경고 0 |
| probe | kernel 56/0, d3d8 81/0, dsound 48/0(Debug, Release) |
| 실제 창(Release) | 제목 `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 60.0`, 클라이언트 1280x960, 스타일 `16CF0000`(크기 조절, 최대화) |
| 합성 입력 | Alt+3 → 1920x1440, Alt+2 → 1280x960, Alt+1 → 640x480. Alt+Enter와 더블클릭 → 3840x2160 `WS_POPUP`(`96020000`), 다시 → 원래 창 |

합성 입력에서는 시작 직후의 첫 단축키 하나가 빠졌다. 어떤 키를 먼저 보내도 같았다. 백그라운드의 테스트 스크립트가 포커스를 가져오지 못한 것이다(Windows의 포커스 강탈 방지).

## 남은 일

* 마우스로 창 가장자리를 끌 때의 종횡비 고정(`SDL_SetWindowAspectRatio`)은 사람이 직접 확인해야 한다. 프로그램으로 크기를 바꾸면 이 제약이 적용되지 않는다.

---

# Issue #31 work log — window title, sizing and fullscreen controls

Work order: [20261013-i031](../work-orders/20261013-i031-window-controls.md) · Design: [20261013-i031](../design/20261013-i031-window-controls.md)

## Summary

The game window's title follows rePIU's form: `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 60.0`. The window opens at twice 640x480 (1280x960). It resizes and maximizes with its aspect ratio kept. Alt+1/2/3 give fixed 1x–3x scales. A double click or Alt+Enter toggles borderless fullscreen.

## Process

1. Followed rePIU's `BuildWindowTitle` and its event handling: Alt+Enter, double click, Alt+1–4, and fullscreen at the desktop resolution.
2. Added `DisplayConfig::scale` and `SetDisplayTitle` to the display contract. The app supplies the name, version and build; the display layer appends the GL version and the FPS.
3. A CMake script writes the build-time header on every build but skips the write when the content is the same. The first version used `file(CONFIGURE)`, which touched the file every time.
4. `main.cpp` still recompiled on builds with no changes. The MSBuild tracking logs (`CL.read.*.tlog`) list NVIDIA driver cache files (`DXCACHE\*.nvph`, `nvdrsdb.bin`) as inputs, and the `rejsrf_game` and SDL tlogs do too. This is an environment issue on this PC, where the driver attaches to the compiler process, and is unrelated to this change.
5. Keyboard input reads key states, so Enter is not sent as START while Alt is held.

## Verification

| Check | Result |
|---|---|
| Win32 Debug and Release builds | succeed with zero warnings |
| probes | kernel 56/0, d3d8 81/0, dsound 48/0 (Debug and Release) |
| real window (Release) | title `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 60.0`; 1280x960 client area; style `16CF0000` (resizable, maximizable) |
| synthesized input | Alt+3 → 1920x1440, Alt+2 → 1280x960, Alt+1 → 640x480; Alt+Enter and a double click → 3840x2160 `WS_POPUP` (`96020000`), and again → the window |

With synthesized input, the first shortcut right after start was lost, whichever key was sent first. The background test script could not take the focus (Windows focus-stealing prevention).

## Remaining

* Someone needs to check by hand that the aspect ratio stays locked while dragging the window edges (`SDL_SetWindowAspectRatio`). Programmatic resizes do not go through that constraint.
