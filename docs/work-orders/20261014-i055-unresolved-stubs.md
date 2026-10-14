# 작업 지시: 미해결 스텁이 놓친 함수를 말없이 건너뛰는 문제 (issue #55)

설계: [20261014-i055](../design/20261014-i055-unresolved-stubs.md)

1. xboxrecomp fork `fix/late-tail-jumps`(upstream main 기반): `_pass_late_tail_jumps`와 단위 테스트.
2. xboxrecomp fork `feat/stub-report`(upstream main 기반): 스텁에 `RECOMP_UNRESOLVED_STUB`, `recomp_types.h`의 `RECOMP_STUB_REPORT` 분기와 선언, 테스트.
3. 두 브랜치를 fork `rejsrf`에 합치고 submodule을 갱신한다. upstream PR은 초안만 작업 로그에 둔다.
4. reJSRF: `rejsrf_recomp`에 `RECOMP_STUB_REPORT`, `recomp_bridge.cpp`에 `recomp_unresolved_stub_log`.
5. 다시 생성: 함수 수·스텁 수 기록, 프레임 덤프 비교, 120초 실행(`0x54750` 실행, 스텁 보고).
6. 남은 스텁 분류: `docs/analysis/unresolved-stubs.md`와 색인.
7. Debug/Release 빌드, 회귀 도구, 문서(ARCHITECTURE, boot-frontier 수치, 작업 로그).

---

# Work order: unresolved stubs silently skip functions the disassembler missed (issue #55)

Design: [20261014-i055](../design/20261014-i055-unresolved-stubs.md)

1. xboxrecomp fork `fix/late-tail-jumps` (based on upstream main): `_pass_late_tail_jumps` and unit tests.
2. xboxrecomp fork `feat/stub-report` (based on upstream main): `RECOMP_UNRESOLVED_STUB` in stubs, the `RECOMP_STUB_REPORT` branch and declaration in `recomp_types.h`, tests.
3. Merge both into the fork's `rejsrf` and update the submodule; the upstream pull request is only drafted in the work log.
4. reJSRF: `RECOMP_STUB_REPORT` on `rejsrf_recomp`, `recomp_unresolved_stub_log` in `recomp_bridge.cpp`.
5. Regenerate: record function and stub counts, compare frame dumps, run 120 s (`0x54750` runs, stub reports).
6. Classify the remaining stubs: `docs/analysis/unresolved-stubs.md` and its index.
7. Debug and Release builds, regression tools, documents (ARCHITECTURE, boot-frontier figures, work log).
