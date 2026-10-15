# 작업 지시: Begin/End 즉시 모드 그리기 (issue #62)

설계: [20261015-i062](../design/20261015-i062-immediate-mode.md)

1. 프로파일에 미구현 경로의 프레임당 호출 수(`unimplemented per frame:`)를 더하고, 데모에서 미구현 사항의 빈도를 잰다.
2. 임시 추적으로 게임의 `Begin`/`End` 호출(레지스터, 상태)을 확인한다. 추적 코드는 커밋하지 않는다.
3. `d3d8_immediate.h/.cpp`: 레지스터 유지, 정점 확정, FVF·선언 배치로 묶기.
4. `Draw`에 호스트 정점 소스를 더하고 `PrepareHostVertexData`로 스트리밍한다. `Begin`/`End`/`SetVertexData2f`/`4f`를 연결한다.
5. `rexbox_d3d8_probe`에 검사를 더한다.
6. 데모 프레임 덤프와 프로파일로 확인한다. Debug/Release, 회귀 도구를 돌린다.
7. 문서: ARCHITECTURE, kb(xbox-d3d8-api), boot-frontier, 가이드(probe 수), 작업 로그.

---

# Work order: Begin/End immediate-mode drawing (issue #62)

Design: [20261015-i062](../design/20261015-i062-immediate-mode.md)

1. Add calls per frame into unimplemented paths to the profile (`unimplemented per frame:`) and measure how often the demo hits each.
2. Confirm the game's `Begin`/`End` calls (registers, state) with a temporary trace, not committed.
3. `d3d8_immediate.h/.cpp`: registers kept, vertices completed, packing in the FVF or declaration layout.
4. A host vertex source in `Draw`, streamed by `PrepareHostVertexData`; `Begin`, `End`, `SetVertexData2f` and `4f` wired to it.
5. Checks in `rexbox_d3d8_probe`.
6. Demo frame dumps and the profile; Debug and Release, regression tools.
7. Documents: ARCHITECTURE, the kb (xbox-d3d8-api), boot-frontier, the guide (probe count), the work log.
