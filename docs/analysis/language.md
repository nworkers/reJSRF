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
* **확인됨**: US판에서 로드 직후·진입 전에 `0x1FC724`를 0으로 쓰면 일본어로 동작한다(50초 실행). 파일 열기 168개 중 6개가 일본어 자산으로 바뀌었다: `Title\LogoJ.dat`(영어 `LogoE`), `Title\Title.dat/.bin`(`TitleE`), `Sounds\SE\se_ppl_j.dat`(`se_pple`), `Disp\SprNorm.bin/.dat`(`SprNorm1`). SEGA 로고, 낙서 경고문, 타이틀 로고가 일본판으로 나왔고 진단 출력에 새 경고·오류는 없었다.

## 5. 코드 차이 (확인됨, `code_diff.py`)

실행 섹션을 디스어셈블하고 이미지 주소를 정규화해 비교했다.

| 섹션 | 결과 |
|---|---|
| D3D, DSOUND, MMATRIX, XGRPH, XPP, DOLBY | 같음 |
| `.text` | 다른 블록 6개. 실제 차이는 아래 둘이고, 나머지 둘은 코드 사이 자료를 디코드한 잡음이다 |
| `.rdata` | 다른 블록 8개(자료) |
| `.data` | 다른 블록 369개. 자료를 명령으로 읽은 비교라 의미가 없고, dword 단위 비교가 따로 필요하다 |

* **함수 `0x694A0`는 배치만 다르다**(확인됨): `mov eax,[esp+4]; cmp eax,0x5E; jae; mov ecx,[ecx]; fld [ecx+eax*4]; ret 4`(0x5E칸 float 표의 getter). 처음에는 US 전용으로 보았으나, 일본판은 같은 함수를 `0x69690`에 두고, 두 빌드 모두 호출하는 `call`이 91곳으로 같다(같은 자리에서 같은 표 객체, US `0x251F5C` / 일본판 `0x251F3C`를 넘긴다). 언어와 관계없는 링커 배치 차이다.
* **옮겨진 함수**: US `0xD4440`의 함수가 일본판에서는 `0xD4321` 근처로 옮겨졌다(그 부근 주소 대응이 `+0x30`). 내용은 같아 보인다.
* 주소 대응: `.data`는 대부분 `-0x20`·`-0x18`·`-0x8`만큼, 언어 전역만 `+0x55834`(bss로 옮겨짐) 이동했다.

## 6. 언어별 초기값은 언어 인덱스 하나다 (확인됨, `data_diff.py`)

`.data`와 `.rdata`를 dword 열로 읽고, 이미지 안을 가리키는 값(포인터)은 같은 토큰으로 바꾼 뒤 정렬해 비교했다.

| 섹션 | 결과 |
|---|---|
| `.rdata` | 같음 |
| `.data` | 다른 블록 2개뿐 |

* US `0x1FC720`~`0x1FC727`(0, 1): 언어 인덱스(`0x1FC724` = 1)와 그 앞 0. 일본판은 이 객체가 bss에 있어 `.data`에서 빠졌다.
* US `0x215384`부터 0 6개: 함수 포인터 표 `0x215364`(8칸, `sub_000A2640`이 `call [eax*4+0x215364]`로 부른다) 뒤의 0 채움이다. 일본판은 같은 자리에 0이 2개뿐이고 바로 다음 자료가 온다. 코드와 자료 어디에서도 이 6개를 가리키지 않으므로 정렬 패딩으로 본다(**추정**).
* 언어 객체는 두 필드다: `+0` 언어 인덱스, `+4`는 `sub_000149D0`(`mov eax,[ecx+4]`)로 읽고 `0x18AEBF`가 쓰는 필드. `+4`의 초기값은 두 빌드 모두 0이다(US `0x1FC728` `.data`, 일본판 `0x251F5C` bss).
* 결론: US판과 일본판이 다르게 시작하는 값은 언어 인덱스 하나다.

## 7. 언어 값을 읽는 곳 (확인됨, `lang_reads.py`)

US판의 `mov ecx, 0x1FC724` 191곳을 모두 분류했다(81개 함수). 189곳은 getter `sub_00043700`을 부르고, 2곳은 다른 접근이다(`0x7E703`은 `sub_000149D0`으로 `+4` 필드를 읽고, `0x1289BB`는 뒤의 공통 getter 호출로 점프한다).

| 읽은 값을 쓰는 방식 | 곳 |
|---|---|
| 언어별 표의 인덱스(`[eax*4 + 표]`) | 92 |
| 비교·분기(`cmp`/`test`/switch) | 26 |
| 몇 명령 뒤에 인덱스·인자로 씀(기타) | 71 |
| 객체 필드에 저장 | 2 |

