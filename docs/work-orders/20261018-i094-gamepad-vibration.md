# 작업 지시: 게임패드 진동과 Input 탭 (issue #94)

설계: [20261018-i094](../design/20261018-i094-gamepad-vibration.md)

1. `input.h`: `SetGamepadRumble`, `SetRumbleStrength`, `TestGamepadRumble`.
2. `input_sdl.cpp`: 컨트롤러별 진동 상태, 세기 적용, 메뉴 중 정지, 다시 걸기, 테스트.
3. HLE `XInputSetState`가 `+0x42`/`+0x44`를 넘기고, `XInputClose`가 진동을 멈춘다.
4. 설정: `[Input]` 절(`Vibration`, `VibrationStrength`), 시작할 때 적용, Input 탭.
5. 검증: 빌드, 회귀 도구, 데모(진동 요청 로그), 실제 패드 진동.
6. 문서: 작업 로그, 가이드, `ARCHITECTURE.md`.

---

# Work order: gamepad vibration and an Input tab (issue #94)

Design: [20261018-i094](../design/20261018-i094-gamepad-vibration.md)

1. `input.h`: `SetGamepadRumble`, `SetRumbleStrength`, `TestGamepadRumble`.
2. `input_sdl.cpp`: per-controller rumble state, strength, stop while the menu is open, renewal, the test.
3. The HLE `XInputSetState` passes `+0x42`/`+0x44`; `XInputClose` stops the rumble.
4. Settings: an `[Input]` section (`Vibration`, `VibrationStrength`), applied at start, the Input tab.
5. Verification: builds, regression tools, the demo (rumble request log), real pad rumble.
6. Documents: work log, guide, `ARCHITECTURE.md`.
