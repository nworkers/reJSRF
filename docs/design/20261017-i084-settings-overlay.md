# 설계: 설정 오버레이(Dear ImGui)와 rejsrf.ini (issue #84)

관련: [i080 렌더링 배율](20261017-i080-render-scale.md), [i082 vsync](20261017-i082-vsync.md), 와이드스크린 Hor+는 [#85](https://github.com/nworkers/reJSRF/issues/85)

## 1. 요구

* Dear ImGui로 그린 설정 화면을 Tab으로 켜고 끈다.
* **Info 탭**: reJSRF 버전, 빌드 일시 등, 드라이버 정보.
* **Graphics 탭**
  * Rendering Resolution: 1X(640×480)~8X(5120×3840), 드롭다운.
  * Fullscreen, Widescreen, VSync: 체크박스.
* **우측 상단**: 현재 FPS와 최근 60초 FPS 그래프. 60 FPS 기준선과 30, 0 눈금을 두고, 눈금마다 설명 글을 붙인다.
* 설정은 실행 파일 옆 INI 파일에 둔다. 바꾸는 즉시 저장하고, 시작할 때 읽는다.

사용자와 정한 것:
* Widescreen은 이번에는 4:3 화면을 16:9로 늘린다. Hor+는 #85에서 다룬다(게임이 Xbox의 XC_VIDEO 와이드 플래그를 무시함을 확인했다. 데모 프레임이 바이트 단위로 같았다).
* FPS 표시는 메뉴가 열려 있을 때 늘 보이고, "Show FPS overlay" 체크박스로 메뉴를 닫아도 보이게 할 수 있다.
* 메뉴가 열린 동안 게임은 계속 돌고 입력만 막는다.
* 형식은 INI다.

## 2. 설계

### 2.1 계층

| 계층 | 맡는 것 |
|---|---|
| `third_party/imgui` | Dear ImGui v1.93.0(MIT, submodule). 코어는 정적 라이브러리 `imgui`다. SDL3·OpenGL3 백엔드는 `rexbox_platform` 안에서 컴파일한다(SDL은 플랫폼 계층의 구현 세부라서) |
| 플랫폼(`include/rexbox/platform/overlay.h`, `display.h`, `paths.h`) | ImGui 컨텍스트·백엔드, 이벤트 전달, Tab·게임패드 토글, 그리기 호출. 전체화면·vsync의 실행 중 변경, 화면 비율. 실행 파일·사용자 폴더 경로 |
| D3D8 HLE(공개 헤더 `include/rexbox/hle/d3d8/d3d8_display.h`) | 렌더링 배율, 와이드스크린(늘리기), 시작 기본값, 드라이버 정보. present에서 게임 화면을 창에 그린 뒤 swap 직전에 `platform::RenderOverlay()`를 부른다. HLE는 화면 내용을 모른다 |
| 앱(`src/app/settings.*`, `src/app/settings_ui.*`) | `rejsrf.ini` 읽기·쓰기, 화면 내용(탭, FPS 그래프), 설정 적용 |

`rexbox`는 타이틀을 모르는 상태를 유지한다. 버전 문자열 같은 타이틀 정보는 앱이 넘긴다.

### 2.2 오버레이(플랫폼)

* `SetOverlayBuilder(std::function<void()>)`: 앱이 ImGui 호출을 담은 함수를 등록한다. 메뉴가 닫혀 있어도 매 present마다 불린다. 앱이 FPS를 재고 FPS 오버레이를 그려야 하기 때문이다.
* `RenderOverlay()`: present 스레드(GL 컨텍스트 스레드)에서, 창 프레임버퍼가 묶인 채 swap 직전에 부른다.
  * 처음 부를 때 ImGui 컨텍스트와 백엔드를 만든다. 숨긴 창이나 빌더가 없으면 아무것도 하지 않는다.
  * `imgui.ini`는 쓰지 않는다.
  * ImGui OpenGL3 백엔드가 GL 상태를 저장하고 되돌린다. HLE는 present 뒤에 GL 상태 캐시를 무효로 하므로 게임 그리기와 섞이지 않는다.
* `OverlayVisible()`/`SetOverlayVisible()`.
  * Tab(반복 아님)으로 토글한다. Tab은 ImGui로 넘기지 않는다.
  * 게임패드는 Back+Start를 1초 누르면 토글한다.
* 이벤트: `PumpDisplayEvents`가 모든 SDL 이벤트를 ImGui 백엔드에 넘긴다. 키보드·게임패드 탐색을 켠다.
* 크기(사용자 요청으로 추가): OSD는 창 크기에 비례해 같은 비율을 유지한다. 기준은 960×720 창이고, 배율은 `min(너비/960, 높이/720)`이다(4:3이든 16:9든 창 안에 들어간다). 스타일 크기와 글꼴 배율(`FontScaleMain`)에 연속 값으로 적용한다. 메뉴는 창 가운데에 기준 560×450 × 배율 크기로 고정되고, 사용자가 옮기거나 크기를 바꿀 수 없다.
* 입력 차단: 메뉴가 열려 있으면 `ReadGamepad`가 연결된 채 아무것도 누르지 않은 상태를 돌려준다. 게임이 컨트롤러가 빠졌다고 여기지 않게 하기 위해서다.

### 2.3 표시 제어

* 플랫폼
  * `SetFullscreen(bool)`/`IsFullscreen()`: 지금의 테두리 없는 전체화면.
  * `SetVsync(VsyncMode)`: 실행 중 swap interval 변경(GL 스레드에서 부른다).
  * `SetPictureAspect(width, height)`: 창이 지킬 화면 비율과 Alt+1/2/3의 기준. 와이드면 854×480이다.
  * `DisplayConfig.fullscreen`: 시작 시 전체화면.
* D3D8 HLE
  * `SetDisplayDefaults({render_scale, widescreen, vsync, fullscreen})`: 디바이스를 만들기 전에 앱이 부르는 기본값이다. 환경 변수(`REXBOX_RENDER_SCALE`, `REXBOX_VSYNC`)가 있으면 그 실행에서는 환경 변수가 이긴다.
  * `RenderScale`/`SetRenderScale`(i080), `Widescreen`/`SetWidescreen`.
  * 와이드는 present에서 화면을 16:9로 맞춰 늘린다. 프레임 덤프와 F9 캡처는 게스트 화면 그대로다.
  * `GetRendererInfo()`: GL vendor, renderer, version, GLSL version.

### 2.4 설정 파일

* 경로: 실행 파일 폴더의 `rejsrf.ini`. 그 폴더에 쓸 수 없으면 사용자 폴더(SDL pref path)에 쓰고 한 번 보고한다. 읽을 때도 같은 순서다.
* 형식:

```ini
; reJSRF settings (written by the settings overlay; Tab opens it)
[Graphics]
RenderScale = 1        ; 1..8
Fullscreen = false
Widescreen = false
VSync = true           ; true: adaptive vsync, false: off
ShowFpsOverlay = false
```

* 알 수 없는 키나 형식이 틀린 값은 무시하고 기본값을 쓴다. 저장은 임시 파일에 쓴 뒤 바꿔 넣는다.
* 저장 시점: UI에서 바꿀 때마다. 단축키(Ctrl+1~8, Alt+Enter, 더블클릭)로 실제 상태가 바뀐 것도 앱이 매 프레임 비교해 저장한다.
* 환경 변수로 덮인 값: 시작할 때의 실제 상태를 "마지막으로 본 값"으로 삼는다. 그래서 사용자가 바꾸기 전에는 파일에 쓰지 않는다.

### 2.5 화면 내용(앱)

* 메뉴: "reJSRF Settings" 창 하나, 탭 두 개.
  * **Info**: 버전, 빌드 일시, 구성, 커밋(빌드 시 `git rev-parse --short HEAD`), 플랫폼. 게임(XBE 타이틀 ID). 드라이버(vendor, renderer, GL·GLSL 버전). 표시 상태(모니터 Hz, swap interval, 창 크기, 렌더 크기). 설정 파일 경로.
    * 메모리(사용자 요청으로 추가, 0.5초마다 갱신)
      * 프로세스 working set·committed, 주소 공간 사용량(32비트 프로세스의 4 GiB가 실제 한계다), Xbox 메모리(게스트 힙에서 할당된 양 / 128 MiB). 플랫폼 계약은 `system_info.h`(`QueryProcessMemory`)다.
      * VRAM: 드라이버 보고값이다. NVIDIA는 `GL_NVX_gpu_memory_info`로 사용량/전체(다른 프로그램 포함), AMD는 `GL_ATI_meminfo`로 여유량을 알 수 있고, 그 밖의 드라이버는 "보고 안 함"으로 표시한다. HLE가 만든 GL 리소스(렌더 타깃: 배율이 곱해진 색·샘플링 사본·깊이, 텍스처: 올린 RGBA8 레벨 합)도 함께 보인다(`GetVideoMemoryInfo`).
  * **Graphics**: Rendering Resolution 드롭다운(`1X (640x480)` … `8X (5120x3840)`), Fullscreen, Widescreen(툴팁: 지금은 늘리기, Hor+ 예정), VSync, Show FPS overlay.
* FPS 패널(우측 상단, 입력 없음, 반투명)
  * 현재 FPS: 최근 1초 평균. 최근 60초를 0.5초 단위 120칸으로 그린다.
  * 세로축은 0~70 FPS다. 60 기준선(녹색), 30·0 눈금(회색)을 두고 오른쪽에 설명을 붙인다: `60 full speed`, `30 half speed`, `0 stopped`.
  * 이 게임은 로직이 프레임 수로 진행되므로, 60 아래면 게임 자체가 느려진다는 뜻이다.

### 2.6 하지 않는 것

* Hor+ 와이드스크린(#85), 오디오·키 설정, 메뉴 중 일시 정지.
* 한글 글꼴(UI는 영어).

### 2.7 사용자 확인 뒤 바꾼 것 (앞의 FPS 패널·게임패드 토글을 대체한다)

* 메뉴 높이는 탭 내용에 맞춘다(너비는 비율 고정, 높이는 자동). Info 탭이 스크롤 없이 다 보인다.
* **Stats 창**이 FPS 패널을 대체한다.
  * FPS, Memory, Video memory 세 섹션이 있고, Graphics 탭의 "Stats window" 체크박스로 각각 켠다(`ShowFps`, `ShowMemory`, `ShowVideoMemory`). 켠 섹션만 한 창에 위에서부터 이어 붙인다.
  * 창은 하나라도 켜져 있으면 메뉴와 상관없이 보인다. 제목 표시줄을 끌어 옮길 수 있고, X는 세 섹션을 모두 끈다.
  * 위치는 창의 오른쪽 위 꼭짓점을 게임 창에 대한 비율로 저장한다(`StatsX`, `StatsY`). 끌고 있지 않을 때는 매 프레임 이 비율로 놓는다. 그래서 게임 창 크기가 바뀌어도 같은 자리에 있고, 섹션을 켜면 왼쪽·아래로 자란다.
  * 섹션마다 같은 레이아웃이다: 현재 값, 최근 60초 그래프, 오른쪽 눈금 설명.
    * Memory: 프로세스 working set을 그린다. 축은 최댓값의 1.25배 이상인 둥근 값이고, 눈금은 최대·절반·0이다.
    * Video memory: 드라이버가 알려 주면 GPU 사용량(다른 프로그램 포함)을 카드 용량 축에 그린다. 모르면 HLE 리소스 합계를 자동 축에 그린다.
* **그래프 안정화**: 0.5초 칸을 절대 시간(`floor(초/0.5)`)에 고정한다. 끝난 칸은 값을 확정해 보관하고(최근 120칸), 그래프는 칸 단위로 왼쪽으로만 흐른다. 예전에는 "지금으로부터 60초"를 매 프레임 다시 나눠 지난 값이 흔들렸다. 표시하는 현재 값은 끝난 두 칸(1초)의 평균이다. 메모리 값도 칸이 끝날 때 함께 잰다.
* **게임패드**
  * LT+RT+Y가 눌리는 순간 메뉴를 토글한다(Back+Start 1초를 대체). 트리거는 절반 넘게 당기면 눌린 것으로 본다.
  * LT+RT+L3+R3을 1초 누르면 게임을 끝낸다. 플랫폼이 창 닫기 요청으로 보고하고, HLE는 창을 닫을 때와 같이 종료한다.

## 3. 검증

* Release·Debug 빌드, 회귀 도구.
* 숨긴 창 데모: 프레임 BMP가 변경 전과 같다(오버레이는 숨긴 창에서 그리지 않고, 덤프는 present 전에 읽는다).
* INI: 없을 때 기본값, 바꾸면 즉시 저장, 다시 시작하면 적용, 환경 변수가 이기고 파일에 쓰이지 않는지.
* 사용자 플레이: Tab·게임패드 토글, 메뉴 중 게임 입력 차단, 각 설정의 실행 중 적용, FPS 그래프.

---

# Design: settings overlay (Dear ImGui) and rejsrf.ini (issue #84)

Related: [i080 render scale](20261017-i080-render-scale.md), [i082 vsync](20261017-i082-vsync.md), Hor+ widescreen in [#85](https://github.com/nworkers/reJSRF/issues/85)

## 1. Requirement

* A settings screen drawn with Dear ImGui, toggled with Tab.
* **Info tab**: reJSRF version, build time and so on, driver information.
* **Graphics tab**
  * Rendering Resolution: 1X (640×480) to 8X (5120×3840), a dropdown.
  * Fullscreen, Widescreen, VSync: checkboxes.
* **Top right**: the current FPS and the last 60 seconds as a graph, with a 60 FPS reference line and ticks at 30 and 0, each labelled.
* Settings in an INI file next to the executable, saved on every change and read at start.

Agreed with the user:
* Widescreen stretches the 4:3 picture to 16:9 for now; Hor+ is #85 (the title was found to ignore the Xbox XC_VIDEO widescreen flag: demo frames were byte-identical).
* The FPS display shows while the menu is open, and a "Show FPS overlay" checkbox keeps it on with the menu closed.
* While the menu is open the game keeps running and only input is blocked.
* The format is INI.

## 2. Design

### 2.1 Layers

| Layer | Responsibility |
|---|---|
| `third_party/imgui` | Dear ImGui v1.93.0 (MIT, submodule). The core is the static library `imgui`; the SDL3 and OpenGL3 backends compile inside `rexbox_platform` (SDL is the platform layer's implementation detail) |
| Platform (`include/rexbox/platform/overlay.h`, `display.h`, `paths.h`) | the ImGui context and backends, event forwarding, the Tab and gamepad toggles, the draw call; changing fullscreen and vsync while running, the picture aspect; executable and user folder paths |
| D3D8 HLE (public header `include/rexbox/hle/d3d8/d3d8_display.h`) | render scale, widescreen (stretch), start-up defaults, driver information; calls `platform::RenderOverlay()` in present after drawing the game picture to the window and before the swap. The HLE knows nothing of the content |
| App (`src/app/settings.*`, `src/app/settings_ui.*`) | reading and writing `rejsrf.ini`, the content (tabs, FPS graph), applying settings |

`rexbox` stays title-agnostic; the app passes title information such as the version string.

### 2.2 Overlay (platform)

* `SetOverlayBuilder(std::function<void()>)`: the app registers a function holding its ImGui calls. It runs on every present, menu open or not, since the app measures FPS and may draw the FPS overlay.
* `RenderOverlay()`: called on the present thread (the GL context's thread) with the window framebuffer bound, just before the swap.
  * The first call creates the ImGui context and backends. With a hidden window or no builder it does nothing.
  * No `imgui.ini` is written.
  * The ImGui OpenGL3 backend saves and restores GL state, and the HLE invalidates its GL state cache after a present, so it does not mix with game drawing.
* `OverlayVisible()`/`SetOverlayVisible()`.
  * Tab (not repeats) toggles it; Tab is not passed to ImGui.
  * On a gamepad, Back+Start held for one second toggles it.
* Events: `PumpDisplayEvents` passes every SDL event to the ImGui backend. Keyboard and gamepad navigation are on.
* Size (added at the user's request): the OSD keeps the same proportion to the window. The base is a 960×720 window and the scale is `min(width/960, height/720)`, so it fits a 4:3 or 16:9 window alike; it applies continuously to style sizes and the font scale (`FontScaleMain`). The menu stays centered at 560×450 × scale and cannot be moved or resized.
* Blocking input: while the menu is open, `ReadGamepad` returns a connected controller with nothing pressed, so the title does not think the controller was removed.

### 2.3 Display control

* Platform
  * `SetFullscreen(bool)`/`IsFullscreen()`: the existing borderless fullscreen.
  * `SetVsync(VsyncMode)`: changes the swap interval while running (called on the GL thread).
  * `SetPictureAspect(width, height)`: the aspect the window keeps and the base of Alt+1/2/3; 854×480 for widescreen.
  * `DisplayConfig.fullscreen`: fullscreen at start.
* D3D8 HLE
  * `SetDisplayDefaults({render_scale, widescreen, vsync, fullscreen})`: defaults the app sets before the device exists. Environment variables (`REXBOX_RENDER_SCALE`, `REXBOX_VSYNC`) win for that run when present.
  * `RenderScale`/`SetRenderScale` (i080), `Widescreen`/`SetWidescreen`.
  * Widescreen stretches the picture to 16:9 in present. Frame dumps and F9 captures stay the guest picture.
  * `GetRendererInfo()`: GL vendor, renderer, version and GLSL version.

### 2.4 Settings file

* Path: `rejsrf.ini` in the executable's folder. If that folder is not writable, the user folder (SDL pref path) is used and reported once; reading follows the same order.
* Format:

```ini
; reJSRF settings (written by the settings overlay; Tab opens it)
[Graphics]
RenderScale = 1        ; 1..8
Fullscreen = false
Widescreen = false
VSync = true           ; true: adaptive vsync, false: off
ShowFpsOverlay = false
```

* Unknown keys and malformed values are ignored in favor of defaults. Saving writes a temporary file and swaps it in.
* When it saves: on every change in the UI. Changes of the actual state through hotkeys (Ctrl+1 to 8, Alt+Enter, double click) are saved too; the app compares each frame.
* Values overridden by environment variables: the actual state at start is taken as the "last seen" value, so they are not written until the user changes them.

### 2.5 Content (app)

* Menu: one "reJSRF Settings" window with two tabs.
  * **Info**: version, build time, configuration, commit (`git rev-parse --short HEAD` at build), platform; the game (XBE title ID); the driver (vendor, renderer, GL and GLSL versions); display state (monitor Hz, swap interval, window size, render size); the settings file path.
    * Memory (added at the user's request, refreshed twice a second)
      * The process's working set and committed memory, its address space use (the 4 GiB of a 32-bit process is the real limit), and Xbox memory (allocated from the guest heap, of 128 MiB). The platform contract is `system_info.h` (`QueryProcessMemory`).
      * VRAM as the driver reports it: NVIDIA gives used/total through `GL_NVX_gpu_memory_info` (all programs), AMD gives free memory through `GL_ATI_meminfo`, and other drivers show "not reported". The GL resources this HLE made are listed too (render targets: scaled color, sampling copies and depth; textures: the RGBA8 levels uploaded), through `GetVideoMemoryInfo`.
  * **Graphics**: the Rendering Resolution dropdown (`1X (640x480)` … `8X (5120x3840)`), Fullscreen, Widescreen (tooltip: stretched for now, Hor+ planned), VSync, Show FPS overlay.
* FPS panel (top right, no input, translucent)
  * The current FPS is the average over the last second. The last 60 seconds are drawn as 120 bins of 0.5 s.
  * The vertical axis runs 0 to 70 FPS, with a 60 reference line (green) and 30 and 0 ticks (grey), labelled on the right: `60 full speed`, `30 half speed`, `0 stopped`.
  * The title's logic advances per frame, so below 60 the game itself slows down.

### 2.6 Not done

* Hor+ widescreen (#85), audio and key settings, pausing while the menu is open.
* A Korean font (the UI is in English).

### 2.7 Changes after the user's check (replacing the FPS panel and the gamepad toggle above)

* The menu's height fits the tab (width fixed in proportion, height automatic), so the Info tab shows without scrolling.
* A **stats window** replaces the FPS panel.
  * It has three sections, FPS, Memory and Video memory, each turned on by the Graphics tab's "Stats window" checkboxes (`ShowFps`, `ShowMemory`, `ShowVideoMemory`). Only the sections turned on are stacked in one window, top to bottom.
  * The window shows whenever any section is on, menu or not. It moves by its title bar, and its close box turns all three sections off.
  * Its place is its top-right corner as fractions of the game window (`StatsX`, `StatsY`). Except while it is being dragged, it is placed from those fractions every frame, so it stays put in proportion when the game window changes size and grows leftward and down as sections are added.
  * Every section has the same layout: the current value, a graph of the last 60 seconds, tick labels on the right.
    * Memory plots the process's working set. The axis is a round value at least 1.25 times the peak, with ticks at the maximum, half and 0.
    * Video memory plots the GPU's use (all programs) against the card's size when the driver tells, else this HLE's resources on an automatic axis.
* **A steady graph**: the 0.5 s bins are fixed in absolute time (`floor(seconds/0.5)`). A finished bin's value is final and kept (the last 120), and the graph only scrolls by whole bins. Before, "the last 60 s" was re-binned every frame, so past values shook. The value shown is the average of the last two finished bins (one second). Memory is measured as each bin finishes.
* **Gamepad**
  * LT+RT+Y toggles the menu as it goes down (replacing Back+Start held for a second). A trigger counts past half way.
  * LT+RT+L3+R3 held for a second quits: the platform reports it as a window close request, and the HLE ends as it does when the window closes.

## 3. Verification

* Release and Debug builds, regression tools.
* Hidden-window demo: frame BMPs identical to before (the overlay does not draw in hidden windows, and dumps are read before present).
* INI: defaults when absent, saved at once on change, applied after a restart, environment variables win and are not written.
* User play: the Tab and gamepad toggles, game input blocked while the menu is open, every setting applying while running, the FPS graph.
