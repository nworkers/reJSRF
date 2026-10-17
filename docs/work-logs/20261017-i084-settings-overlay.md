# 작업 로그: 설정 오버레이와 rejsrf.ini (issue #84)

설계: [20261017-i084](../design/20261017-i084-settings-overlay.md) · 작업 지시: [20261017-i084](../work-orders/20261017-i084-settings-overlay.md)

## 사전 검토

* 사용자와 정한 것: Widescreen은 이번에는 늘리기, Hor+는 #85. FPS 표시는 메뉴와 "Show FPS overlay" 옵션. 메뉴 중 게임은 계속 진행하고 입력만 막는다. 형식은 INI.
* 실험: `ExQueryNonVolatileSetting`의 XC_VIDEO에 와이드 플래그(`0x00010000`)를 주어도 데모 프레임 1500, 3000, 4650이 바이트 단위로 같았다. 게임은 Xbox 와이드 설정을 쓰지 않는다. 실험 수정은 되돌렸다.

## 한 일

1. `third_party/imgui` submodule(v1.93.0, `c1aaac65e`). 루트 CMake에 코어 정적 라이브러리 `imgui`를 두고, 백엔드는 `rexbox_platform`에 넣었다.
2. 플랫폼
   * `overlay.h`, `sdl/overlay_sdl.cpp`
     * 빌더 등록, `RenderOverlay`(처음에 ImGui와 백엔드 생성, `imgui.ini` 안 씀, 키보드·게임패드 탐색).
     * Tab 토글(ImGui로 넘기지 않음), Back+Start 1초 토글.
     * 창 높이 720 기준으로 UI 배율을 0.25 단위로 맞춘다(스타일 크기와 `FontScaleMain`).
   * `display_sdl_internal.h`: 디스플레이와 오버레이 사이 내부 함수.
   * `display.h`: `SetFullscreen`, `IsFullscreen`, `SetVsync`, `SetPictureAspect`, `DisplayConfig.fullscreen`.
   * `input_sdl.cpp`: 메뉴가 열려 있으면 컨트롤러 0은 연결된 채 입력 없음.
   * `paths.h`, `sdl/paths_sdl.cpp`: 실행 파일 폴더, 사용자 설정 폴더.
3. D3D8 HLE
   * 공개 헤더 `d3d8_display.h`, 구현 `d3d8_display.cpp`, 내부 `d3d8_display_state.h`: 시작 기본값, 와이드스크린, 드라이버 문자열.
   * `OpenDisplay`: 기본값의 vsync·전체화면(`REXBOX_VSYNC`이 이김, `adaptive` 값도 받음), 드라이버 정보 기록, 와이드면 창 비율.
   * `InitialRenderScale`: 환경 변수가 없으면 기본값.
   * present: 와이드면 16:9로 맞춰 늘린다. swap 직전에 `platform::RenderOverlay()`를 부른다.
4. 앱
   * `settings.*`: `rejsrf.ini`([Graphics] RenderScale, Fullscreen, Widescreen, VSync, ShowFpsOverlay). 임시 파일에 쓴 뒤 바꿔 넣고, 쓸 수 없으면 사용자 폴더로 간다.
   * `settings_ui.*`
     * Info·Graphics 탭과 FPS 패널.
     * FPS는 프레임 간격으로 계산한다(0.5초 칸 120개, 현재 값은 최근 1초). 칸마다 프레임 수를 세면 60과 62 사이를 오가며 흔들려서 바꿨다.
     * 실제 상태를 매 프레임 비교해 단축키로 바뀐 것도 저장한다. 시작 시 상태는 쓰지 않는다(환경 변수 보호).
   * `main.cpp`: 설정 읽기, `SetDisplayDefaults`, 빌더 등록. `build_stamp.cmake`: 커밋(`+modified` 표시).
