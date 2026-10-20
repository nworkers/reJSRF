# 작업 지시: 게임 언어 선택 (issue #104)

설계: [20261020-i104](../design/20261020-i104-language-option.md)

1. `jsrf_profile.h/.cpp`: `enum class Language { kJapanese, kEnglish, kFrench, kGerman, kSpanish }`(인덱스 0~4), `LanguageName`, `ParseLanguage`, `ApplyLanguage(image, memory, language)`. 기본 variant일 때만 `0x1FC724`에 쓰고 한 줄 기록(`[jsrf] language: Japanese`).
2. `settings.h/.cpp`: `Settings::language`(기본 English), `[Game]` 절 읽기·쓰기(`Language = <이름>`).
3. `main.cpp`: `REJSRF_LANGUAGE`가 있으면 우선(저장 안 함), `LoadXbeIntoGuestMemory` 뒤·진입 전에 `ApplyLanguage`. 실행 중 언어를 메뉴에 넘긴다.
4. `settings_ui.cpp/.h`: Game 탭, `Language` 콤보, 다음 실행부터 적용 안내, 실행 중 언어와 다르면 "Restart to apply.".
5. 검증: Release 빌드, 회귀 도구, 다섯 언어 실행의 파일 열기·프레임 비교(설계 §5), ini 왕복.
6. 문서: 작업 로그, 가이드(`win32-setup-and-build.md`의 설정 설명), `ARCHITECTURE.md`, `docs/analysis/language.md`.

---

# Work order: game language choice (issue #104)

Design: [20261020-i104](../design/20261020-i104-language-option.md)

1. `jsrf_profile.h/.cpp`: `enum class Language { kJapanese, kEnglish, kFrench, kGerman, kSpanish }` (indices 0-4), `LanguageName`, `ParseLanguage`, and `ApplyLanguage(image, memory, language)`, which writes `0x1FC724` for the primary variant only and logs one line (`[jsrf] language: Japanese`).
2. `settings.h/.cpp`: `Settings::language` (default English); read and write the `[Game]` section (`Language = <name>`).
3. `main.cpp`: `REJSRF_LANGUAGE` wins when set (not saved); call `ApplyLanguage` after `LoadXbeIntoGuestMemory` and before the entry; hand the running language to the menu.
4. `settings_ui.cpp/.h`: a Game tab with a `Language` combo, the from-the-next-start note, and "Restart to apply." while the running language differs.
5. Verification: Release build, regression tools, file-open and frame comparison of runs in all five languages (design §5), the ini round trip.
6. Documents: work log, the guide (`win32-setup-and-build.md`'s settings text), `ARCHITECTURE.md`, `docs/analysis/language.md`.
