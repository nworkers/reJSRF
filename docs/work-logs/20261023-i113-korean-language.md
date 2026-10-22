# 작업 로그: 영어·일본어와 함께 한국어 (issue #113)

설계: [20261023-i113](../design/20261023-i113-korean-language.md) · 작업 지시: [20261023-i113](../work-orders/20261023-i113-korean-language.md)

## 한 일

1. 조사
   * JSRF-kr-patch v0.0.12의 인계 문서와 영문 모드 설계를 읽었다.
   * 패치 결과 XBE(`out/jsrf-kr-en/default.xbe`)를 US판 원본과 섹션별로 비교했다. 코드·라이브러리 섹션은 같고, `.rdata`·`.data`와 새 섹션 `KRTEXT`만 다르다.
   * 캐시 파티션(`Partition5`, Z:)과 저장 파티션(`Partition1`)을 확인했다.
2. 결정(사용자)
   * 한국어 폴더 위치: 명령줄 + ini + 기본 `<게임 폴더>-kr`
   * 캐시: 세트가 바뀌면 비운다. 파티션을 따로 연결하는 rexbox 변경은 Xbox 동작이 아니라는 지적으로 고르지 않았다.
3. 프로파일
   * `Language::kKorean`, `kLanguageCount` 3
   * `LanguageAvailable`(일본판 빌드는 Korean 불가), `GameLanguageIndex`(Korean → 1)
   * `ClassifyXbeText`: US판 섹션 11개의 64비트 FNV-1a 해시 표(해시만)로 원본 / 한국어 패치 / 코드가 다름 / 다른 빌드를 가린다.
4. 앱
   * `--korean <폴더>`, `rejsrf.ini [Game] KoreanGameDirectory`, 기본 `<게임 폴더>-kr`
   * 시작할 때 한국어 폴더의 XBE를 읽어 검사한다. Korean이면 그 XBE와 폴더를 디스크로 쓰고 `[app] Korean: the disc is …`를 낸다. 검사에 실패하면 이유를 남기고 영어로 시작한다.
   * 게임 폴더 자체가 한국어 패치면 Korean으로만 실행한다.
   * 코드가 다른 XBE는 거부한다(종료 코드 1).
   * `PrepareCache`: `<hdd root>/rejsrf_cache_text.txt`의 세트가 다르거나 없으면 `Partition3`~`5`를 비운다.
5. 설정 메뉴 Game 탭
   * Korean 선택지. 쓸 수 없는 언어는 회색과 툴팁으로 이유를 보인다.
   * Korean 절: 폴더, 준비 상태와 이유
6. 문서: 가이드(한국어 폴더 만들기와 쓰는 법), `ARCHITECTURE.md`, `README.md`, 설계 상태

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드(US, JP) | 경고 0 |
| 한국어 실행(`--korean …/out/jsrf-kr-en`, 창 캡처) | 부팅, 메인 메뉴(영어판 그림 글자), New Game 뒤 차고 대화 "어이, 여긴 "GG"의 차고다. 피자 배달치곤 손이 가벼운데?"와 이름 "콘"이 한글로 나온다 |
| 전환(같은 HDD) | 한국어 → 영어: `[app] cache: filled from korean text …; emptied Partition3-5 (266 entries)`, 같은 장면이 영어("This is the GG's Garage…", "Corn")로 나온다. 영어 → 일본어: 캐시를 비우지 않고 일본어("よう、ここは"GG"のガレージだ。…", "コーン")로 나온다. `Partition1`(저장)은 남는다 |
| 거부와 대체 | 코드 1바이트를 바꾼 XBE를 게임 폴더로 주면 종료 코드 1. 같은 것을 `--korean`으로 주면 "changed code" 경고와 영어. 한국어 폴더가 없으면 기본 경로를 알리고 영어. 패치 폴더를 게임 폴더로 주고 Japanese를 요청하면 Korean으로 실행. 일본판 빌드에서 Korean을 요청하면 이유와 Japanese |
| 회귀(새 HDD) | US 영어, US+Japanese, 일본판 빌드 기본의 데모 프레임 300~1800과 파일 열기 목록이 v0.0.46 기준(v0.0.48까지 같음)과 바이트 단위로 같다 |
| 한국어 데모 프레임 | 300~1800이 영어와 같다(데모 구간에는 문장이 없다) |
| 회귀 도구(두 트리) | rexbox probe 94/55/220 실패 0, `rejsrf_decomp_check` 129 차이 0, `rejsrf_d3d8_check` 14,018 차이 0 |
| CI | PR에서 확인 |

