# 작업 지시: FCMOVcc 번역 (issue #27)

설계: [20261013-i027](../design/20261013-i027-fcmov-logo-fade.md)

1. xboxrecomp: `lift_basic_block`과 `_lift_fpu`에 `FCMOVcc`를 넣고 `tools/recomp/test_lifter_fcmov.py`를 더한다. `fix/xadd-flags`(PR #198)에 커밋하고 PR 제목과 본문을 갱신한다.
2. 포크 `rejsrf` 브랜치에 합치고 push한다. reJSRF submodule을 갱신한다.
3. 재생성, 빌드, 회귀 도구, 부팅 frame dump.
4. 문서: boot-frontier, `THIRD_PARTY_NOTICES.md`, `ARCHITECTURE.md`, 작업 로그.

---

# Work order: lift FCMOVcc (issue #27)

Design: [20261013-i027](../design/20261013-i027-fcmov-logo-fade.md)

(1) xboxrecomp: add `FCMOVcc` to `lift_basic_block` and `_lift_fpu` with `tools/recomp/test_lifter_fcmov.py`; commit on `fix/xadd-flags` (PR #198) and update the PR title and body. (2) Merge into the fork's `rejsrf` branch, push, and update the reJSRF submodule. (3) Regenerate, build, run the regression tools and a boot with frame dumps. (4) Documents: boot-frontier, `THIRD_PARTY_NOTICES.md`, `ARCHITECTURE.md`, the work log.
