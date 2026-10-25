# 작업 로그: 빌드 스크립트의 중첩 submodule 검사 (issue #121)

작업 지시: [20261025-i121](../work-orders/20261025-i121-nested-submodule-check.md) · rexbox: reexec/rexbox#33

## 한 일

* `scripts\build_win32_x86.ps1`: `Test-Submodules`. `.git`과 `git`이 있으면 `git submodule status --recursive`를 읽고, `-`로 시작하는 줄(등록되었지만 체크아웃되지 않은 submodule)의 경로를 모아 `Submodule(s) not checked out: <경로>; run: git submodule update --init --recursive`로 멈춘다. CMake와 Visual Studio를 찾기 전에 부른다.
* `scripts/build_linux.sh`: 같은 검사를 `cmake --preset` 앞에 둔다(`awk '/^-/ { print $2 }'`, 종료 코드 2).
* 중첩 submodule의 이름은 어디에도 적지 않았다. rexbox가 submodule을 더하거나 빼도 그대로 동작한다.
* rexbox 안의 메시지(`git submodule update --init`)는 reexec/rexbox#33에서 `--recursive`를 붙인다. 이 저장소는 다음 submodule 올림에서 받는다.

## 검증

| 검증 | 결과 |
|---|---|
| `git -C third_party/rexbox submodule deinit -f third_party/smaa` 뒤 `build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols` | CMake 전에 `Submodule(s) not checked out: third_party/rexbox/third_party/smaa; run: git submodule update --init --recursive`로 멈춤, 종료 코드 1 |
| 같은 상태에서 `bash scripts/build_linux.sh x64 release --no-test`(Windows의 Git Bash; 검사 경로만) | `submodule(s) not checked out: third_party/rexbox/third_party/smaa` / `run: git submodule update --init --recursive`, 종료 코드 2 |
| `bash -n scripts/build_linux.sh` | 문법 오류 없음 |
| `git submodule update --init --recursive` 뒤 `git submodule status --recursive` | 여섯 submodule 모두 체크아웃됨 |
| 모두 체크아웃된 상태에서 `build_win32_x86.bat -Configuration Release`(전체) | 검사 통과, 구성과 빌드 완료, `rejsrf.exe`까지 경고 0, 종료 코드 0 |

---

# Work log: the nested-submodule check in the build scripts (issue #121)

Work order: [20261025-i121](../work-orders/20261025-i121-nested-submodule-check.md) · rexbox: reexec/rexbox#33

## Done

* `scripts\build_win32_x86.ps1`: `Test-Submodules`. When `.git` and `git` exist it reads `git submodule status --recursive`, collects the paths of lines starting with `-` (registered submodules that are not checked out) and stops with `Submodule(s) not checked out: <paths>; run: git submodule update --init --recursive`. It runs before CMake and Visual Studio are looked up.
* `scripts/build_linux.sh`: the same check ahead of `cmake --preset` (`awk '/^-/ { print $2 }'`, exit code 2).
* No nested submodule is named anywhere, so the check keeps working when rexbox adds or removes one.
* The message inside rexbox (`git submodule update --init`) gains `--recursive` in reexec/rexbox#33; this repository takes it with the next submodule bump.

## Verification

| Check | Result |
|---|---|
| After `git -C third_party/rexbox submodule deinit -f third_party/smaa`, `build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols` | stops before CMake with `Submodule(s) not checked out: third_party/rexbox/third_party/smaa; run: git submodule update --init --recursive`, exit code 1 |
| Same state, `bash scripts/build_linux.sh x64 release --no-test` (Git Bash on Windows; the check path only) | `submodule(s) not checked out: third_party/rexbox/third_party/smaa` / `run: git submodule update --init --recursive`, exit code 2 |
| `bash -n scripts/build_linux.sh` | no syntax error |
| `git submodule update --init --recursive`, then `git submodule status --recursive` | all six submodules checked out |
| Everything checked out, `build_win32_x86.bat -Configuration Release` (full) | check passes, configure and build complete through `rejsrf.exe` with 0 warnings, exit code 0 |
