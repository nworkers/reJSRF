# 작업 지시: XDK 함수 식별과 HLE 바인딩 (issue #6)

설계: [20261012-i006](../design/20261012-i006-xdk-symbol-binding.md)

1. `third_party/XbSymbolDatabase` submodule(MIT) 고정, 라이브러리만 빌드. THIRD_PARTY_NOTICES 갱신.
2. 도구 `rexbox_xdk_symbols`: XBE 스캔 → JSON.
3. `regenerate.py`: 심볼 JSON 생성, `hle_libraries.json`에 따라 override C 생성, lifter에 함께 전달.
4. `rexbox_recomp`: `rexbox_hle_enter(slot)`(이름으로 export 해석·캐시, 없으면 정지), 정지 사유 `kUnimplementedHle`.
5. 앱: XDK 함수 정지 보고.
6. 식별 결과와 jsrf-recomp D3D8 표 비교, 새 frontier 기록, 문서·작업 로그.

---

# Work order: XDK function identification and HLE binding (issue #6)

Design: [20261012-i006](../design/20261012-i006-xdk-symbol-binding.md)

(1) Pin the `third_party/XbSymbolDatabase` submodule (MIT) and build only its library; update THIRD_PARTY_NOTICES. (2) Tool `rexbox_xdk_symbols`: scan an XBE into JSON. (3) `regenerate.py`: produce the symbol JSON, generate the override C from `hle_libraries.json` and pass it to the lifter. (4) `rexbox_recomp`: `rexbox_hle_enter(slot)` (resolve the export by name and cache it; stop when there is none) and the stop reason `kUnimplementedHle`. (5) App: report XDK-function stops. (6) Compare with jsrf-recomp's D3D8 table, record the new frontier, update docs and the work log.
