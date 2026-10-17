# 작업 로그: 게임패드 진동과 Input 탭 (issue #94)

설계: [20261018-i094](../design/20261018-i094-gamepad-vibration.md) · 작업 지시: [20261018-i094](../work-orders/20261018-i094-gamepad-vibration.md)

## 한 일

1. 조사: JSRF의 패드 갱신 함수 `0x00167180`이 `XInputSetState`를 부르고, 모터 값을 피드백 구조체 `+0x42`에 쓰는 것을 lift 코드에서 확인했다.
2. 플랫폼 계약(`input.h`): `SetGamepadRumble`, `SetRumbleStrength`, `TestGamepadRumble`.
3. SDL(`input_sdl.cpp`)
   * 컨트롤러 4개까지 원한 값·적용 값·적용 시각을 둔다.
   * 적용 값은 원한 값 × 세기%이고, 설정 메뉴가 열려 있으면 0이다.
   * 0이 아니면 `SDL_RumbleGamepad`를 2초로 걸고 1초마다 다시 건다. 값이 바뀌면 바로 건다.
   * 다시 걸기는 `ReadGamepad`(게임이 매 프레임 읽음) 앞부분과 `SetGamepadRumble`에서 한다.
   * 테스트는 메뉴가 열려 있어도 바로 걸고, 그 시간 동안은 다시 걸기가 덮어쓰지 않는다.
4. HLE(`xinput_state.cpp`)
   * `XInputSetState`가 `+0x42`/`+0x44`를 장치 포트로 넘긴다. 0이 아닌 첫 요청은 한 번 기록한다.
   * `XInputClose`가 그 포트의 진동을 멈춘다.
5. 설정: `rejsrf.ini`의 `[Input]` 절(`Vibration`, `VibrationStrength`). 시작할 때 세기를 플랫폼에 넘긴다.
6. Input 탭: Vibration 체크박스, Strength 슬라이더(끌 때는 바로 반영하고 놓을 때 저장), Test 버튼.
7. 문서: 가이드, `ARCHITECTURE.md`.

## 검증

| 검증 | 결과 |
|---|---|
| Release·Debug 빌드 | 경고·오류 없음 |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 check 0 failure / 14018 case 0 difference / 129 case 0 difference |
| 숨긴 창 데모 100초 | 정상. 데모는 진동을 요청하지 않는다(요청 로그 없음). 프레임 1500은 이전 빌드와 같다. 3000·4650은 빌드가 바뀔 때의 실시간 진행 차이로 어긋난다(i091 작업 로그와 같은 현상) |
| 설정 파일 | 기존 `rejsrf.ini`를 읽기만 해서는 바뀌지 않는다 |
| Input 탭 화면 | Info, Graphics, Input 탭. Vibration, Strength 100%, Test 버튼 |
| 성능(사용자가 저하를 보고) | VSync와 vblank 대기를 끈 숨긴 창 데모에서 진동 전(main)과 같다: 구간별 212/194/212 fps 대 212/195/215 fps, 각 2회. Xbox Series X 패드에서 `SDL_RumbleGamepad` 한 번은 약 0.02 ms. 보고한 실행의 로그는 시작 설정이 MSAA 16x와 배율 4였다 |
| 실제 패드 진동(Test 버튼, 플레이 중 진동) | 사용자 확인 대기 |

---

# Work log: gamepad vibration and an Input tab (issue #94)

Design: [20261018-i094](../design/20261018-i094-gamepad-vibration.md) · Work order: [20261018-i094](../work-orders/20261018-i094-gamepad-vibration.md)

## Done

1. Investigation: the lifted code shows JSRF's pad update `0x00167180` calling `XInputSetState` with the motor speeds at `+0x42` of the feedback block.
2. Platform contract (`input.h`): `SetGamepadRumble`, `SetRumbleStrength`, `TestGamepadRumble`.
3. SDL (`input_sdl.cpp`)
   * Up to four controllers, each with the asked speeds, the applied speeds and when they were applied.
   * Applied = asked × strength %, and 0 while the settings menu is open.
   * A nonzero rumble goes to `SDL_RumbleGamepad` for 2 s and is renewed every second; a change applies at once.
   * Renewal happens at the start of `ReadGamepad` (the game reads every frame) and in `SetGamepadRumble`.
   * A test plays at once even with the menu open, and renewal leaves it alone for its length.
4. HLE (`xinput_state.cpp`)
   * `XInputSetState` passes `+0x42`/`+0x44` to the device's port. The first nonzero request is logged once.
   * `XInputClose` stops that port's rumble.
5. Settings: an `[Input]` section in `rejsrf.ini` (`Vibration`, `VibrationStrength`). The strength goes to the platform at start.
6. Input tab: a Vibration checkbox, a Strength slider (followed while dragged, saved on release), a Test button.
7. Documents: the guide, `ARCHITECTURE.md`.

## Verification

| Check | Result |
|---|---|
| Release and Debug builds | no warnings or errors |
| `rexbox_d3d8_probe` / `rejsrf_d3d8_check` / `rejsrf_decomp_check` | 163 checks, 0 failures / 14018 cases, 0 differences / 129 cases, 0 differences |
| Hidden-window demo, 100 s | runs; the demo asks for no rumble (no request logged). Frame 1500 matches the previous build; 3000 and 4650 drift with real-time progress whenever the build changes (as in the i091 work log) |
| Settings file | reading an existing `rejsrf.ini` leaves it unchanged |
| Input tab | Info, Graphics and Input tabs; Vibration, Strength 100 %, the Test button |
| Performance (the user reported a drop) | same as before vibration (main) in the hidden-window demo with VSync and the vblank wait off: 212/194/212 fps against 212/195/215 fps per window, two runs each. One `SDL_RumbleGamepad` takes about 0.02 ms on an Xbox Series X pad. The reported run started with MSAA 16x at scale 4 |
| Real pad rumble (Test button, rumble in play) | waiting for the user's check |