* 대부분은 **쓸 때마다 다시 읽는다**. 부팅 때 한 번 읽어 전역에 저장하는 곳은 없다.
* 객체 필드에 저장해 두는 곳이 세 군데다: `[obj+0x74C]`(`sub_00028BAD`, `sub_00038460`), `[obj+0xB0]`(`sub_0007E6A0`, vtable `0x1CCFB8` 객체의 생성자). 이 객체들은 만들 때의 언어를 계속 쓴다.
* 언어별 자산(텍스처, 목록 파일)은 장면을 읽을 때 열린다. 이미 읽은 자산은 언어를 바꿔도 그대로다(**추정**).
* 따라서 실행 중 언어 전환은 안전하지 않다. **진입 전에 쓰고 다음 실행부터 적용**한다.

## 8. 다섯 언어 실행 (확인됨, issue #104 구현 후)

`REJSRF_LANGUAGE`로 다섯 언어를 각각 55초 실행해(숨긴 창, 같은 빌드) 파일 열기와 프레임 300·600·1500·1800·2400·3000을 비교했다.

| 언어 | 영어와 다르게 여는 파일 | 화면(처음 55초) |
|---|---|---|
| 영어(1) | `LogoE`, `TitleE.dat/.bin`, `se_pple`, `SprNorm1.bin/.dat` | 기준. 설정 없는 실행과 파일 열기·프레임이 같다 |
| 일본어(0) | `LogoJ`, `Title.dat/.bin`, `se_ppl_j`, `SprNorm.bin/.dat` | 로고·경고문·타이틀이 일본판 |
| 프랑스어(2) | `LogoF`, `TitleF.dat/.bin`, `SprNorm2.bin/.dat` | 영어와 같다 |
| 독일어(3) | `LogoG`, `TitleG.dat/.bin`, `SprNorm3.bin/.dat` | 영어와 같다(프레임 3000은 데모 진행 시각만 조금 다르다) |
| 스페인어(4) | `LogoS`, `TitleS.dat/.bin`, `SprNorm4.bin/.dat` | 영어와 같다 |

* 유럽 세 언어의 처음 화면이 영어와 같은 것은 자산이 같기 때문이다(디스크 MD5): `LogoE` = `LogoF` = `LogoG` = `LogoS`, 모든 `Title*.bin`이 같다, `SprNorm1~4.bin`이 같다. 다른 것은 `TitleF/G/S.dat`(셋은 서로 같고 `TitleE.dat`와 다르다)와 `SprNorm2/3/4.dat`(셋은 서로 같고 `SprNorm1.dat`와 다르다)이다.
* 유럽 세 언어는 `SprNormN`을 `Z:`(유틸리티 파티션)에 복사해 두고 쓴다(`Z:` 열기 실패 → `D:`에서 읽기 → `Z:\...~`에 쓰고 이름 바꾸기). 영어·일본어 파일은 이전 실행에서 이미 복사되어 있었다. 게임 자체의 캐시 동작이며 모두 성공했다.
* 음성 효과음 `se_ppl_*`는 영어(`se_pple`)와 일본어(`se_ppl_j`)만 따로 연다. 유럽 세 언어는 영어 효과음을 쓰는 것으로 보인다(**추정**).

## 9. 미확정

* 로고·경고문·타이틀 너머(메뉴, 게임 중 자막·음성)도 일본판과 같은지, 유럽 세 언어의 메뉴·자막이 그 언어로 나오는지. 입력이 필요해 아직 보지 않았다.
* `TitleF/G/S.dat`와 `TitleE.dat`의 차이가 화면 어디에 나오는지(처음 55초 프레임에는 보이지 않는다).
* `[obj+0x74C]`와 `[obj+0xB0]`을 가진 객체가 무엇인지.

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
* **Confirmed**: writing 0 to `0x1FC724` after load and before the entry makes the US build run in Japanese (a 50 s run). Of 168 file opens, six became Japanese assets: `Title\LogoJ.dat` (English `LogoE`), `Title\Title.dat/.bin` (`TitleE`), `Sounds\SE\se_ppl_j.dat` (`se_pple`), `Disp\SprNorm.bin/.dat` (`SprNorm1`). The SEGA logo, the graffiti warning and the title logo came out as in the Japanese build, and diagnostics showed no new warnings or errors.

## 5. Code differences (confirmed, `code_diff.py`)

The executable sections were disassembled with image addresses normalized, then compared.

| Section | Result |
|---|---|
| D3D, DSOUND, MMATRIX, XGRPH, XPP, DOLBY | identical |
| `.text` | 6 differing blocks: the two real differences below, and two that are data between code decoded as instructions |
| `.rdata` | 8 differing blocks (data) |
| `.data` | 369 differing blocks; a comparison of data read as instructions, meaningless, so a dword-level comparison is still needed |

* **Function `0x694A0` is only placed differently** (confirmed): `mov eax,[esp+4]; cmp eax,0x5E; jae; mov ecx,[ecx]; fld [ecx+eax*4]; ret 4` (the getter of a 0x5E-entry float table). It first looked US-only, but the Japanese build has the same function at `0x69690`, and both builds call it from 91 `call`s (the same sites, passing the same table object, US `0x251F5C` / Japanese `0x251F3C`). A linker placement difference, unrelated to language.
* **A moved function**: US `0xD4440`'s function sits near `0xD4321` in the Japanese build (addresses around it map with `+0x30`); the body looks the same.
* Address map: `.data` moves mostly by `-0x20`, `-0x18` and `-0x8`; only the language global moves by `+0x55834` (into bss).

