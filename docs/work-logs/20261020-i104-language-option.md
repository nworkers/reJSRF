# 작업 로그: 게임 언어 선택 (issue #104)

설계: [20261020-i104](../design/20261020-i104-language-option.md) · 작업 지시: [20261020-i104](../work-orders/20261020-i104-language-option.md) · 분석: [language](../analysis/language.md)

## 한 일

1. 분석(구현 전): `.data`·`.rdata` dword 비교(`data_diff.py`), `0x694A0`의 일본판 대응(`0x69690`), 언어 인덱스를 읽는 191곳 분류(`lang_reads.py`), 임시 진단 코드로 `0x1FC724`=0 실행 시험. 결과는 [language §5~§7](../analysis/language.md). 임시 코드는 커밋하지 않았다.
2. 범위 변경(사용자 결정): English/Japanese에서 다섯 언어(English, Japanese, French, German, Spanish)로 넓히고, 설정은 새 Game 탭에 둔다.
3. 타이틀 프로파일(`jsrf_profile.h/.cpp`): `enum class Language`(0~4), `LanguageName`, `ParseLanguage`(대소문자 무시), `ApplyLanguage`(기본 variant만 `0x1FC724`에 쓰고 `[jsrf] language: <이름>` 출력).
4. 설정(`settings.h/.cpp`): `Settings::language`(기본 English), `rejsrf.ini`의 `[Game]` 절 `Language` 읽기·쓰기. 모르는 값은 기본값.
5. `main.cpp`: `REJSRF_LANGUAGE`가 있으면 그 실행에만 우선(저장 안 함, 모르는 값은 경고 후 ini 값). XBE를 올린 직후·진입 전에 `ApplyLanguage`. 실행 중 언어를 메뉴에 넘긴다.
6. Game 탭(`settings_ui.cpp/.h`, Info 바로 다음, 사용자 요청): `Language` 콤보(프로파일 순서), 도움말, 아래 줄에 "Applies from the next start." / 실행 중 언어와 다르면 "Restart to apply. Running in <언어>." / `REJSRF_LANGUAGE`가 있으면 그 사실.
7. 문서: 설계 확정, 작업 지시, 분석 §8(다섯 언어 실행), 가이드, `ARCHITECTURE.md`(실행 흐름, 타이틀 설정 계약).

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드 | 경고·오류 없음(`gen/`은 xboxrecomp v0.15로 다시 생성) |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` / `rexbox_d3d8_probe` | 56 / 48 / 163 check, 0 failure |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 129 / 14018 case, 0 difference |
| 설정 없음(기본 English) | 파일 열기 168개와 프레임 300~2400이 언어 설정 없던 빌드의 실행(분석 단계의 언어 1 실행)과 같다. 3000은 빌드가 바뀔 때의 실시간 진행 차이로 어긋난다 |
| `REJSRF_LANGUAGE=English` | 설정 없음과 파일 열기·프레임 모두 같다 |
| `Japanese` | `LogoJ`, `Title.dat/.bin`, `se_ppl_j`, `SprNorm`; 로고·경고문·타이틀이 일본판 |
| `French` / `German` / `Spanish` | 각각 `LogoF/G/S`, `TitleF/G/S`, `SprNorm2/3/4`를 연다(`Z:` 캐시 복사 포함, 모두 성공). 처음 55초 화면은 영어와 같다. 디스크의 해당 자산이 영어와 같기 때문이다([language §8](../analysis/language.md)) |
| ini 읽기 | `Language = japanese` → Japanese, `Klingon` → English. 읽기만으로 파일이 바뀌지 않는다 |
| `REJSRF_LANGUAGE` | `GERMAN`이 ini의 Japanese보다 우선, `korean`은 경고 후 ini 값. 저장되지 않는다(ini 없는 실행 뒤에도 파일이 생기지 않음) |
| Game 탭 화면, 탭에서 바꾼 값의 저장 | 사용자 확인 대기(입력 자동화 수단이 없다) |
| 메뉴·게임 중 자막이 그 언어로 나오는지 | 사용자 확인 대기(Press Start 이후는 입력이 필요하다) |

## 남은 것

* 저장 데이터에 언어가 들어 있는지(설계 §3.4, 미확정).
* 한국어 패치 검토(#105)는 일본어 실행을 출발점으로 한다.

---

# Work log: game language choice (issue #104)

Design: [20261020-i104](../design/20261020-i104-language-option.md) · Work order: [20261020-i104](../work-orders/20261020-i104-language-option.md) · Analysis: [language](../analysis/language.md)

## Done

1. Analysis (before implementing): dword diff of `.data` and `.rdata` (`data_diff.py`), the Japanese counterpart of `0x694A0` (`0x69690`), classification of the 191 reads of the language index (`lang_reads.py`), and a run test with `0x1FC724` = 0 through temporary diagnostic code. Results in [language §5-§7](../analysis/language.md); the temporary code was not committed.
2. Scope change (the user's decision): from English/Japanese to five languages (English, Japanese, French, German, Spanish), with the setting in a new Game tab.
3. Title profile (`jsrf_profile.h/.cpp`): `enum class Language` (0-4), `LanguageName`, `ParseLanguage` (case-insensitive), `ApplyLanguage` (writes `0x1FC724` for the primary variant only and prints `[jsrf] language: <name>`).
4. Settings (`settings.h/.cpp`): `Settings::language` (default English); `Language` in `rejsrf.ini`'s `[Game]` section, read and written; unknown values keep the default.
5. `main.cpp`: `REJSRF_LANGUAGE`, when set, wins for that run (not saved; an unknown value warns and keeps the ini's). `ApplyLanguage` right after the XBE is placed, before the entry. The running language goes to the menu.
6. Game tab (`settings_ui.cpp/.h`, right after Info at the user's request): a `Language` combo (profile order), a tooltip, and below it "Applies from the next start." / "Restart to apply. Running in <language>." while the running language differs / a note when `REJSRF_LANGUAGE` is set.
7. Documents: design settled, work order, analysis §8 (runs in all five languages), the guide, `ARCHITECTURE.md` (execution flow, the title-settings contract).

## Verification

| Check | Result |
|---|---|
| Release build | no warnings or errors (`gen/` regenerated with xboxrecomp v0.15) |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` / `rexbox_d3d8_probe` | 56 / 48 / 163 checks, 0 failures |
| `rejsrf_decomp_check` / `rejsrf_d3d8_check` | 129 / 14018 cases, 0 differences |
| No setting (default English) | the 168 file opens and frames 300-2400 equal a run of the build without the setting (the analysis-stage language-1 run); 3000 drifts with the real-time progress that changes between builds |
| `REJSRF_LANGUAGE=English` | file opens and frames all equal the no-setting run |
| `Japanese` | `LogoJ`, `Title.dat/.bin`, `se_ppl_j`, `SprNorm`; logo, warning and title as in the Japanese build |
| `French` / `German` / `Spanish` | open `LogoF/G/S`, `TitleF/G/S`, `SprNorm2/3/4` respectively (including the `Z:` cache copies, all successful). The first 55 s look like English, because those assets on the disc equal English's ([language §8](../analysis/language.md)) |
| ini read | `Language = japanese` → Japanese, `Klingon` → English; reading never rewrites the file |
| `REJSRF_LANGUAGE` | `GERMAN` wins over the ini's Japanese; `korean` warns and keeps the ini's; never saved (no file appears after a run without an ini) |
| The Game tab on screen, saving a value changed there | waiting for the user (no input automation) |
| Menus and in-game subtitles in the chosen language | waiting for the user (past Press Start needs input) |

## Open

* Whether save data records the language (design §3.4, unresolved).
* The Korean patch review (#105) starts from the Japanese run.
