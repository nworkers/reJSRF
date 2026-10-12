# 작업 지시: 장면 노드 순회 fault (issue #21)

설계: [20261012-i021](../design/20261012-i021-node-walk-fault.md)

1. fault 시점 게스트 메모리 덤프(`REXBOX_STOP_DUMP`)와 스택으로 fault 함수(`0x00011070`)를 찾는다.
2. `REJSRF_RECOMP_ABI_CHECK` 빌드(별도 트리 `build/abi`)로 스택 불균형의 첫 호출을 찾는다.
3. xboxrecomp: 업스트림 기준 `fix/cond-branch-into-body`에 `_pass_branches_into_bodies`와 테스트, 포크에 push, 업스트림 PR, `rejsrf`에 병합, submodule 갱신.
4. 재생성, 빌드(Debug/Release), 회귀 도구, 부팅으로 다음 frontier 기록. `REXBOX_FRAME_LOG` 진단을 더한다.
5. 문서: boot-frontier, `docs/kb/external-references.md`, `THIRD_PARTY_NOTICES.md`, `ARCHITECTURE.md`, 가이드, 작업 로그.

---

# Work order: the scene node walk fault (issue #21)

Design: [20261012-i021](../design/20261012-i021-node-walk-fault.md)

(1) Find the faulting function (`0x00011070`) from a guest memory dump at the fault (`REXBOX_STOP_DUMP`) and the stack. (2) Find the first call with a stack imbalance with a `REJSRF_RECOMP_ABI_CHECK` build (separate tree `build/abi`). (3) xboxrecomp: `_pass_branches_into_bodies` and tests on `fix/cond-branch-into-body`, based on upstream; push to the fork, open the upstream PR, merge into `rejsrf`, update the submodule. (4) Regenerate, build Debug and Release, run the regression tools, boot and record the next frontier; add the `REXBOX_FRAME_LOG` diagnostic. (5) Documents: boot-frontier, `docs/kb/external-references.md`, `THIRD_PARTY_NOTICES.md`, `ARCHITECTURE.md`, the guide, the work log.