## 6. The only per-language initial value is the language index (confirmed, `data_diff.py`)

`.data` and `.rdata` were read as dword streams, values pointing into the image (pointers) replaced by one token, then aligned and compared.

| Section | Result |
|---|---|
| `.rdata` | identical |
| `.data` | only 2 differing blocks |

* US `0x1FC720`-`0x1FC727` (0, 1): the language index (`0x1FC724` = 1) and the zero before it. In the Japanese build that object is in bss, so it is missing from `.data`.
* Six zeros from US `0x215384`: the zero fill after the function-pointer table `0x215364` (8 entries, called by `sub_000A2640` as `call [eax*4+0x215364]`). The Japanese build has only two zeros there before the next data. Nothing in code or data points at those six, so they are taken as alignment padding (**inferred**).
* The language object has two fields: `+0` the language index, and `+4`, read by `sub_000149D0` (`mov eax,[ecx+4]`) and written at `0x18AEBF`. `+4` starts at 0 in both builds (US `0x1FC728` in `.data`, Japanese `0x251F5C` in bss).
* Conclusion: the language index is the only value the US and Japanese builds start with differently.

## 7. Where the language is read (confirmed, `lang_reads.py`)

All 191 `mov ecx, 0x1FC724` in the US build were classified (81 functions). 189 call the getter `sub_00043700`; two are other accesses (`0x7E703` reads the `+4` field through `sub_000149D0`, and `0x1289BB` jumps to a shared getter call further on).

| What the read value is used for | Sites |
|---|---|
| Index into a per-language table (`[eax*4 + table]`) | 92 |
| Compare or branch (`cmp`/`test`/switch) | 26 |
| Index or argument a few instructions later (other) | 71 |
| Stored into an object field | 2 |

* Most sites **read it again every time they need it**. No site reads it once at boot into a global.
* Three places keep a copy in an object: `[obj+0x74C]` (`sub_00028BAD`, `sub_00038460`) and `[obj+0xB0]` (`sub_0007E6A0`, the constructor of the object with vtable `0x1CCFB8`). Those objects keep the language they were created with.
* Per-language assets (textures, list files) are opened when a scene loads; assets already loaded stay as they are when the language changes (**inferred**).
* So switching while running is not safe. **Write it before the entry, and apply it from the next start.**

## 8. Runs in all five languages (confirmed, after the issue #104 implementation)

Each language was run for 55 s through `REJSRF_LANGUAGE` (hidden window, the same build), comparing file opens and frames 300, 600, 1500, 1800, 2400 and 3000.

| Language | Files it opens instead of English's | Screens (first 55 s) |
|---|---|---|
| English (1) | `LogoE`, `TitleE.dat/.bin`, `se_pple`, `SprNorm1.bin/.dat` | the reference; file opens and frames equal a run without the setting |
| Japanese (0) | `LogoJ`, `Title.dat/.bin`, `se_ppl_j`, `SprNorm.bin/.dat` | logo, warning and title as in the Japanese build |
| French (2) | `LogoF`, `TitleF.dat/.bin`, `SprNorm2.bin/.dat` | the same as English |
| German (3) | `LogoG`, `TitleG.dat/.bin`, `SprNorm3.bin/.dat` | the same as English (frame 3000 differs only in the demo's timing) |
| Spanish (4) | `LogoS`, `TitleS.dat/.bin`, `SprNorm4.bin/.dat` | the same as English |

* The three European languages start out looking like English because the assets are the same (disc MD5s): `LogoE` = `LogoF` = `LogoG` = `LogoS`, all `Title*.bin` are equal, `SprNorm1-4.bin` are equal. What differs is `TitleF/G/S.dat` (equal to each other, not to `TitleE.dat`) and `SprNorm2/3/4.dat` (equal to each other, not to `SprNorm1.dat`).
* The European languages copy `SprNormN` to `Z:` (the utility partition) and use the copy (`Z:` open fails → read from `D:` → write `Z:\...~` and rename). English's and Japanese's files had been copied by earlier runs. It is the game's own caching, and every step succeeded.
* The voice effect `se_ppl_*` is opened per language only for English (`se_pple`) and Japanese (`se_ppl_j`); the European languages seem to use the English one (**inferred**).

## 9. Unresolved

* Whether everything past the logos, warning and title (menus, in-game subtitles and voice) matches the Japanese build, and whether the European languages' menus and subtitles come out in their language; it needs input and has not been looked at yet.
* Where on screen `TitleF/G/S.dat` differs from `TitleE.dat` (nothing in the first 55 s of frames shows it).
* What the objects holding `[obj+0x74C]` and `[obj+0xB0]` are.
