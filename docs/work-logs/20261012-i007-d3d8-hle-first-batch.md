# Issue #7 작업 로그 — D3D8 HLE 첫 묶음: SDL3 + OpenGL 호스트, 어댑터·모드·Caps 질의, 디바이스 생성

작업 지시: [20261012-i007](../work-orders/20261012-i007-d3d8-hle-first-batch.md) · 설계: [20261012-i007](../design/20261012-i007-d3d8-hle-first-batch.md)

## 요약

SDL3(release-3.4.18, zlib)를 submodule로 들여와 플랫폼 표시 계약 `display.h`의 백엔드(`src/platform/sdl/`)를 만들고, D3D8 HLE 모듈을 영역별 파일로 나눠 어댑터·모드·Caps 질의 8개와 `Direct3D_CreateDevice`를 구현했다. 질의 함수의 답은 추론하지 않고 새 도구 `rejsrf_d3d8_check`로 lift된 XDK 원본을 실행해 측정했고(모드 24개, Caps 53 DWORD, 포맷 능력 바이트 표 256개), 같은 도구의 비교 모드가 HLE와 원본의 eax·esp·출력 버퍼 일치를 12,202 케이스에서 확인한다. 부팅은 **`Direct3D_CreateDevice`에서 640×480 OpenGL 3.3 창을 열고 성공한 뒤 `D3DDevice_GetBackBuffer`** 에서 멈춘다.

## 과정

1. 환경: 이 PC에는 Python이 없었다(Store 스텁만). `winget`으로 Python 3.12를 사용자 범위에 설치하고 capstone을 넣은 뒤 `regenerate.py`로 생성 코드를 다시 만들었다(220초). submodule 세 개(xboxrecomp, XbSymbolDatabase, SDL)를 받았다.
2. 설계 결정은 issue에 기록된 사용자 결정(SDL3 + OpenGL 계열)을 따랐다. SDL3는 OS가 아니라 공유 호스트 라이브러리이므로 `src/platform/sdl/` 규칙을 AGENTS.md와 CODING_STYLE.md에 더했다.
3. 측정 도구를 먼저 만들었다. 원본 `sub_X_gen`은 커널 HLE(`AvSendTVEncoderOption`) 위에서 그대로 돌고, 내부 D3D 함수(`CMiniport_GetDisplayCapabilities`)는 override를 거치므로 `SetUnimplementedHleFallback(true)`(새 API, 환경 변수 `REXBOX_HLE_FALLBACK`의 프로그램 쪽 스위치)로 lift 본문을 실행시켰다.
4. `--dump`로 드러난 사실: `Direct3D_GetAdapterCount`와 `D3D_GetAdapterModeCount`는 모두 `ret 4`(심볼 데이터베이스는 후자를 인자 2개로 적음); `D3DDISPLAYMODE`는 `{Width, Height, RefreshRate, Flags, Format}`; `D3DDevice_GetDeviceCaps`는 212바이트를 복사하고 eax를 보존; `CheckDeviceFormat`은 `RType`을 보지 않고 `0x0019A058`의 포맷 능력 바이트(bit7 RT, bit6 DS, 0x3C 일반)를 보며 `AdapterFormat`의 bit7도 요구; `CheckDeviceMultiSampleType`은 같은 표를 `MultiSampleType`으로 인덱싱해 실제 멀티샘플 값에는 모두 `NOTAVAILABLE`. 표 바이트는 XBE에서 직접 읽어 HLE 상수로 넣었다.
5. 첫 HLE 초안은 8,450건이 어긋났고(인자 수, 레이아웃, 판정 규칙), 측정값을 반영한 뒤 0건이 됐다.
6. 부팅: 게임은 640×480 `LIN_R5G6B5`, 백버퍼 2, 깊이 `LIN_D24S8`, 멀티샘플 `0x0011`, `HARDWARE_VERTEXPROCESSING`으로 디바이스를 만든다. 창이 열리고 GL 3.3 컨텍스트(Intel HD 620)에서 검게 지운 뒤 `D3DDevice_GetBackBuffer`에서 정지했다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug 빌드(SDL3 포함) | 성공, 우리 타깃 경고 0 |
| Win32 Release 빌드 | 성공, 우리 타깃 경고 0; probe 27/27, decomp 6/6, d3d8_check 12,202/0, 부팅 동일 frontier |
| `rexbox_kernel_probe` | 27/27 |
| `rejsrf_decomp_check` | 6/6 |
| `rejsrf_d3d8_check` | 12,202 케이스, 차이 0 |
| 부팅(`rejsrf.exe default_eng.xbe`) | 창 640×480, OpenGL 3.3; `stopped at unimplemented XDK function D3D8 D3DDevice_GetBackBuffer (0018D620)` |
| CI | 다음 PR에서 확인(생성 코드 없이 SDL 포함 빌드) |

