# 작업 지시: 실행 인자를 게임 디렉터리로 (issue #33)

설계: [20261013-i033](../design/20261013-i033-game-directory-argument.md)

1. `src/app/main.cpp`:
   * 디렉터리 인자를 검사한다.
   * `FindGameXbe`로 XBE를 내용으로 식별해 고른다(주 변형 우선).
   * 게임 디렉터리를 `KernelConfig::game_directory`로 넘긴다.
   * 사용법을 바꾼다.
2. CI smoke: 파일과 XBE 없는 디렉터리의 거부.
3. 문서: README, Win32 가이드(명령과 기대 출력), `ARCHITECTURE.md`, 작업 로그.

---

# Work order: the game directory as the argument (issue #33)

Design: [20261013-i033](../design/20261013-i033-game-directory-argument.md)

1. `src/app/main.cpp`:
   * Check that the argument is a directory.
   * Pick the XBE by content with `FindGameXbe` (primary variant first).
   * Pass the game directory as `KernelConfig::game_directory`.
   * Update the usage text.
2. CI smoke: reject a file and a directory without an XBE.
3. Documents: README, the Win32 guide (command and expected output), `ARCHITECTURE.md`, the work log.
