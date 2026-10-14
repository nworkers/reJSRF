# 작업 지시: 늦게 생긴 본문의 조건 분기 (issue #58)

설계: [20261014-i058](../design/20261014-i058-late-cond-branches.md)

1. xboxrecomp fork `fix/cond-branch-into-body`(upstream PR #200 브랜치): `block_ends_in_indirect_jump`, `_pass_late_tail_jumps`의 `jcc` 처리, 그 뒤 `_pass_branches_into_bodies` 재실행, 테스트.
2. fork `rejsrf`에 합치고 submodule 갱신. PR #200 브랜치 push, PR 본문 갱신.
3. seed 없이 다시 생성해 결과 확인. 그다음 `0x1783FE`, `0x1296C7` seed를 넣고 다시 생성.
4. 데모 프레임, Debug/Release, 회귀 도구.
5. 문서: unresolved-stubs, THIRD_PARTY_NOTICES, ARCHITECTURE, 작업 로그.

---

# Work order: conditional branches from late bodies (issue #58)

Design: [20261014-i058](../design/20261014-i058-late-cond-branches.md)

1. xboxrecomp fork `fix/cond-branch-into-body` (upstream PR #200's branch): `block_ends_in_indirect_jump`, `jcc` handling in `_pass_late_tail_jumps`, `_pass_branches_into_bodies` run again after it, tests.
2. Merge into the fork's `rejsrf` and update the submodule; push the PR #200 branch and update the PR description.
3. Regenerate without seeds and check; then add the `0x1783FE` and `0x1296C7` seeds and regenerate.
4. Demo frames, Debug and Release, regression tools.
5. Documents: unresolved-stubs, THIRD_PARTY_NOTICES, ARCHITECTURE, work log.
