# 설계: 게임패드 진동과 Input 탭 (issue #94)

## 1. 목표

* JSRF가 `XInputSetState`로 요청하는 진동을 호스트 게임패드로 낸다.
* 설정 메뉴에 Input 탭을 새로 두고, 진동 켜기/끄기와 세기를 둔다.

## 2. 현재 상태(**확인됨**)

* 게임의 패드 갱신 함수 `0x00167180`이 `XInputSetState`(`0x001C3E85`, HLE로 연결됨)를 부른다.
  * 피드백 구조체는 `ebx+0x3F4`이고, 모터 값을 `+0x42`(`[ebx+0x436]`)에 쓴다.
  * 이전 요청의 `dwStatus`가 끝났을 때만 다음 요청을 보낸다.
* Xbox `XINPUT_FEEDBACK`: 헤더(`dwStatus`, `hEvent`, 58바이트 예약) 66바이트 뒤에 `XINPUT_RUMBLE { WORD wLeftMotorSpeed; WORD wRightMotorSpeed; }`(`+0x42`, `+0x44`). 왼쪽은 저주파(큰 모터), 오른쪽은 고주파(작은 모터)다.
* HLE `XInputSetState`는 값을 무시하고 바로 성공으로 완료한다. 플랫폼 계약(`input.h`)에는 출력이 없다. SDL 백엔드는 SDL3 게임패드를 이미 열어 쓴다.
* xboxrecomp는 관계없다(게임 코드를 lift할 뿐이다).

## 3. 설계

### 3.1 플랫폼 계약 (`include/rexbox/platform/input.h`)

```cpp
// Motor speeds 0..65535 (left: low frequency, right: high frequency). Holds until changed.
void SetGamepadRumble(uint32_t index, uint16_t left, uint16_t right);
// Strength in percent applied to every rumble; 0 turns rumble off.
void SetRumbleStrength(uint32_t percent);
// A short rumble at the current strength, to try the setting.
void TestGamepadRumble(uint32_t index, uint32_t milliseconds);
```

### 3.2 SDL 구현 (`src/platform/sdl/input_sdl.cpp`)

* 컨트롤러마다 게임이 원한 값, 실제로 건 값, 건 시각을 둔다.
* 적용 값 = 원한 값 × 세기%. 설정 메뉴가 열려 있으면 0이다(메뉴가 열린 동안 게임은 입력을 받지 않으므로 진동도 멈춘다).
* Xbox 진동은 바꿀 때까지 이어지고, `SDL_RumbleGamepad`는 지속시간을 받는다. 그래서 0이 아니면 2초로 걸고 1초마다 다시 건다. 0이면 멈춘다.
* 다시 거는 일은 `SetGamepadRumble`과 `ReadGamepad`(게임이 매 프레임 상태를 읽음)에서 한다. 메뉴 열림은 `ReadGamepad`의 앞에서 처리한다.
* 테스트는 메뉴가 열려 있어도 `SDL_RumbleGamepad`로 바로 걸고, 그 시간 동안은 다시 걸기가 덮어쓰지 않는다.
* 키보드 대체 입력이거나 패드가 없으면 아무것도 하지 않는다.

### 3.3 HLE (`src/rexbox/hle/xinput/xinput_state.cpp`)

* `XInputSetState`: `+0x42`, `+0x44`를 읽어 그 장치의 포트로 `SetGamepadRumble`을 부른다. 완료 방식은 그대로다.
* `XInputClose`: 그 포트의 진동을 0으로 한다.

### 3.4 설정과 Input 탭

* `rejsrf.ini`에 `[Input]` 절: `Vibration = true|false`(기본 true), `VibrationStrength = 10..100`(기본 100).
* 설정 메뉴에 Info, Graphics 다음으로 Input 탭을 둔다.
  * Vibration 체크박스.
  * Strength 슬라이더(10~100%, Vibration이 꺼져 있으면 비활성).
  * Test 버튼: 첫 컨트롤러를 0.5초 진동한다.
* 플랫폼에 넘기는 세기는 Vibration이 꺼져 있으면 0, 켜져 있으면 Strength다.

## 4. 하지 않는 것

* 포트별 설정, 컨트롤러 이름·배정 화면, 트리거 진동.

---

# Design: gamepad vibration and an Input tab (issue #94)

## 1. Goal

* Play the rumble JSRF asks for through `XInputSetState` on the host gamepad.
* Add an Input tab to the settings menu with vibration on/off and strength.

## 2. Current state (**confirmed**)

* The game's pad update `0x00167180` calls `XInputSetState` (`0x001C3E85`, bound to the HLE).
  * The feedback block is at `ebx+0x3F4`, and the motor speeds are written at `+0x42` (`[ebx+0x436]`).
  * The next request is sent only once the previous one's `dwStatus` is complete.
* The Xbox `XINPUT_FEEDBACK` is a 66-byte header (`dwStatus`, `hEvent`, 58 reserved bytes) followed by `XINPUT_RUMBLE { WORD wLeftMotorSpeed; WORD wRightMotorSpeed; }` (`+0x42`, `+0x44`). Left is the low-frequency (large) motor, right the high-frequency (small) one.
* The HLE `XInputSetState` ignores the values and completes at once. The platform contract (`input.h`) has no output. The SDL backend already opens SDL3 gamepads.
* xboxrecomp is not involved (it only lifts the game's code).

## 3. Design

### 3.1 Platform contract (`include/rexbox/platform/input.h`)

`SetGamepadRumble(index, left, right)` (0..65535, held until changed), `SetRumbleStrength(percent)` (0 turns rumble off), `TestGamepadRumble(index, milliseconds)`.

### 3.2 SDL implementation (`src/platform/sdl/input_sdl.cpp`)

* Per controller: the speeds the game asked for, the speeds applied, and when they were applied.
* Applied = asked × strength %, and 0 while the settings menu is open (the game gets no input then, so it gets no rumble either).
* Xbox rumble lasts until changed, while `SDL_RumbleGamepad` takes a duration. A nonzero rumble is applied for 2 s and renewed every second; zero stops it.
* Renewal happens in `SetGamepadRumble` and in `ReadGamepad` (the game reads state every frame), before `ReadGamepad`'s menu early-out.
* A test rumbles through `SDL_RumbleGamepad` at once, even with the menu open, and renewal leaves it alone for its length.
* Nothing happens for the keyboard fallback or without a gamepad.

### 3.3 HLE (`src/rexbox/hle/xinput/xinput_state.cpp`)

* `XInputSetState` reads `+0x42` and `+0x44` and calls `SetGamepadRumble` for the device's port. Completion is unchanged.
* `XInputClose` sets that port's rumble to 0.

### 3.4 Settings and the Input tab

* An `[Input]` section in `rejsrf.ini`: `Vibration = true|false` (default true), `VibrationStrength = 10..100` (default 100).
* An Input tab after Info and Graphics: a Vibration checkbox, a Strength slider (10-100 %, disabled with Vibration off), and a Test button (rumbles the first controller for 0.5 s).
* The strength passed to the platform is 0 with Vibration off, else Strength.

## 4. Not done

* Per-port settings, a controller name/assignment screen, trigger rumble.
