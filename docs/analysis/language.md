# 언어: US판과 일본판이 언어를 정하는 방법

issue [#104](https://github.com/nworkers/reJSRF/issues/104)의 분석 결과다. 스크립트는 [scripts/analysis/language/](../../scripts/analysis/language/README.md)에 있다.

## 1. 덤프의 XBE (확인됨)

| 파일 | MD5 | 빌드 |
|---|---|---|
| `default.xbe` | `f4e366bfdceee551a09f42219c90d84e` | US(지역 1), `default_eng.xbe`와 같은 파일 |
| `default_eng.xbe` | `f4e366bfdceee551a09f42219c90d84e` | US |
| `default_jpn.xbe` | `d8490cdb75bd47b36c62effdf1461ae0` | 일본(지역 2) |

* 세 XBE는 같은 `Media` 폴더를 쓴다. 이 디스크는 두 빌드를 함께 담고 있다([jsrf-xbe-overview §1](jsrf-xbe-overview.md)의 미확정 질문에 대한 답).
* **미확정**: 원래 디스크에서 대시보드가 어느 XBE를 실행하는지. 덤프의 `default.xbe`는 US판 사본이다.

## 2. 두 빌드 모두 다섯 언어의 자산을 안다 (확인됨)

* 두 XBE의 문자열은 주소 이동으로 바뀐 즉시값을 빼면 같다(`xbe_strings.py`).
* 두 XBE 모두 언어별 자산 이름을 담는다: `A:\Media\Ending\tex_{Eng,Frn,Ger,Jpn,Spa}.dat`, `A:\Media\Mark\List_{eng,frn,ger,jpn,spa}.{bin,dat}`, `A:\Media\Mark\tex_{...}.dat`, 그리고 `_j` 접미사 파일(`e_boss_j.dat`, `pv_aku_j.dat`, `se_grg_j.dat` ...).

## 3. 언어 인덱스 (확인됨, US판)

* 전역 DWORD `0x1FC724`가 언어 인덱스다. getter `sub_00043700`(`mov eax, [ecx]; ret`)를 `mov ecx, 0x1FC724`로 부르는 곳이 189곳이다.
* 인덱스 값의 뜻은 엔딩 텍스처를 고르는 switch(`0x27B87`, 표 `0x27C5C`)에서 확인했다.

| 인덱스 | 언어 | 엔딩 텍스처 |
|---|---|---|
| 0 | 일본어 | `tex_Jpn.dat` |
| 1 | 영어 | `tex_Eng.dat` |
| 2 | 프랑스어 | `tex_Frn.dat` |
| 3 | 독일어 | `tex_Ger.dat` |
| 4 | 스페인어 | `tex_Spa.dat` |

* **미확정**: `sub_00134320`은 0~5의 여섯 갈래로 나뉜다. 5가 무엇인지(이탈리아어 등)는 모른다.

## 4. 언어는 빌드에 박힌 상수다

* **확인됨**: US판 `0x1FC724`는 초기화된 `.data` 안에 있고 파일 값이 **1(영어)**이다.
* **확인됨**: 일본판의 대응 전역은 `0x251F58`(US 주소 + `0x55834`, 193곳에서 같은 방식으로 쓰임)이고 bss에 있어 **0(일본어)**이다(`read_lang_var.py`, `code_diff.py`의 주소 대응).
* **확인됨**: US판에서 `0x1FC724`를 담은 dword 191개는 모두 `mov ecx, imm32`다. 이 주소에 직접 쓰는 명령은 없다.
* **확인됨**: `REXBOX_WATCH32=1FC724`로 12초 실행했을 때 바뀐 것은 로드 시의 0→1 한 번(첫 HLE 진입에서 본 초기값)뿐이다.
* **추정**: 게임은 대시보드 언어(`XC_LANGUAGE`, `XGetLanguage`)를 쓰지 않는다. 커널 HLE의 `XC_LANGUAGE` 답(영어 1)은 이 값과 무관해 보인다. 조회 여부는 아직 추적하지 않았다.
* **추정**: 따라서 US판에서 로드 직후 `0x1FC724`를 0으로 바꾸면 일본어로 동작할 수 있다. 시험은 아직 안 했다.

## 5. 코드 차이 (확인됨, `code_diff.py`)

실행 섹션을 디스어셈블하고 이미지 주소를 정규화해 비교했다.

| 섹션 | 결과 |
|---|---|
| D3D, DSOUND, MMATRIX, XGRPH, XPP, DOLBY | 같음 |
| `.text` | 다른 블록 6개. 실제 차이는 아래 둘이고, 나머지 둘은 코드 사이 자료를 디코드한 잡음이다 |
| `.rdata` | 다른 블록 8개(자료) |
| `.data` | 다른 블록 369개. 자료를 명령으로 읽은 비교라 의미가 없고, dword 단위 비교가 따로 필요하다 |

* **US에만 있는 함수 `0x694A0`**: `mov eax,[esp+4]; cmp eax,0x5E; jae; mov ecx,[ecx]; fld [ecx+eax*4]; ret 4`(0x5E칸 float 표의 getter). 일본판은 그 자리가 nop이다.
* **옮겨진 함수**: US `0xD4440`의 함수가 일본판에서는 `0xD4321` 근처로 옮겨졌다(그 부근 주소 대응이 `+0x30`). 내용은 같아 보인다.
* 주소 대응: `.data`는 대부분 `-0x20`·`-0x18`·`-0x8`만큼, 언어 전역만 `+0x55834`(bss로 옮겨짐) 이동했다.

## 6. 미확정

* US판에서 언어를 0으로 바꾸면 메뉴·자막·음성이 일본판과 같아지는지(실행으로 확인).
* `.data`의 차이 중 주소 이동이 아닌 값(언어별 초기값 표 등)이 있는지.
* US 전용 함수 `0x694A0`를 누가 부르는지, 영어 전용 동작인지.
* 언어 값을 언제 읽는지(부팅 때 한 번인지, 장면마다인지). 실행 중 바꿀 수 있는지가 여기에 달렸다.

---

# Language: how the US and Japanese builds choose it

Findings of issue [#104](https://github.com/nworkers/reJSRF/issues/104). The scripts are in [scripts/analysis/language/](../../scripts/analysis/language/README.md).

## 1. The dump's XBEs (confirmed)

| File | MD5 | Build |
|---|---|---|
| `default.xbe` | `f4e366bfdceee551a09f42219c90d84e` | US (region 1), the same file as `default_eng.xbe` |
| `default_eng.xbe` | `f4e366bfdceee551a09f42219c90d84e` | US |
| `default_jpn.xbe` | `d8490cdb75bd47b36c62effdf1461ae0` | Japan (region 2) |

* All three use the same `Media` folder: the disc carries both builds (answering the unresolved question in [jsrf-xbe-overview §1](jsrf-xbe-overview.md)).
* **Unresolved**: which XBE the dashboard runs from the original disc. The dump's `default.xbe` is a copy of the US build.

## 2. Both builds know the assets of five languages (confirmed)

* The two XBEs' strings are the same apart from immediates changed by relocation (`xbe_strings.py`).
* Both hold the per-language asset names: `A:\Media\Ending\tex_{Eng,Frn,Ger,Jpn,Spa}.dat`, `A:\Media\Mark\List_{eng,frn,ger,jpn,spa}.{bin,dat}`, `A:\Media\Mark\tex_{...}.dat`, and files with a `_j` suffix (`e_boss_j.dat`, `pv_aku_j.dat`, `se_grg_j.dat` ...).

## 3. The language index (confirmed, US build)

* The DWORD global `0x1FC724` is the language index. The getter `sub_00043700` (`mov eax, [ecx]; ret`) is called with `mov ecx, 0x1FC724` at 189 sites.
* The values' meaning comes from the switch that picks the ending texture (`0x27B87`, table `0x27C5C`): 0 Japanese (`tex_Jpn.dat`), 1 English, 2 French, 3 German, 4 Spanish.
* **Unresolved**: `sub_00134320` branches six ways, 0 to 5; what 5 is (Italian?) is unknown.

## 4. The language is a constant built into each build

* **Confirmed**: in the US build `0x1FC724` lies in initialized `.data` and its file value is **1 (English)**.
* **Confirmed**: the Japanese build's counterpart is `0x251F58` (the US address + `0x55834`, used the same way at 193 sites), in bss, so **0 (Japanese)** (`read_lang_var.py` and `code_diff.py`'s address map).
* **Confirmed**: all 191 dwords in the US build holding `0x1FC724` are `mov ecx, imm32`; no instruction writes the address directly.
* **Confirmed**: a 12 s run with `REXBOX_WATCH32=1FC724` saw one change only, 0→1 at load (the initial value, seen at the first HLE entry).
* **Inferred**: the game does not use the dashboard language (`XC_LANGUAGE`, `XGetLanguage`); the kernel HLE's `XC_LANGUAGE` answer (English, 1) looks unrelated. Whether it is queried has not been traced.
* **Inferred**: so setting `0x1FC724` to 0 right after load could make the US build run in Japanese. Not tried yet.

## 5. Code differences (confirmed, `code_diff.py`)

The executable sections were disassembled with image addresses normalized, then compared.

| Section | Result |
|---|---|
| D3D, DSOUND, MMATRIX, XGRPH, XPP, DOLBY | identical |
| `.text` | 6 differing blocks: the two real differences below, and two that are data between code decoded as instructions |
| `.rdata` | 8 differing blocks (data) |
| `.data` | 369 differing blocks; a comparison of data read as instructions, meaningless, so a dword-level comparison is still needed |

* **A function only the US build has, `0x694A0`**: `mov eax,[esp+4]; cmp eax,0x5E; jae; mov ecx,[ecx]; fld [ecx+eax*4]; ret 4` (the getter of a 0x5E-entry float table). The Japanese build has nops there.
* **A moved function**: US `0xD4440`'s function sits near `0xD4321` in the Japanese build (addresses around it map with `+0x30`); the body looks the same.
* Address map: `.data` moves mostly by `-0x20`, `-0x18` and `-0x8`; only the language global moves by `+0x55834` (into bss).

## 6. Unresolved

* Whether the US build with the language set to 0 matches the Japanese build in menus, subtitles and voice (to be checked by running).
* Whether `.data` holds differences other than relocation (per-language initial tables, ...).
* Who calls the US-only `0x694A0`, and whether it is English-only behaviour.
* When the language is read (once at boot, or per scene); whether it can change while running depends on it.
