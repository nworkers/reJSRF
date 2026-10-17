# 작업 지시: 설정 오버레이와 rejsrf.ini (issue #84)

설계: [20261017-i084](../design/20261017-i084-settings-overlay.md)

1. `third_party/imgui` submodule(v1.93.0), CMake 대상 `imgui`(코어). 플랫폼에 SDL3·OpenGL3 백엔드.
2. 플랫폼: `overlay.h`(빌더, `RenderOverlay`, 보이기, Tab·게임패드 토글, 이벤트 전달), 입력 차단, `display.h`(`SetFullscreen`, `IsFullscreen`, `SetVsync`, `SetPictureAspect`, `DisplayConfig.fullscreen`), `paths.h`(실행 파일·사용자 폴더).
3. D3D8 HLE: `d3d8_display.h`(`SetDisplayDefaults`, 렌더링 배율, 와이드스크린, `GetRendererInfo`). present에서 와이드 맞춤과 `RenderOverlay`.
4. 앱: `settings.*`(INI), `settings_ui.*`(탭, FPS 패널), `main.cpp`에서 읽기·기본값·빌더 등록, 빌드 스탬프에 커밋.
5. 검증: 빌드, 회귀 도구, 숨긴 창 데모 BMP, INI 동작.
6. 문서: 작업 로그, 가이드, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`.

---

# Work order: settings overlay and rejsrf.ini (issue #84)

Design: [20261017-i084](../design/20261017-i084-settings-overlay.md)

1. `third_party/imgui` submodule (v1.93.0), CMake target `imgui` (core); the SDL3 and OpenGL3 backends in the platform.
2. Platform: `overlay.h` (builder, `RenderOverlay`, visibility, Tab and gamepad toggles, event forwarding), blocking input, `display.h` (`SetFullscreen`, `IsFullscreen`, `SetVsync`, `SetPictureAspect`, `DisplayConfig.fullscreen`), `paths.h` (executable and user folders).
3. D3D8 HLE: `d3d8_display.h` (`SetDisplayDefaults`, render scale, widescreen, `GetRendererInfo`); widescreen fitting and `RenderOverlay` in present.
4. App: `settings.*` (INI), `settings_ui.*` (tabs, FPS panel); `main.cpp` reads settings, sets defaults and registers the builder; the commit in the build stamp.
5. Verification: builds, regression tools, hidden-window demo BMPs, INI behavior.
6. Documents: work log, guide, `ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`.
