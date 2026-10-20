# 작업 지시: 한국어 패치 검토 (issue #105)

검토: [20261020-i105](../design/20261020-i105-korean-patch-review.md)

검토와 문서화만 한다. 게임 코드는 바꾸지 않는다.

1. 문장이 있는 곳 찾기: XBE와 `Media`에서 Shift-JIS·영어 문장 검색(`scripts/analysis/text/find_text.py`).
2. 문장 형식과 분량: 레코드 구조, 제어 코드, 글자 수와 서로 다른 글자 수(`text_records.py`).
3. 글꼴: `Font/jetfont.dat` 형식, 글리프 수, 빈 공간, 글리프 대응표의 위치.
4. 그림 속 글자, 음성: 언어별 텍스처·음성 파일 목록.
5. 문서: 분석 `docs/analysis/text-and-font.md`(색인 포함), 검토 문서의 결론·방안·배포·작업량, 스크립트 README, 작업 로그.

---

# Work order: Korean patch review (issue #105)

Review: [20261020-i105](../design/20261020-i105-korean-patch-review.md)

Review and documentation only; no game code changes.

1. Where the text lives: search the XBE and `Media` for Shift-JIS and English text (`scripts/analysis/text/find_text.py`).
2. Text format and volume: record structure, control codes, character counts and distinct characters (`text_records.py`).
3. Font: the `Font/jetfont.dat` format, glyph count, free space, where the glyph mapping lives.
4. Lettering in pictures and voice: the per-language texture and voice files.
5. Documents: analysis `docs/analysis/text-and-font.md` (with the index), the review's conclusion, options, distribution and effort, the scripts' README, the work log.
