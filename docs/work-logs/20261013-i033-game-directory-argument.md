# Issue #33 작업 로그 — 실행 인자를 게임 디렉터리로

작업 지시: [20261013-i033](../work-orders/20261013-i033-game-directory-argument.md) · 설계: [20261013-i033](../design/20261013-i033-game-directory-argument.md)

## 요약

`rejsrf`는 이제 게임 디렉터리만 받는다: `rejsrf.exe "roms\Jet Set Radio Future"`. 앱은 디렉터리의 `.xbe`를 내용으로 식별해 US판을 실행하고(`xbe: USA (region 1) (default_eng.xbe)`), 그 폴더를 `D:`로 쓴다. README, Win32 가이드, ARCHITECTURE, CI smoke를 함께 고쳤다.

## 검증

| 경우 | 결과 |
|---|---|
| 인자 없음 | 사용법, 종료 코드 2 |
| `LICENSE`(파일) | `is not a directory; pass the game directory`, 1 |
| `docs`(XBE 없음) | `no .xbe file`, `no supported Jet Set Radio Future XBE`, 1 |
| 일본판만 있는 임시 디렉터리 | `xbe: Japan (region 2) (default.xbe)`, 읽은 뒤 멈춤, 0 |
| `roms\Jet Set Radio Future` | US판 부팅, 2,700번째 present(어트랙트 데모) 덤프, 50초 watchdog |
| Win32 Debug/Release 빌드 | 성공, 경고 0 |

## 남은 일

* 없음. 도구들은 계속 XBE 파일 경로를 받는다(설계 §1).

---

# Issue #33 work log — the game directory as the argument

Work order: [20261013-i033](../work-orders/20261013-i033-game-directory-argument.md) · Design: [20261013-i033](../design/20261013-i033-game-directory-argument.md)

## Summary

`rejsrf` now takes only the game directory: `rejsrf.exe "roms\Jet Set Radio Future"`. The app identifies the `.xbe` files in it by content, runs the US build (`xbe: USA (region 1) (default_eng.xbe)`), and serves the folder as `D:`. The README, the Win32 guide, ARCHITECTURE and the CI smoke step changed with it.

## Verification

| Case | Result |
|---|---|
| no argument | usage; exit code 2 |
| `LICENSE` (a file) | `is not a directory; pass the game directory`; 1 |
| `docs` (no XBE) | `no .xbe file`, `no supported Jet Set Radio Future XBE`; 1 |
| a temporary directory with only the Japan build | `xbe: Japan (region 2) (default.xbe)`; loaded, then stops; 0 |
| `roms\Jet Set Radio Future` | the US build boots; the 2,700th present (attract demo) dumped; 50 s watchdog |
| Win32 Debug and Release builds | succeed with zero warnings |

## Remaining

* Nothing. The tools still take an XBE file path (design §1).
