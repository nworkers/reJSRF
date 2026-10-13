# 작업 지시: 저사양 호스트를 위한 HLE 비용 줄이기 (issue #46)

설계: [20261014-i046](../design/20261014-i046-hle-performance.md)

1. 프로필에 `compared` 열(비교한 바이트와 범위 수)을 더하고 기준을 잰다. 기준 실행 파일을 보관한다.
2. 플랫폼 계약: `AllocateVirtualMemory`의 `track_writes`, `TakeWrittenPages`(Win32 `MEM_WRITE_WATCH`/`GetWriteWatch`).
3. 런타임: `GuestMemory`가 추적을 켜고 `WriteVersion`을 제공한다.
4. D3D8: 8 KiB 이상 정점·인덱스 범위와 텍스처 표본 해시에서 쓰기 버전을 쓴다.
5. D3D8: 정점 셰이더 상수 버전으로 `SetArray4` 비교를 생략한다.
6. recomp 브리지: `FirstTimeAt`에 스레드별 마지막 지점 빠른 경로를 둔다.
7. D3D8: `BindRenderTarget`의 마지막 쌍을 기억한다.
8. 검증: 회귀 도구, 로고 프레임 덤프 비교, 기준과 번갈아 3회 CPU 측정, 샘플링.
9. 문서: 작업 로그, frame-time 분석, ARCHITECTURE.md.

---

# Work order: cheaper HLE work for low-end hosts (issue #46)

Design: [20261014-i046](../design/20261014-i046-hle-performance.md)

1. Add a `compared` column (bytes and ranges compared) to the profile and measure the baseline. Keep the baseline executable.
2. Platform contract: `track_writes` for `AllocateVirtualMemory`, and `TakeWrittenPages` (Win32 `MEM_WRITE_WATCH`/`GetWriteWatch`).
3. Runtime: `GuestMemory` turns tracking on and provides `WriteVersion`.
4. D3D8: write versions for vertex and index ranges of 8 KiB or more and for the texture sample hash.
5. D3D8: a vertex shader constant version lets `SetArray4` skip its comparison.
6. Recomp bridge: a per-thread last-site fast path in `FirstTimeAt`.
7. D3D8: `BindRenderTarget` remembers the last pair.
8. Verification: regression tools, logo frame dumps compared, CPU measured alternately with the baseline three times, sampling.
9. Documents: work log, the frame-time analysis, ARCHITECTURE.md.