* 창 캡처는 scratch 폴더에서 창으로 실행하고 키 메시지(Enter = START)를 보내 찍었다. 게임 화면이 담겨 있어 커밋하지 않았다.
* 설정 메뉴 Game 탭의 새 화면(Korean 선택지, Korean 절)은 캡처하지 않았다(메뉴는 Info 탭으로 열린다). 사용자 확인.

## 남은 것

* Game 탭 화면 확인(사용자).
* 미션 목표, 메뉴 문자열 등 대화 밖의 한국어 화면 확인과 그 표시 문제는 JSRF-kr-patch의 화면 확인 범위다.

---

# Work log: Korean beside English and Japanese (issue #113)

Design: [20261023-i113](../design/20261023-i113-korean-language.md) · Work order: [20261023-i113](../work-orders/20261023-i113-korean-language.md)

## Done

1. Survey
   * Read JSRF-kr-patch v0.0.12's handoff and English-mode design.
   * Compared the patched XBE (`out/jsrf-kr-en/default.xbe`) with the US original section by section: the code and library sections are equal; only `.rdata`, `.data` and the new `KRTEXT` section differ.
   * Located the cache partition (`Partition5`, Z:) and the save partition (`Partition1`).
2. Decisions (user)
   * The Korean folder: command line + ini + default `<game folder>-kr`.
   * The cache is emptied when the set changes. A rexbox change mapping partitions elsewhere was not chosen, after the user's point that it reproduces no Xbox behavior.
3. Profile
   * `Language::kKorean`, `kLanguageCount` 3.
   * `LanguageAvailable` (no Korean in the Japanese build) and `GameLanguageIndex` (Korean → 1).
   * `ClassifyXbeText`: a table of 64-bit FNV-1a hashes of the US build's 11 sections (hashes only) tells original, Korean patch, changed code and another build apart.
4. App
   * `--korean <folder>`, `rejsrf.ini [Game] KoreanGameDirectory`, default `<game folder>-kr`.
   * At start the Korean folder's XBE is read and checked. For Korean, that XBE and folder become the disc, with `[app] Korean: the disc is …`. A failed check records why and starts in English.
   * A game folder that is itself the Korean patch runs in Korean only.
   * An XBE with changed code is refused (exit code 1).
   * `PrepareCache`: when the set in `<hdd root>/rejsrf_cache_text.txt` differs or is missing, `Partition3`-`5` are emptied.
5. Settings menu Game tab
   * The Korean choice; unavailable languages are greyed out with the reason as a tooltip.
   * A Korean section with the folder, its readiness and the reason.
6. Documents: the guide (making and using the Korean folder), `ARCHITECTURE.md`, `README.md`, the design's status.

## Verification

| Check | Result |
|---|---|
| Release builds (US, JP) | zero warnings |
| Korean run (`--korean …/out/jsrf-kr-en`, window captures) | boots; the main menu (English picture lettering); after New Game the garage line "어이, 여긴 "GG"의 차고다. 피자 배달치곤 손이 가벼운데?" and the name "콘" show in Hangul |
| Switching (one HDD) | Korean → English: `[app] cache: filled from korean text …; emptied Partition3-5 (266 entries)`, and the same scene in English ("This is the GG's Garage…", "Corn"). English → Japanese: the cache is kept, and the scene is Japanese ("よう、ここは"GG"のガレージだ。…", "コーン"). `Partition1` (saves) remains |
| Refusals and fallbacks | an XBE with one code byte changed as the game folder: exit code 1; the same as `--korean`: a "changed code" warning and English; no Korean folder: the default path named and English; the patch folder as the game folder with Japanese asked: runs Korean; Korean asked in the Japanese build: the reason and Japanese |
| Regression (fresh HDDs) | the demo frames 300-1800 and file-open lists of US English, US + Japanese and the Japanese build's default equal the v0.0.46 baselines (unchanged through v0.0.48) byte for byte |
| Korean demo frames | 300-1800 equal English (the demo stretch has no text) |
| Regression tools (both trees) | rexbox probes 94/55/220 with 0 failures, `rejsrf_decomp_check` 129 with 0 differences, `rejsrf_d3d8_check` 14,018 with 0 differences |
| CI | checked on the pull request |

* The window captures came from visible runs in a scratch folder driven by key messages (Enter = START). They show game pictures and are not committed.
* The settings menu's new Game tab content (the Korean choice and section) was not captured (the menu opens on the Info tab); user check.

## Open

* The Game tab screen (user check).
* Korean screens beyond dialogue (mission objectives, menu strings) and their display issues are within JSRF-kr-patch's screen checks.