5. 문서: 가이드, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`(Dear ImGui MIT 전문). `.gitignore`에 `rejsrf.ini`.

## 검증

| 검증 | 결과 |
|---|---|
| Release, Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 숨긴 창 데모 95초 | 프레임 1500, 3000, 4650 BMP가 변경 전과 바이트 단위로 같음. ini가 없어 기본값을 썼고, 파일은 만들어지지 않음 |
| 보이는 창(키 입력을 프로그램으로 보냄) | Tab으로 메뉴가 열림(`[overlay] menu opened (Tab)`). Info 탭에 버전, 빌드, 커밋, 게임, 드라이버(NVIDIA RTX 4090, GL 3.3, GLSL 3.30), 표시 상태가 나옴. Graphics 탭 클릭, Ctrl+2 뒤 드롭다운이 `2X (1280x960)`이고 `rejsrf.ini`에 `RenderScale = 2`가 즉시 저장됨 |
| ini를 미리 쓴 실행 | `[settings] loaded`, `render scale 2`. Widescreen으로 창과 화면이 16:9. 메뉴를 닫은 채 FPS 패널(60.0, 60·30·0 눈금과 설명) |
| 게임패드 토글, 메뉴 중 게임 입력 차단, 전체화면·VSync 체크박스 | 미검증(사용자 플레이) |

## 추가: Info 탭에 메모리와 VRAM (사용자 요청)

* 플랫폼: `system_info.h`의 `QueryProcessMemory`(Win32: `GetProcessMemoryInfo`, `GlobalMemoryStatusEx`).
* D3D8: 텍스처 업로드가 RGBA8 바이트를 센다(`TextureCacheBytes`). 렌더 타깃은 배율이 곱해진 저장 크기를 센다(`RenderTargetBytes`). `GetVideoMemoryInfo`는 여기에 드라이버 값을 더한다. NVIDIA는 `GL_NVX_gpu_memory_info`, AMD는 `GL_ATI_meminfo`이고, 질의가 낼 수 있는 GL 오류는 지운다.
* 앱: "Memory"·"Video memory" 섹션을 0.5초마다 갱신한다. Xbox 메모리는 게스트 힙(`Env().heap`)에서 읽는다.
* 확인: Debug 빌드 경고 없음. 사용자가 실행 중이던 게임이 Release 실행 파일을 잡고 있어 Release는 다시 링크하지 못했다. 실행 화면은 아직 확인하지 않았다.

## 추가: OSD 크기를 화면에 비례하게 (사용자 요청)

* 처음 구현은 창 높이 720 기준으로 0.25 단위, 최소 1배였다. 메뉴 크기도 처음 열 때만 정해서, 창 크기를 바꾸면 비율이 달라졌다.
* 이제 배율은 `min(너비/960, 높이/720)`의 연속 값(0.25~8)이다. 메뉴는 기준 560×450 × 배율로 가운데에 고정했다(이동·크기 변경 없음). FPS 패널의 여백도 배율을 따른다.
* 확인: 1280×960 창과 Alt+1 뒤 640×480 창에서 메뉴가 창 너비의 약 58%로 가운데에 놓이고, FPS 패널도 같은 비율이었다. Release 빌드 경고 없음.

---

# Work log: settings overlay and rejsrf.ini (issue #84)

Design: [20261017-i084](../design/20261017-i084-settings-overlay.md) · Work order: [20261017-i084](../work-orders/20261017-i084-settings-overlay.md)

## Review before starting

* Agreed with the user: Widescreen stretches for now, with Hor+ in #85; the FPS display shows with the menu and through a "Show FPS overlay" option; the game keeps running while the menu is open, with input blocked; the format is INI.
* Experiment: answering XC_VIDEO in `ExQueryNonVolatileSetting` with the widescreen flag (`0x00010000`) left demo frames 1500, 3000 and 4650 byte-identical; the title does not use the Xbox widescreen setting. The experiment was reverted.

## Done

1. `third_party/imgui` submodule (v1.93.0, `c1aaac65e`); the core static library `imgui` in the root CMake, the backends in `rexbox_platform`.
2. Platform
   * `overlay.h`, `sdl/overlay_sdl.cpp`
     * Builder registration and `RenderOverlay` (creates ImGui and the backends on first use, no `imgui.ini`, keyboard and gamepad navigation).
     * The Tab toggle (not passed to ImGui) and the Back+Start one-second toggle.
     * A UI scale following the window height on a 720-pixel base, in 0.25 steps (style sizes and `FontScaleMain`).
   * `display_sdl_internal.h`: internal functions between the display and the overlay.
   * `display.h`: `SetFullscreen`, `IsFullscreen`, `SetVsync`, `SetPictureAspect`, `DisplayConfig.fullscreen`.
   * `input_sdl.cpp`: with the menu open, controller 0 is connected with no input.
   * `paths.h`, `sdl/paths_sdl.cpp`: the executable's folder and the user settings folder.
3. D3D8 HLE
   * The public header `d3d8_display.h`, `d3d8_display.cpp` and the internal `d3d8_display_state.h`: start-up defaults, widescreen, driver strings.
   * `OpenDisplay`: vsync and fullscreen from the defaults (`REXBOX_VSYNC` wins and now also takes `adaptive`), records the driver information, sets the window aspect for widescreen.
   * `InitialRenderScale`: the default when no environment variable is set.
   * Present: stretches to 16:9 for widescreen, and calls `platform::RenderOverlay()` just before the swap.
4. App
   * `settings.*`: `rejsrf.ini` ([Graphics] RenderScale, Fullscreen, Widescreen, VSync, ShowFpsOverlay), written through a temporary file, with the user folder as a fallback.
   * `settings_ui.*`
     * The Info and Graphics tabs and the FPS panel.
     * FPS comes from frame intervals (120 bins of 0.5 s; the current value over the last second). Counting frames per bin jittered between 60 and 62, so it was changed.
     * The actual state is compared every frame so hotkey changes are saved too; the state at start is not written (keeps environment overrides out).
   * `main.cpp`: reads the settings, calls `SetDisplayDefaults`, registers the builder. `build_stamp.cmake`: the commit (marked `+modified`).
5. Documents: the guide, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md` (the Dear ImGui MIT text); `rejsrf.ini` in `.gitignore`.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Hidden-window demo, 95 s | frames 1500, 3000 and 4650 byte-identical to before; no ini, so defaults, and no file created |
| Visible window (keys sent by a program) | Tab opened the menu (`[overlay] menu opened (Tab)`); the Info tab showed version, build, commit, game, driver (NVIDIA RTX 4090, GL 3.3, GLSL 3.30) and display state; after clicking the Graphics tab and pressing Ctrl+2 the dropdown read `2X (1280x960)` and `rejsrf.ini` held `RenderScale = 2` at once |
| A run with a prepared ini | `[settings] loaded`, `render scale 2`; widescreen made the window and picture 16:9; the FPS panel (60.0, the 60/30/0 ticks and labels) showed with the menu closed |
| Gamepad toggle, game input blocked by the menu, Fullscreen and VSync checkboxes | not verified (user play) |

