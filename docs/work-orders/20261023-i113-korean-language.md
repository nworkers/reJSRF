# 작업 지시: 영어·일본어와 함께 한국어 (issue #113)

설계: [20261023-i113](../design/20261023-i113-korean-language.md)

1. 프로파일(`jsrf_profile`)
   * `Language`에 `Korean`. 게임에 쓰는 인덱스는 따로 `GameLanguageIndex`로 정한다(Korean → 1).
   * US판 섹션 해시 표(64비트 FNV-1a, 실행 섹션과 `.rdata`·`.data`)와 `ClassifyKoreanXbe`(원본 / 한국어 패치 / 코드가 다름 / 지원하지 않음)
   * 일본판 빌드는 Korean을 지원하지 않는다(`LanguageAvailable`).
2. 앱
   * 명령줄 `--korean <폴더>`, `rejsrf.ini [Game] KoreanGameDirectory`, 기본 `<게임 폴더>-kr`
   * 시작 때 한국어 폴더 검사. 실패하면 영어로 시작하고 이유를 남긴다.
   * 패치된 폴더를 게임 폴더로 바로 주면 Korean으로 실행하고, 일본어는 막는다.
   * HDD 캐시 표시 파일 `rejsrf_cache_text.txt`. 세트가 바뀌면 `Partition3`~`5`를 비운다.
3. 설정 메뉴 Game 탭: Korean 선택지, 쓸 수 없을 때의 이유, 한국어 폴더 경로 표시.
4. 검증(설계 §5).
5. 문서: 작업 로그, `ARCHITECTURE.md`(타이틀 설정), 가이드(한국어 사용법), 설계 i104의 언어 목록.

---

# Work order: Korean beside English and Japanese (issue #113)

Design: [20261023-i113](../design/20261023-i113-korean-language.md)

1. Profile (`jsrf_profile`)
   * `Language` gains `Korean`; the index written into the game comes from `GameLanguageIndex` (Korean → 1).
   * A table of the US build's section hashes (64-bit FNV-1a; the executable sections, `.rdata` and `.data`) and `ClassifyKoreanXbe` (original / Korean patch / code differs / unsupported).
   * The Japanese build does not support Korean (`LanguageAvailable`).
2. App
   * The command line `--korean <folder>`, `rejsrf.ini [Game] KoreanGameDirectory`, default `<game folder>-kr`.
   * The Korean folder is checked at start; on failure the run starts in English and records why.
   * A patched folder given as the game folder runs as Korean, with Japanese blocked.
   * The HDD cache marker `rejsrf_cache_text.txt`; a changed set empties `Partition3`-`5`.
3. Settings menu Game tab: the Korean choice, why it is unavailable, the Korean folder path.
4. Verification (design §5).
5. Documents: work log, `ARCHITECTURE.md` (title settings), the guide (using Korean), the language list of design i104.
