# 작업 지시: 게임 언어 선택 (issue #104)

설계: [20261020-i104](../design/20261020-i104-language-option.md)

1. `jsrf_profile.h/.cpp`: `enum class Language { kJapanese = 0, kEnglish = 1 }`, `LanguageName`, `ParseLanguage`, `ApplyLanguage(image, memory, language)`. 기본 variant일 때만 `0x1FC724`에 쓰고 한 줄 기록(`[jsrf] language: Japanese`).
2. `settings.h/.cpp`: `Settings::language`(기본 English), `[Game]` 절 읽기·쓰기(`Language = English|Japanese`).
3. `main.cpp`: `REJSRF_LANGUAGE`가 있으면 우선(저장 안 함), `LoadXbeIntoGuestMemory` 뒤·진입 전에 `ApplyLanguage`. 실행 중 언어를 메뉴에 넘긴다.
4. `settings_ui.cpp/.h`: Info 바로 다음에 Game 탭, `Language` 콤보, 다음 실행부터 적용 안내, 실행 중 언어와 다르면 "Restart to apply.".
5. 검증: Release 빌드, 회귀 도구, English/Japanese 실행의 파일 열기·프레임 비교(설계 §5), ini 왕복, 빠진 언어 이름 처리.
6. 문서: 작업 로그, 가이드(`win32-setup-and-build.md`의 설정 설명), `ARCHITECTURE.md`, `docs/analysis/language.md`.

경과: 한때 다섯 언어(French, German, Spanish 포함)로 구현했다가 두 언어로 줄였다. 이유는 설계 §3.4.

---

# Work order: game language choice (issue #104)

Design: [20261020-i104](../design/20261020-i104-language-option.md)

1. `jsrf_profile.h/.cpp`: `enum class Language { kJapanese = 0, kEnglish = 1 }`, `LanguageName`, `ParseLanguage`, and `ApplyLanguage(image, memory, language)`, which writes `0x1FC724` for the primary variant only and logs one line (`[jsrf] language: Japanese`).
2. `settings.h/.cpp`: `Settings::language` (default English); read and write the `[Game]` section (`Language = English|Japanese`).
3. `main.cpp`: `REJSRF_LANGUAGE` wins when set (not saved); call `ApplyLanguage` after `LoadXbeIntoGuestMemory` and before the entry; hand the running language to the menu.
4. `settings_ui.cpp/.h`: a Game tab right after Info with a `Language` combo, the from-the-next-start note, and "Restart to apply." while the running language differs.
5. Verification: Release build, regression tools, file-open and frame comparison of English and Japanese runs (design §5), the ini round trip, handling of the dropped language names.
6. Documents: work log, the guide (`win32-setup-and-build.md`'s settings text), `ARCHITECTURE.md`, `docs/analysis/language.md`.

History: implemented for a while with five languages (French, German and Spanish included), then narrowed to two; the reasons are in design §3.4.
