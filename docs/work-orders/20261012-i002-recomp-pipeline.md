# 작업 지시: 재컴파일 파이프라인 연결 (issue #2)

설계: [20261012-i002](../design/20261012-i002-recomp-pipeline.md)

1. `scripts/recomp/regenerate.py`: 파싱 → 디스어셈블 → 분류 → ABI → lift를 `gen/<variant>/`로 실행. 디컴파일 소스의 `sub_X` 정의를 모아 `--exclude-manual`로 넘긴다.
2. `rexbox_recomp`: 생성 코드가 요구하는 전역·함수 정의(C), `recomp_lookup_manual/kernel`의 registry 연결, 미구현 커널 호출 정지, 부팅 준비(메모리 offset, 코드 범위, 스택, FS).
3. 플랫폼 계약 `RunGuarded`(Win32 SEH).
4. 커널 thunk 슬롯에 합성 주소를 쓰고 registry를 그 주소로 바인딩(`BindKernelImports` 변경).
5. CMake: `gen/`이 있으면 `rejsrf_recomp` 타깃(`/w /bigobj`). 우리 타깃에만 `/W4`를 적용하도록 경고 설정을 함수로 옮긴다.
6. 앱: 생성 코드가 있으면 진입점부터 실행하고 결과를 보고.
7. 디컴파일 증명: leaf 함수 하나를 `src/game/jsrf/decomp/`에 디컴파일하고 `rejsrf_decomp_check`로 lift 버전과 비교.
8. 문서: ARCHITECTURE, 분석(부팅 frontier), 가이드(재생성 절차), THIRD_PARTY_NOTICES, 작업 로그.
9. 검증: 설계 §8.

완료 기준: Win32에서 생성 코드와 함께 링크되고, 진입점 실행 결과(첫 미구현 커널 호출 또는 fault)가 보고되며, 디컴파일 함수 하나가 우선순위 경로로 동작함이 확인된다.

---

# Work order: connecting the recompilation pipeline (issue #2)

Design: [20261012-i002](../design/20261012-i002-recomp-pipeline.md)

(1) `scripts/recomp/regenerate.py`: run parse → disassemble → classify → ABI → lift into `gen/<variant>/`, passing the `sub_X` definitions collected from the decompiled sources as `--exclude-manual`. (2) `rexbox_recomp`: the globals and functions generated code requires (C), `recomp_lookup_manual/kernel` wired to the registry, stopping at unimplemented kernel calls, boot preparation (memory offset, code range, stack, FS). (3) Platform contract `RunGuarded` (Win32 SEH). (4) Write synthetic addresses into the kernel thunk slots and bind the registry there (`BindKernelImports` changes). (5) CMake: a `rejsrf_recomp` target (`/w /bigobj`) when `gen/` exists; warnings move into a function so `/W4` applies to our targets only. (6) App: run from the entry point when generated code exists and report the outcome. (7) Decompilation proof: decompile one leaf function into `src/game/jsrf/decomp/` and compare it with the lifted version in `rejsrf_decomp_check`. (8) Docs: ARCHITECTURE, analysis (boot frontier), guide (regeneration), THIRD_PARTY_NOTICES, work log. (9) Verify: design §8.

Done when it links with the generated code on Win32, the entry-point run reports its outcome (the first unimplemented kernel call or a fault), and one decompiled function is shown to work through the priority path.
