# 작업 지시: 렌더 타깃을 표면 메모리로 식별 (issue #71)

설계: [20261017-i071](../design/20261017-i071-render-target-by-memory.md)

1. `d3d8_gl_targets`
   * `g_colors`, `g_depths`의 키를 물리 데이터 주소로 바꾼다. `g_framebuffers`는 메모리 쌍으로 찾는다.
   * `BindRenderTarget`: 헤더에서 메모리를 읽어 찾고, 크기가 다르면 그 메모리의 GL 객체를 다시 만든다.
   * `SampleRenderTarget`: 메모리로 바로 찾는다.
   * 헤더를 받는 함수들은 헤더에서 메모리를 읽는다. 보고 문구에 메모리를 더한다.
2. 검증: Release·Debug 빌드, 회귀 도구 세 개, 데모 프레임 BMP 비교.
3. 문서: 작업 로그, `ARCHITECTURE.md`·관련 분석 문서의 렌더 타깃 설명.

---

# Work order: identify render targets by surface memory (issue #71)

Design: [20261017-i071](../design/20261017-i071-render-target-by-memory.md)

1. `d3d8_gl_targets`
   * Key `g_colors` and `g_depths` by physical data address; look up `g_framebuffers` by the memory pair.
   * `BindRenderTarget`: find the target by the memory read from the header, and remake that memory's GL objects when the size differs.
   * `SampleRenderTarget`: find the target by memory directly.
   * Functions taking a header read the memory from it. The reports gain the memory.
2. Verification: Release and Debug builds, the three regression tools, comparing the demo frame BMP.
3. Documents: work log, and the render-target description in `ARCHITECTURE.md` and the related analysis.
