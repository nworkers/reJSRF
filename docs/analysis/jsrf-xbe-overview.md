# JSRF XBE 개요

`rexbox_xbe_info`(issue #1)로 사용자 덤프의 두 XBE를 읽은 결과다. 표기: **확인됨**은 우리 도구로 직접 읽은 값, **추정**은 정황이나 외부 출처, **미확정**은 다음에 검증할 질문이다.

This is what `rexbox_xbe_info` (issue #1) read from the two XBEs of the user's dump. **Confirmed** values were read by our tool directly; **inferred** ones rest on circumstance or an outside source; **unresolved** ones are questions to check next.

## 1. 빌드 변형 / Build variants

| 항목 / Field | US판 / USA `default.xbe` | 일본판 / Japan `default.xbe` | 한국 번들 / Korea bundle `SegaJSRF.xbe` |
|---|---|---|---|
| 지역 / Region | 1 (North America) | 2 (Japan) | 2 (Asia / Japan) |
| MD5 | `f4e366bfdceee551a09f42219c90d84e` | `d8490cdb75bd47b36c62effdf1461ae0` | `8bf3c450e9c1cc39730d4025ef987e2c` |
| SHA-1 | `86ffb4081ffb1a8a5da2e03d3838382ce31d7ae8` | `178dd80cb800846778c4827f12e2a7a17a52dab1` | `33adb802ce61d24d5243cc8a2c9a0d5c84319c7e` |
| 크기 / Size | 2,281,472 | 2,281,472 | 2,281,472 |
| 인증서 버전 / Cert version | `00000001` | `00000001` | `00000007` |
| 인증서 시각 / Certificate time | `3C5881D9` | `3C56850C` | `3E44325E` |
| 이미지 시각 / Image time | `3C558EA7` | `3C559627` | `3C44170F` |
| 진입점 / Entry point | `00148023` | `00148023` | `00147FF3` (-0x30) |
| TLS index 주소 / address | `00264850` | `00264838` | `00264810` (-0x40) |
| `.data` raw size | `00044574` | `00044554` | `00044574` (US와 같음) |
| 언어 / Native Language | 영어 / English (1) | 일본어 / Japanese (0) | 영어 / English (1, `0x001FC6E4`) |
| 프로파일 레이블 / Profile label | `USA (region 1)` — **1차 타깃 / primary** | `Japan (region 2)` — 지원됨 / supported | `Korea bundle (SegaJSRF.xbe)` — issue #124 |

* **확인됨**: 세 빌드는 title id `5345000A`, XDK 4134, 커널 import 120개가 같다.
* **확인됨**: 한국 번들(`Sega GT 2002 + JSRF - Jet Set Radio Future (Kor)`)의 `SegaJSRF.xbe`는 게임 코드가 영문(Language index 1)이다.
* **확인됨**: `SegaJSRF.xbe`의 섹션 크기는 `.text`가 0x30 줄어든 것(`0x17BB00`) 외에 XDK 섹션 및 `.rdata`, `.data`의 크기가 US판과 완전히 일치한다.
* **확인됨**: `SegaJSRF.xbe`의 225개 named function(seed, extra symbols, decomp)은 주소 시프트 규칙(`va < 0x00074EC3`는 그대로, `0x00074EC3 <= va < 0x18CB40`는 -0x30, `va >= 0x18CB40`는 -0x40)을 적용하면 지문이 100% 일치한다(불일치 0개).
* **확인됨**: `default.xbe`의 MD5는 [dplewis/jsrf](https://github.com/dplewis/jsrf)가 "JSRF (USA)"로 명시한 MD5와 같다. US판을 1차 타깃으로 정한 근거다. [jsrf-recomp](https://github.com/andeecollard/jsrf-recomp)도 US판과 같은 진입점 `0x00148023`을 쓴다.
* **미확정**: 이 디스크가 어느 발매판(일본판인지, 지역 변형을 함께 담은 판인지)인지, 그리고 런처가 어느 XBE를 고르는지.
  * **확인됨**(i104): 덤프에는 `default.xbe`(US)와 같은 `default_eng.xbe`, 그리고 `default_jpn.xbe`가 같은 `Media`와 함께 있다. 두 빌드의 차이는 [language](language.md)에 있다.

* **Confirmed**: both builds share title id `5345000A`, certificate version 1, XDK 4134, entry point `00148023`, the section layout and the 120 kernel imports.
* **Confirmed**: the files differ in 146,461 bytes, including inside `.text` (from file offset `0x1000`), so function addresses from one build do not carry over to the other.
* **Confirmed**: `default.xbe`'s MD5 equals the one [dplewis/jsrf](https://github.com/dplewis/jsrf) names as "JSRF (USA)", which is why the US build is the primary target. [jsrf-recomp](https://github.com/andeecollard/jsrf-recomp) uses the same entry point `0x00148023` for the US build.
* **Unresolved**: which release this disc is (a Japanese one, or one carrying regional variants), and which XBE the dashboard launches.
  * **Confirmed** (i104): the dump holds `default_eng.xbe`, identical to `default.xbe` (US), and `default_jpn.xbe`, sharing one `Media`. How the builds differ is in [language](language.md).

## 2. 이미지 헤더 / Image header (US)

| 항목 / Field | 값 / Value |
|---|---|
| 키 / Keys | retail |
| base | `00010000` |
| image size | `00278620` |
| headers size | `00000988` |
| init flags | `00000005` |
| entry point | `00148023` |
| kernel thunk table | `001C3F60` (`.rdata`의 시작 / start of `.rdata`) |
| stack commit | `00010000` |
| heap reserve / commit | `00100000` / `00001000` |
| TLS | raw data 없음, zero-fill `0x0C`, index `00264850` / no raw data, zero-fill `0x0C`, index `00264850` |

## 3. 섹션 / Sections (US)

플래그: W 쓰기, X 실행, P preload. / Flags: W writable, X executable, P preload.

| 이름 / Name | 플래그 / Flags | vaddr | vsize | raw | raw size |
|---|---|---|---|---|---|
| `.text` | -XP | `00011000` | `0017BB30` | `00001000` | `0017BB30` |
| `D3D` | WXP | `0018CB40` | `000117F8` | `0017D000` | `0000E6B0` |
| `DSOUND` | WXP | `0019E340` | `0001C55C` | `0018C000` | `0001C32C` |
| `MMATRIX` | -XP | `001BA8A0` | `00001210` | `001A9000` | `00001210` |
| `XGRPH` | WXP | `001BBAC0` | `00000CFC` | `001AB000` | `00000CFC` |
| `XPP` | WXP | `001BC7C0` | `00007798` | `001AC000` | `00007798` |
| `.rdata` | -XP | `001C3F60` | `00027800` | `001B4000` | `000277F0` |
| `.data` | WXP | `001EB760` | `00092914` | `001DC000` | `00044574` |
| `DOLBY` | -XP | `0027E080` | `00006D98` | `00221000` | `00006D84` |
| `$$XTIMAGE` | --- | `00284E20` | `00002800` | `00228000` | `00002800` |
| `$$XSIMAGE` | --- | `00287620` | `00001000` | `0022B000` | `00001000` |

* **확인됨**: XDK 라이브러리 코드는 이름 붙은 섹션으로 분리되어 있다(`D3D`, `DSOUND`, `XGRPH`, `XPP`, `MMATRIX`, `DOLBY`). HLE 바인딩 대상 함수의 상당수가 이 섹션 안에 있다는 뜻이다.
* **추정**: `XPP`는 XInput/주변기기(Xbox Peripheral Protocol) 코드, `DOLBY`는 DSP 코드(실행 플래그 없음, APU용 데이터로 보임), `$$XTIMAGE`/`$$XSIMAGE`는 대시보드용 타이틀/저장 이미지다.
* **미확정**: `.text` 안에서 게임 코드와 정적 링크된 CRT·XAPI·CRI 미들웨어 코드의 경계.

* **Confirmed**: XDK library code sits in named sections (`D3D`, `DSOUND`, `XGRPH`, `XPP`, `MMATRIX`, `DOLBY`), so many functions to be bound to HLE live there.
* **Inferred**: `XPP` is XInput/peripheral (Xbox Peripheral Protocol) code, `DOLBY` is DSP code (not executable; apparently data for the APU), and `$$XTIMAGE`/`$$XSIMAGE` are the dashboard's title and save images.
* **Unresolved**: where game code ends and statically linked CRT, XAPI and CRI middleware begin inside `.text`.

## 4. 링크된 XDK 라이브러리 / Linked XDK libraries

| 라이브러리 / Library | 버전 / Version | flags |
|---|---|---|
| XAPILIB | 1.0.4134 | `4001` |
| D3D8 | 1.0.4134 | `4002` |
| DSOUND | 1.0.4134 | `4001` |
| XBOXKRNL | 1.0.4134 | `4001` |
| LIBCMT | 1.0.4134 | `4001` |
| LIBCPMT | 1.0.4134 | `4001` |
| XGRAPHC | 1.0.4134 | `4001` |

**확인됨**: 모든 라이브러리가 XDK 4134다. HLE 모듈이 맞춰야 할 API 표면(예: D3D 렌더 상태 번호)은 4134 기준이다. **추정**: jsrf-recomp는 4134의 렌더 상태 번호가 Cxbx-Reloaded가 쓰는 5933 기준과 FOGENABLE부터 10, PSTEXTUREMODES부터 19만큼 다르다고 보고했다.

**Confirmed**: every library is XDK 4134, so the API surface the HLE modules must match (for example D3D render-state numbers) is 4134's. **Inferred**: jsrf-recomp reports 4134's render-state numbers are 10 lower than the 5933-based ones Cxbx-Reloaded uses from FOGENABLE on, and 19 lower from PSTEXTUREMODES on.

## 5. 커널 import / Kernel imports

**확인됨**: 120개, US와 JP가 같다. 접두사별 개수는 다음과 같다.

**Confirmed**: 120, identical in US and JP. By prefix:

| 접두사 / Prefix | 개수 / Count | 영역 / Area |
|---|---|---|
| `Nt` | 27 | 파일 I/O, 가상 메모리, 이벤트, 대기 / file I/O, virtual memory, events, waits |
| `Ke`, `Kf` | 26 | 스레드, 타이머, DPC, 인터럽트, IRQL / threads, timers, DPCs, interrupts, IRQL |
| `Mm` | 13 | 물리·연속 메모리 / physical and contiguous memory |
| `Rtl` | 11 | critical section, 문자열, 시간, 예외 / critical sections, strings, time, exceptions |
| `Io` | 9 | 장치, 심볼릭 링크 / devices, symbolic links |
| `Hal` | 8 | SMC, PCI, 종료 / SMC, PCI, shutdown |
| `Ex` | 6 | 풀, 비휘발 설정 / pools, non-volatile settings |
| `Xbox`, `Xc`, `Xe` | 9 | 키, 하드웨어 정보, SHA, 섹션 로드 / keys, hardware info, SHA, section load |
| `Av` | 4 | 비디오 모드 / video mode |
| `Ps`, `Ob` | 5 | 시스템 스레드, 객체 참조 / system threads, object references |
| `Dbg`, `LaunchDataPage` | 2 | 디버그 출력, 실행 인자 / debug print, launch data |

* **추정**: `KeConnectInterrupt`, `HalGetInterruptVector`, `IoCreateDevice`, `MmClaimGpuInstanceMemory`는 정적 링크된 D3D8/DSOUND가 하드웨어를 직접 초기화하는 경로다. 이 라이브러리들을 API 수준에서 HLE하면 이 import들은 최소 구현(성공 반환)으로 충분할 가능성이 있다.
* **미확정**: `XeLoadSection`/`XeUnloadSection`으로 동적 로드하는 섹션이 있는지(모든 섹션에 preload 플래그가 있으면 없을 수 있다).

* **Inferred**: `KeConnectInterrupt`, `HalGetInterruptVector`, `IoCreateDevice` and `MmClaimGpuInstanceMemory` are the paths on which the statically linked D3D8/DSOUND initialize the hardware directly. If those libraries are HLE'd at the API level, these imports may need only minimal implementations (return success).
* **Unresolved**: whether any section is loaded dynamically through `XeLoadSection`/`XeUnloadSection` (with every executable section marked preload there may be none).

## 6. 디스크 자산 / Disc assets

**확인됨**: `Media/` 아래 파일 3,220개. 확장자별: `.dat` 1,076, `.bin` 1,069, `.adx` 703, `.prs` 150, `.grf` 150, `.JTX` 22, `.tbl` 19, `.xbx` 5, 기타 소수.

**Confirmed**: 3,220 files under `Media/`. By extension: `.dat` 1,076, `.bin` 1,069, `.adx` 703, `.prs` 150, `.grf` 150, `.JTX` 22, `.tbl` 19, `.xbx` 5, a few others.

* **추정**: `.adx`는 CRI ADX 오디오, `.prs`는 Sega PRS 압축, `.xbx`는 Xbox 대시보드 이미지다. 형식 분석은 해당 자산을 다루는 task에서 한다.
* **Inferred**: `.adx` is CRI ADX audio, `.prs` Sega PRS compression, `.xbx` Xbox dashboard images. Format analysis happens in the tasks that handle those assets.
