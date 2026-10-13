# 작업 지시: 그리기 밖의 프레임 시간 분석 (issue #42)

설계: [20261013-i042](../design/20261013-i042-frame-time-analysis.md)

1. `REXBOX_D3D8_PROFILE`: `wait` 단계, present 스레드만 세기, `cpu` 열(`ThreadCpuTimeNanoseconds`).
2. 플랫폼 계약 `sampler.h`와 Win32 구현, `REXBOX_SAMPLE`, 링크 `/MAP`, `scripts/perf/resolve_samples.py`.
3. 데모 구간을 프로파일하고 샘플링해 CPU 작업의 구성을 낸다. 상위 lift 함수의 기계어를 본다.
4. 작은 변경: HLE 레지스트리 비트맵, `ReportOncePer`. `/GS-`는 측정해서 판단한다.
5. 변형 실행 파일을 번갈아 실행해 비교하고, 회귀 도구를 돌린다.
6. 문서: `docs/analysis/frame-time.md`, 가이드, `ARCHITECTURE.md`, 작업 로그.

---

# Work order: analysing the frame time outside drawing (issue #42)

Design: [20261013-i042](../design/20261013-i042-frame-time-analysis.md)

1. `REXBOX_D3D8_PROFILE`: a `wait` phase, counting on the presenting thread only, a `cpu` column (`ThreadCpuTimeNanoseconds`).
2. The platform contract `sampler.h` and its Win32 implementation, `REXBOX_SAMPLE`, `/MAP` at link time, `scripts/perf/resolve_samples.py`.
3. Profile and sample the demo window to get the composition of the CPU work, and look at the machine code of the top lifted functions.
4. Small changes: the HLE registry bitmap and `ReportOncePer`. Decide on `/GS-` by measurement.
5. Compare by running variant executables alternately, and run the regression tools.
6. Documents: `docs/analysis/frame-time.md`, the guide, `ARCHITECTURE.md`, the work log.