## Added: memory and VRAM in the Info tab (user request)

* Platform: `system_info.h` with `QueryProcessMemory` (Win32: `GetProcessMemoryInfo`, `GlobalMemoryStatusEx`).
* D3D8: texture uploads count their RGBA8 bytes (`TextureCacheBytes`), render targets their scaled storage (`RenderTargetBytes`). `GetVideoMemoryInfo` adds the driver's figures from `GL_NVX_gpu_memory_info` (NVIDIA) or `GL_ATI_meminfo` (AMD), clearing the GL error the query may raise.
* App: "Memory" and "Video memory" sections, refreshed every 0.5 s; Xbox memory from the guest heap (`Env().heap`).
* Checked: Debug build without warnings (a game the user was running held the Release executable, so Release was not relinked); runtime display not checked yet.

## Added: OSD size in proportion to the screen (user request)

* The first version scaled on a 720-pixel-high base in 0.25 steps with a minimum of 1x, and set the menu size only when first opened, so resizing the window changed the proportions.
* The scale is now the continuous `min(width/960, height/720)` (0.25 to 8). The menu is fixed at 560×450 × scale, centered (no moving or resizing), and the FPS panel's margin follows the scale.
* Checked: in a 1280×960 window and in 640×480 after Alt+1, the menu sat centered at about 58% of the window's width, and the FPS panel kept the same proportion. No Release build warnings.
