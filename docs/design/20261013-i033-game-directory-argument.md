# 설계: 실행 인자를 게임 디렉터리로 (issue #33)

선행: [i003 §4 파일](20261012-i003-kernel-hle.md), [jsrf-xbe-overview](../analysis/jsrf-xbe-overview.md). §1의 XBE 고르기는 [i035](20261013-i035-default-xbe.md)가 대체한다(`default.xbe`, `--xbe`).

## 1. 결정

* `rejsrf <게임 디렉터리>`만 받는다. 게임 디렉터리는 디스크에서 추출한 폴더(`default_eng.xbe`, `default.xbe`, `Media\`)이고, 커널 HLE가 그대로 `D:`로 쓴다. 전에는 XBE 경로를 받아 그 부모를 `D:`로 썼다. 사용자에게 필요한 것은 디렉터리뿐이므로 파일명을 받지 않기로 했다.
* XBE는 이름이 아니라 **내용**으로 고른다.
  1. 디렉터리의 `.xbe`(대소문자 무시)를 모두 읽어 타이틀 프로파일(`IdentifyXbe`)로 식별한다.
  2. 주 변형(US판)이 있으면 그것을 실행한다.
  3. 없으면 지원하는 다른 변형(일본판)을 읽기만 하고 멈춘다. 지금과 같은 동작이다.
  4. 지원하는 XBE가 없으면 본 파일과 이유를 적고 종료 코드 1로 끝난다.
* 파일 경로를 주면 "게임 디렉터리를 달라"는 메시지와 함께 종료 코드 1로 끝난다. 인자가 없으면 사용법을 보이고 종료 코드 2로 끝난다.
* 도구(`rexbox_xbe_info`, `rejsrf_d3d8_check`, `regenerate.py`)는 XBE 파일을 다루는 도구이므로 그대로 둔다.

## 2. 검증

* 인자 없음 → 2, 파일 → 1, XBE 없는 디렉터리 → 1, 일본판만 있는 디렉터리 → 읽고 멈춤(0), 게임 디렉터리 → US판으로 부팅해 어트랙트 데모.
* CI smoke가 파일과 XBE 없는 디렉터리를 거부하는지 본다.

---

# Design: the game directory as the argument (issue #33)

Prerequisites: [i003 §4 files](20261012-i003-kernel-hle.md), [jsrf-xbe-overview](../analysis/jsrf-xbe-overview.md). How §1 picks the XBE is replaced by [i035](20261013-i035-default-xbe.md) (`default.xbe`, `--xbe`).

## 1. Decisions

* `rejsrf <game directory>` is the only form.
  * The game directory is the folder extracted from the disc (`default_eng.xbe`, `default.xbe`, `Media\`), which the kernel HLE serves as `D:` as it is.
  * Before, the app took an XBE path and served its parent as `D:`. The user only needs to name the directory, so the file name is no longer taken.
* The XBE is chosen by **content**, not by name.
  1. Every `.xbe` in the directory (case-insensitive) is read and identified with the title profile (`IdentifyXbe`).
  2. The primary variant (US) runs if it is there.
  3. Otherwise another supported variant (Japan) is loaded and the app stops, as before.
  4. With no supported XBE, the app lists what it saw and why, and exits with code 1.
* A file path exits with code 1 and a message asking for the game directory. No argument prints the usage and exits with code 2.
* The tools (`rexbox_xbe_info`, `rejsrf_d3d8_check`, `regenerate.py`) work on XBE files and stay as they are.

## 2. Verification

* Exit codes and outcomes by argument:
  * no argument: 2
  * a file: 1
  * a directory without an XBE: 1
  * a directory with only the Japan build: loaded, then stops (0)
  * the game directory: boots the US build into the attract demo
* The CI smoke step rejects a file and a directory without an XBE.
