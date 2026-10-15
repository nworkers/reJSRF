# 작업 지시: present 정지 보고 (issue #60)

설계: [20261015-i060](../design/20261015-i060-stall-report.md)

1. 런타임 `thread_activity`: 스레드 레코드, HLE·대기·present 기록, 대기 객체 설명 함수 등록, 감시 스레드.
2. `HleRegistry::LookupExport`. recomp bridge의 `rexbox_hle_enter`·`recomp_lookup_manual`에서 마지막 HLE 기록.
3. 커널: 스레드 이름, `WaitAndDeliver`·`KeDelayExecutionThread` 대기 기록, 대기 객체 설명 함수.
4. D3D8 `Swap`에서 present 기록. 앱에서 감시 시작(`REXBOX_STALL_REPORT`).
5. 데모, 짧은 시간으로 정지 유도, 끄기 확인. Debug/Release, 회귀 도구.
6. 문서: ARCHITECTURE(진단 변수), 가이드, 작업 로그.

---

# Work order: present-stall report (issue #60)

Design: [20261015-i060](../design/20261015-i060-stall-report.md)

1. Runtime `thread_activity`: thread records, HLE/wait/present notes, the wait-object describer registration, the monitor thread.
2. `HleRegistry::LookupExport`; the last HLE call noted in the recomp bridge's `rexbox_hle_enter` and `recomp_lookup_manual`.
3. Kernel: thread names, wait notes in `WaitAndDeliver` and `KeDelayExecutionThread`, the wait-object describer.
4. Presents noted in D3D8's `Swap`; the monitor started by the app (`REXBOX_STALL_REPORT`).
5. Demo, a stall provoked with a short time, the off switch; Debug and Release, regression tools.
6. Documents: ARCHITECTURE (diagnostic variables), the guide, the work log.
