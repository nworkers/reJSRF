# Issue #6 작업 로그 — XDK 함수 식별과 HLE 바인딩

작업 지시: [20261012-i006](../work-orders/20261012-i006-xdk-symbol-binding.md) · 설계: [20261012-i006](../design/20261012-i006-xdk-symbol-binding.md)

## 요약

XbSymbolDatabase(MIT)를 submodule로 들여와 도구 `rexbox_xdk_symbols`를 만들었다. US XBE에서 XDK 심볼 363개를 1.5초에 식별하며, jsrf-recomp의 D3D8 진입점 103개 중 98개와 주소·이름이 일치한다. `regenerate.py`는 D3D8·D3D8LTCG 함수 136개 각각에 override를 생성해 직접·간접 호출을 모두 `rexbox_hle_enter`로 보낸다. 원본은 `sub_X_gen`으로 남는다. 부팅은 이제 NV2A fault 대신 **첫 D3D8 API `Direct3D_GetAdapterCount`** 에서 이름을 보고하며 멈춘다. 진단 모드(`REXBOX_HLE_FALLBACK=lifted`)로 D3D8 호출 순서를 얻었고, i003에서 추정한 GPU 접근 지점이 `Direct3D_CreateDevice` → `CMiniport_InitHardware`임을 확인했다.

## 과정

1. XbSymbolDatabase의 라이선스(MIT)와 API를 확인하고, CLI와 같은 순서(라이브러리·섹션 필터 → 컨텍스트 → 수동·전체 스캔 → xref)로 JSON을 내는 도구를 썼다.
2. 생성 코드의 직접 호출은 C 호출로 고정되어 실행 시간 바인딩으로는 가로챌 수 없으므로, i002의 생성 단계 override를 확장했다. override는 디스어셈블러가 탐지한 함수 시작에만 만든다(그렇지 않으면 `sub_X_gen`이 없어 링크가 깨진다). 결과적으로 136개 모두 해당했다.
3. XbSymbolDatabase가 찾지 못하는 5개(jsrf-recomp가 추론한 이름)는 `xdk_symbols_extra.json`으로 보충했다. 첫 정지 함수 `Direct3D_GetAdapterCount`도 그중 하나다.
4. 슬롯에 원본 포인터가 있으므로, 원본을 실행하고 기록하는 진단 모드를 넣어 호출 순서를 얻었다.

## 검증

| 검증 | 결과 |
|---|---|
| `rexbox_xdk_symbols default_eng.xbe` | 363개(D3D8 131+32, DSOUND 137, XAPILIB 48+11, XGRAPHC 4), 1.5초 |
| jsrf-recomp D3D8 표 대조 | 103개 중 98개 주소·이름 일치, 5개는 jsrf-recomp의 추론 이름(보충 파일로 추가) |
| `regenerate.py` | override 136개, `137 wrapped as sub_X_gen`(D3D8 136개 + 디컴파일 1개) |
| Win32 Debug / Release 빌드 | 성공, 경고 0 |
| `rexbox_kernel_probe` / `rejsrf_decomp_check` | 27/27, 6/6 (두 구성) |
| 부팅 | `stopped at unimplemented XDK function D3D8 Direct3D_GetAdapterCount (00199820)` |
| 부팅 + `REXBOX_HLE_FALLBACK=lifted` | D3D8 함수 10개를 원본으로 실행한 뒤 `CMiniport_InitHardware`에서 NV2A `0xFD001804` fault |

## 남은 일

* 다음 task: D3D8 HLE 첫 묶음. 어댑터·모드·Caps 질의와 `Direct3D_CreateDevice`를 구현하고, 화면 출력 백엔드(SDL3 등)를 설계한다.
* DSOUND도 같은 방식으로 HLE 대상에 넣을지는 오디오 task에서 정한다(`hle_libraries.json`에 추가만 하면 된다).

---

# Issue #6 work log — XDK function identification and HLE binding

Work order: [20261012-i006](../work-orders/20261012-i006-xdk-symbol-binding.md) · Design: [20261012-i006](../design/20261012-i006-xdk-symbol-binding.md)

## Summary

XbSymbolDatabase (MIT) came in as a submodule behind the new tool `rexbox_xdk_symbols`, which identifies 363 XDK symbols in the US XBE in 1.5 s and agrees on address and name with 98 of jsrf-recomp's 103 D3D8 entry points. `regenerate.py` generates an override for each of the 136 D3D8/D3D8LTCG functions, sending both direct and indirect calls to `rexbox_hle_enter`, with the original kept as `sub_X_gen`. Boot now stops, instead of faulting on the NV2A, at **the first D3D8 API, `Direct3D_GetAdapterCount`**, naming it. A diagnostic mode (`REXBOX_HLE_FALLBACK=lifted`) yielded the D3D8 call order and confirmed that the GPU access inferred in i003 is `Direct3D_CreateDevice` → `CMiniport_InitHardware`.

## Process

1. Checked XbSymbolDatabase's license (MIT) and API, and wrote a tool emitting JSON in the CLI's order (library and section filters → context → manual and full scans → xrefs).
2. Generated code fixes direct calls as C calls, so run-time binding cannot intercept them; the i002 generation-time override mechanism was extended. Overrides are made only at function starts the disassembler found (otherwise there is no `sub_X_gen` and the link breaks); all 136 qualified.
3. The 5 functions XbSymbolDatabase misses (names inferred by jsrf-recomp) are supplied by `xdk_symbols_extra.json`; the first stop, `Direct3D_GetAdapterCount`, is one of them.
4. Since a slot holds the original's pointer, a diagnostic mode that runs and logs the original gave the call order.

## Verification

The table in the Korean section applies: 363 symbols in 1.5 s; 98 of 103 jsrf-recomp D3D8 entries agree (the other 5 added from its inferred names); 136 overrides and `137 wrapped as sub_X_gen`; Debug and Release builds with zero warnings; `rexbox_kernel_probe` 27/27 and `rejsrf_decomp_check` 6/6 in both; boot stops at `Direct3D_GetAdapterCount`; with the diagnostic fallback, ten D3D8 functions run as originals before `CMiniport_InitHardware` faults on NV2A `0xFD001804`.

## Remaining

* Next task: the first D3D8 HLE batch (adapter, mode and caps queries, `Direct3D_CreateDevice`) and the design of the display backend (SDL3 or similar).
* Whether DSOUND joins the HLE libraries the same way is decided in the audio task (it only takes an entry in `hle_libraries.json`).
