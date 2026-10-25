# 작업 지시: 빌드 스크립트의 중첩 submodule 검사 (issue #121)

rexbox(`third_party/rexbox`)는 자기 submodule(SDL, imgui, smaa, XbSymbolDatabase)을 가진다. `git submodule update --init`만 실행하면 그 네 디렉터리는 있지만 비어 있고, 구성은 rexbox 안에서 같은 비재귀 명령을 안내하는 메시지로 멈춘다(rexbox의 메시지는 reexec/rexbox#33에서 고친다). 빌드 스크립트가 CMake 전에 올바른 명령을 말하게 한다. 설계 변경은 없다.

1. 작업 브랜치 `i121-nested-submodule-check`. 커밋 제목 끝에 `(#121)`.
2. `scripts/build_win32_x86.ps1`, `scripts/build_linux.sh`: 구성 전에 `git submodule status --recursive`를 읽어 체크아웃되지 않은 submodule(`-`로 시작하는 줄)이 있으면 그 경로와 `git submodule update --init --recursive`를 말하고 멈춘다. `.git`이 없는 트리(내보낸 소스)는 검사를 건너뛴다. 중첩 submodule의 이름은 적지 않는다(rexbox가 바뀌어도 그대로 동작해야 한다).
3. 검증: 중첩 submodule 하나를 `git submodule deinit`한 뒤 스크립트가 CMake 전에 멈추는지, 다시 초기화한 뒤 Release 빌드가 통과하는지.
4. 작업 로그 `docs/work-logs/20261025-i121-nested-submodule-check.md`.

---

# Work order: the nested-submodule check in the build scripts (issue #121)

rexbox (`third_party/rexbox`) carries its own submodules (SDL, imgui, smaa, XbSymbolDatabase). After a plain `git submodule update --init` those four directories exist but are empty, and the configure stops inside rexbox with a message that names the same non-recursive command (rexbox's message is fixed in reexec/rexbox#33). The build scripts say the right command before CMake runs. No design change.

(1) Task branch `i121-nested-submodule-check`; commit titles end with `(#121)`. (2) `scripts/build_win32_x86.ps1` and `scripts/build_linux.sh`: before configuring, read `git submodule status --recursive`; when a submodule is not checked out (a line starting with `-`), name its path and `git submodule update --init --recursive` and stop. A tree without `.git` (an exported source) skips the check. The nested submodules are not named, so the check keeps working when rexbox changes. (3) Verify: with one nested submodule `git submodule deinit`-ed the script stops before CMake; after re-initializing it, a Release build passes. (4) Work log `docs/work-logs/20261025-i121-nested-submodule-check.md`.
