# 작업 지시: Info 탭에 rexbox 버전과 커밋 표시, Stats 그래프 높이 절반 (issue #111)

설계: [20261023-i111](../design/20261023-i111-rexbox-version-info.md)

1. `src/app/build_stamp.cmake`: `REJSRF_REXBOX_VERSION`, `REJSRF_REXBOX_COMMIT`를 `rejsrf_build_stamp.h`에 쓴다.
2. `BuildInfo`에 `rexbox_version`, `rexbox_commit`. `main.cpp`가 채운다.
3. `SettingsUi::DrawInfoTab`: `Game` 다음에 `rexbox`, `rexbox commit` 두 줄. `DrawGraph` 높이 64 → 32.
4. 검증: 빌드, 생성 헤더 값 확인.
5. 문서: 작업 로그, 설계 i084의 Info 탭 항목, 가이드의 Info 탭 설명.

---

# Work order: the rexbox version and commit in the Info tab, Stats graphs at half height (issue #111)

Design: [20261023-i111](../design/20261023-i111-rexbox-version-info.md)

1. `src/app/build_stamp.cmake`: write `REJSRF_REXBOX_VERSION` and `REJSRF_REXBOX_COMMIT` into `rejsrf_build_stamp.h`.
2. `rexbox_version` and `rexbox_commit` in `BuildInfo`, filled by `main.cpp`.
3. `SettingsUi::DrawInfoTab`: two rows, `rexbox` and `rexbox commit`, after `Game`. `DrawGraph` height 64 → 32.
4. Verification: build, check the generated header's values.
5. Documents: work log, the Info tab entry of design i084, the guide's Info tab description.
