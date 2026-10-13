# 작업 지시: lift된 코드를 싸게 만드는 xboxrecomp 개선 (issue #44)

설계: [20261013-i044](../design/20261013-i044-recomp-codegen.md)

1. 샘플을 명령 단위로 분류해(디스어셈블과 맞춤) lift 코드의 비용 원인을 찾는다.
2. fork에 변경마다 upstream main 기반 브랜치를 만들고 `rejsrf`에 합쳐 fork에만 push한다. upstream PR은 내지 않는다.
   * `perf/mem-base-local`: 메모리 기준을 함수마다 한 번 읽는다.
   * `perf/xmm-intrinsics`: XMM 도우미를 SSE2 intrinsic으로 바꾸고, 비트 단위 비교 테스트를 둔다.
3. 생성 코드를 다시 만들고 빌드한 뒤 v0.0.17과 번갈아 실행해 CPU 시간을 비교한다.
4. 회귀 도구를 돌리고, 데모 프레임을 v0.0.15와 비교한다.
5. 남은 후보(레지스터 지역화, 간접 호출 캐시)의 효과 상한을 측정한다.
6. 문서: 작업 로그, frame-time 분석, `THIRD_PARTY_NOTICES.md`, external-references, boot-frontier.

---

# Work order: cheaper lifted code through xboxrecomp (issue #44)

Design: [20261013-i044](../design/20261013-i044-recomp-codegen.md)

1. Classify samples per instruction (matched to the disassembly) to find what lifted code pays for.
2. On the fork, one upstream-based branch per change, merged into `rejsrf` and pushed to the fork only; no upstream PR.
   * `perf/mem-base-local`: read the memory base once per function.
   * `perf/xmm-intrinsics`: SSE2 intrinsics for the XMM helpers, with a bit-for-bit comparison test.
3. Regenerate and build, then compare CPU time against v0.0.17 in alternating runs.
4. Run the regression tools and compare demo frames with v0.0.15.
5. Measure the upper bound of the remaining candidates (register localization, an indirect-call cache).
6. Documents: work log, the frame-time analysis, `THIRD_PARTY_NOTICES.md`, external references, boot-frontier.
