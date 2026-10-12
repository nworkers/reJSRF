# 작업 지시: 창 제목과 크기·전체화면 조작 (issue #31)

설계: [20261013-i031](../design/20261013-i031-window-controls.md)

1. 표시 계약(`display.h`): `DisplayConfig::scale`과 `SetDisplayTitle`을 더하고, 조작을 문서화한다.
2. SDL 구현(`display_sdl.cpp`):
   * 크기 조절과 종횡비 고정, 2배 크기로 열기
   * drawable 갱신
   * Alt+1/2/3, Alt+Enter, 더블클릭
   * FPS 제목
3. 입력(`input_sdl.cpp`): Alt가 눌린 동안 Enter를 START로 보내지 않는다.
4. 앱: 빌드 일시 스탬프(`build_stamp.cmake`), `$<CONFIG>`, 빌드 정체성, `SetDisplayTitle`.
5. 실제 창과 합성 입력으로 검증한다. Debug/Release 빌드와 probe.
6. 문서: `ARCHITECTURE.md`, Win32 가이드, 작업 로그.

---

# Work order: window title, sizing and fullscreen controls (issue #31)

Design: [20261013-i031](../design/20261013-i031-window-controls.md)

1. Display contract (`display.h`): add `DisplayConfig::scale` and `SetDisplayTitle`, and document the controls.
2. SDL implementation (`display_sdl.cpp`):
   * a resizable window with a locked aspect ratio, opened at 2x
   * drawable updates
   * Alt+1/2/3, Alt+Enter and double click
   * the FPS title
3. Input (`input_sdl.cpp`): Enter is not START while Alt is held.
4. App: the build-time stamp (`build_stamp.cmake`), `$<CONFIG>`, the build identity and `SetDisplayTitle`.
5. Verify with a real window and synthesized input. Debug and Release builds and the probes.
6. Documents: `ARCHITECTURE.md`, the Win32 guide, the work log.