## 남은 일

* 다음 task: `D3DDevice_GetBackBuffer`부터 서피스·텍스처 객체와 렌더 상태, 첫 프레임 present까지(D3D8 HLE 두 번째 묶음).
* 표시 서비스 스레드(macOS, Android, Web)와 커널 AV 설정의 런타임 계약화는 설계 i007 §6대로 보류.
* 사용자 질문으로 확인한 참고 저장소 [reexec/re2DJ](https://github.com/reexec/re2DJ)(BSD-3-Clause)는 `docs/kb/external-references.md`에 추가했다. SDL3 호스트 계층과 DirectSound 버퍼 모델이 공유 후보다.

---

# Issue #7 work log — D3D8 HLE, first batch: SDL3 + OpenGL host, adapter/mode/caps queries, device creation

Work order: [20261012-i007](../work-orders/20261012-i007-d3d8-hle-first-batch.md) · Design: [20261012-i007](../design/20261012-i007-d3d8-hle-first-batch.md)

## Summary

SDL3 (release-3.4.18, zlib) came in as a submodule behind the platform display contract `display.h` (`src/platform/sdl/`), and the D3D8 HLE module was split into one file per area implementing the eight adapter, mode and caps queries and `Direct3D_CreateDevice`. The queries' answers were not inferred but measured by running the lifted XDK originals with the new tool `rejsrf_d3d8_check` (24 modes, 53 caps DWORDs, a 256-entry format capability table), whose comparison mode confirms that HLE and originals agree on eax, esp and output buffers in 12,202 cases. Boot now **succeeds in `Direct3D_CreateDevice`, opening a 640×480 OpenGL 3.3 window, and stops at `D3DDevice_GetBackBuffer`**.

## Process

1. Environment: this PC had no Python (only the Store stub). Python 3.12 was installed per user with `winget`, capstone added, and the generated code rebuilt with `regenerate.py` (220 s). The three submodules (xboxrecomp, XbSymbolDatabase, SDL) were fetched.
2. The design follows the user's decision recorded in the issue (SDL3 + OpenGL family). SDL3 is a shared host library rather than an OS, so the `src/platform/sdl/` rule was added to AGENTS.md and CODING_STYLE.md.
3. The measurement tool came first. The originals `sub_X_gen` run as they are on the kernel HLE (`AvSendTVEncoderOption`); internal D3D functions (`CMiniport_GetDisplayCapabilities`) go through overrides, so `SetUnimplementedHleFallback(true)` (a new API, the programmatic side of `REXBOX_HLE_FALLBACK`) runs their lifted bodies.
4. `--dump` revealed: `Direct3D_GetAdapterCount` and `D3D_GetAdapterModeCount` both do `ret 4` (the symbol database lists the latter with 2 parameters); `D3DDISPLAYMODE` is `{Width, Height, RefreshRate, Flags, Format}`; `D3DDevice_GetDeviceCaps` copies 212 bytes and preserves eax; `CheckDeviceFormat` ignores `RType`, reads the format capability byte at `0x0019A058` (bit 7 render target, bit 6 depth/stencil, 0x3C plain) and also requires bit 7 of `AdapterFormat`; `CheckDeviceMultiSampleType` indexes the same table with `MultiSampleType`, so every real multisample value is `NOTAVAILABLE`. The table bytes were read from the XBE into HLE constants.
5. The first HLE draft differed in 8,450 cases (argument counts, layout, verdict rules); after applying the measurements, zero.
6. Boot: the game creates its device at 640×480 `LIN_R5G6B5`, 2 back buffers, `LIN_D24S8` depth, multisample `0x0011`, `HARDWARE_VERTEXPROCESSING`. The window opened, the GL 3.3 context (Intel HD 620) was cleared to black, and the run stopped at `D3DDevice_GetBackBuffer`.

## Verification

The table in the Korean section applies: Debug build with SDL3 and zero warnings on our targets; Release build with zero warnings on our targets and the same probe, decomp-check, d3d8-check and boot results; `rexbox_kernel_probe` 27/27; `rejsrf_decomp_check` 6/6; `rejsrf_d3d8_check` 12,202 cases with 0 differences; boot opening the 640×480 OpenGL 3.3 window and stopping at `D3DDevice_GetBackBuffer (0018D620)`; CI checked on the next pull request (the no-generated-code build with SDL).

## Remaining

* Next task: from `D3DDevice_GetBackBuffer` through surface and texture objects, render state and the first presented frame (the second D3D8 HLE batch).
* The display service thread (macOS, Android, web) and lifting the kernel's AV settings into a runtime contract stay deferred as in design i007 §6.
* The reference repository [reexec/re2DJ](https://github.com/reexec/re2DJ) (BSD-3-Clause), checked on the user's question, was added to `docs/kb/external-references.md`; its SDL3 host layer and DirectSound buffer model are sharing candidates.
