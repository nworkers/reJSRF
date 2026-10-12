# 작업 지시: 소리가 나지 않는 문제 — 호스트 스레드 스택과 자기 일시정지 (issue #25)

설계: [20261013-i025](../design/20261013-i025-audio-host-threads.md)

1. `src/app/CMakeLists.txt`: `/STACK:67108864`를 `/LARGEADDRESSAWARE`로 바꾼다.
2. `kernel_thread.cpp`: 자기 자신에 대한 `NtSuspendThread`는 재개될 때까지 기다린다.
3. 검증: 빌드, probe, 오디오 출력 줄, 숨김 창 반복 실행, 일시정지 호출 수.
4. 문서: `docs/analysis/dsound.md`, `docs/analysis/boot-frontier.md`, `ARCHITECTURE.md`, 작업 로그.

---

# Work order: no sound — host-thread stacks and self-suspension (issue #25)

Design: [20261013-i025](../design/20261013-i025-audio-host-threads.md)

(1) `src/app/CMakeLists.txt`: replace `/STACK:67108864` with `/LARGEADDRESSAWARE`. (2) `kernel_thread.cpp`: `NtSuspendThread` on the calling thread waits until resumed. (3) Verify: builds, probes, the audio output line, repeated hidden-window runs, the suspend call rate. (4) Documents: `docs/analysis/dsound.md`, `docs/analysis/boot-frontier.md`, `ARCHITECTURE.md`, the work log.
