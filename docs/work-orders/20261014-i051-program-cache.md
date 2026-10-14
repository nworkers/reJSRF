# 작업 지시: 처음 쓰는 GL 프로그램의 컴파일 끊김 줄이기 (issue #51)

설계: [20261014-i051](../design/20261014-i051-program-cache.md)

1. 프로파일에 구간별 프로그램 수·시간, 가장 긴 프레임과 33.4 ms를 넘은 프레임 수를 더해 기준을 잰다.
2. `d3d8_gl_program_cache`: 바이너리 저장·불러오기(드라이버 문자열과 소스가 완전히 같을 때만), 거부되면 컴파일로 돌아감. 위치는 `cache\shaders\`, `REXBOX_SHADER_CACHE`로 바꾸거나 끔.
3. GL 진입점 추가(`glGetProgramBinary`, `glProgramBinary`, `glProgramParameteri`(선택), `glDeleteProgram`), `.gitignore`에 `cache/`.
4. 두 번 실행, 캐시 끔/켬 번갈아 실행, 망가진 항목으로 확인한다.
5. Debug/Release 빌드, 회귀 도구, 문서(가이드, ARCHITECTURE, frame-time, 작업 로그).

## 후속: 저장 경로 (설계 §4)

6. 프로파일에 저장 수·시간(`stored N, X ms`)을 더한다.
7. `StoreProgram`: `close()` 뒤 상태 확인, 실패하면 임시 파일을 지우고 한 번 보고한다.
8. 캐시 끔과 빈 캐시 첫 실행을 번갈아 재고, 읽기 전용 폴더로 실패 경로를 확인한다.
9. Debug/Release 빌드, 회귀 도구, 설계·작업 로그 갱신.

---

# Work order: less stutter from compiling GL programs on first use (issue #51)

Design: [20261014-i051](../design/20261014-i051-program-cache.md)

1. Add per-window program counts and time, the longest frame and the frames over 33.4 ms to the profile, and measure the baseline.
2. `d3d8_gl_program_cache`: store and load binaries (only when the driver identity and the sources match exactly), falling back to compiling when one is rejected; location `cache\shaders\`, moved or turned off with `REXBOX_SHADER_CACHE`.
3. GL entry points (`glGetProgramBinary`, `glProgramBinary`, optional `glProgramParameteri`, `glDeleteProgram`); `cache/` in `.gitignore`.
4. Check with two runs, runs alternating the cache off and on, and damaged entries.
5. Debug and Release builds, regression tools, documents (guide, ARCHITECTURE, frame-time, work log).

## Follow-up: the store path (design §4)

6. Add store counts and time (`stored N, X ms`) to the profile.
7. `StoreProgram`: check the state after `close()`; on failure remove the temporary file and report once.
8. Measure cache-off runs alternating with first runs on an empty cache, and check the failure path with a read-only folder.
9. Debug and Release builds, regression tools, design and work-log updates.
