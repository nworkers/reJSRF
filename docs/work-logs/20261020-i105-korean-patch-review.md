# 작업 로그: 한국어 패치 검토 (issue #105)

검토: [20261020-i105](../design/20261020-i105-korean-patch-review.md) · 작업 지시: [20261020-i105](../work-orders/20261020-i105-korean-patch-review.md) · 분석: [text-and-font](../analysis/text-and-font.md)

## 한 일

1. `find_text.py`로 XBE와 `Media`(오디오 제외)를 검색했다. 화면 문장은 `Event/Event/e###.dat`, `Mission/mssn####.bin`, `Event/TalkEvent/TE###.bin`, `default.xbe`에 있다. 다른 파일의 일본어는 제작 경로나 잡음이었다.
2. `e105.dat`를 바이트로 덤프해 `NORM` 레코드와 `$r$e$r`로 끝나는 메시지 구조를 확인했다. `text_records.py`로 레코드 수, 일본어 약 27,000자, 서로 다른 글자 800자를 셌다.
3. `jetfont.dat`를 DXT3로 풀어(alpha만, 폭 512) 글리프 아틀라스임을 확인했다. 약 88줄, 약 1,500개, 아래쪽 약 24줄이 비어 있다. 글리프 순서표를 XBE와 `Media`에서 Shift-JIS·16비트·UTF-16으로 찾았지만 없었다.
4. 언어별 텍스처(`LogoJ`, `Title.dat`, `SprNorm`, `tex_Jpn`, `Mark/*_jpn`)와 음성(`Z_ADX/MUSENJ` 116개, `Sounds/SE/*_j.dat`)을 목록으로 정리했다.
5. 검토 문서에 결론(가능, 런타임 대체 권장), 방안 비교, 배포·권리, 작업량을 적었다.

## 검증

| 검증 | 결과 |
|---|---|
| 문장 위치 | 표본을 CP932로 풀어 화면 문장임을 확인(예: 미션 "“ドーゲン坂”をグラフィティでぬりつぶせ！" / "Cover Dogenzaka Hill in paint!") |
| 글꼴 | alpha 이미지에서 한자·가나·영숫자 글리프를 눈으로 확인 |
| 코드 변경 | 없음(분석 스크립트 두 개만 추가). 빌드 검증 대상 아님 |

## 남은 것

* 글리프 대응, `$` 코드의 뜻, 그림 속 글자의 내용, 자막·음성 시간 관계([text-and-font §5](../analysis/text-and-font.md)).
* 한국어 패치를 별도 프로젝트로 할지(사용자 결정). 하면 첫 작업은 텍스트 렌더러 디컴파일이다.

---

# Work log: Korean patch review (issue #105)

Review: [20261020-i105](../design/20261020-i105-korean-patch-review.md) · Work order: [20261020-i105](../work-orders/20261020-i105-korean-patch-review.md) · Analysis: [text-and-font](../analysis/text-and-font.md)

## Done

1. Searched the XBE and `Media` (audio excluded) with `find_text.py`. On-screen text is in `Event/Event/e###.dat`, `Mission/mssn####.bin`, `Event/TalkEvent/TE###.bin` and `default.xbe`; the Japanese elsewhere was authoring paths or noise.
2. Dumped `e105.dat` to confirm the `NORM` records and the messages ending in `$r$e$r`. Counted the records, about 27,000 Japanese characters and 800 distinct ones with `text_records.py`.
3. Decoded `jetfont.dat` as DXT3 (alpha only, 512 wide) and confirmed a glyph atlas: about 88 rows, about 1,500 glyphs, about 24 empty rows at the bottom. Searched the XBE and `Media` for the glyph order as Shift-JIS, 16-bit and UTF-16, without a hit.
4. Listed the per-language textures (`LogoJ`, `Title.dat`, `SprNorm`, `tex_Jpn`, `Mark/*_jpn`) and voice (`Z_ADX/MUSENJ`, 116 files, `Sounds/SE/*_j.dat`).
5. Wrote the review's conclusion (feasible, runtime replacement recommended), the options, distribution and rights, and the effort.

## Verification

| Check | Result |
|---|---|
| Text locations | samples decoded as CP932 are on-screen text (for example the mission line "“ドーゲン坂”をグラフィティでぬりつぶせ！" / "Cover Dogenzaka Hill in paint!") |
| Font | kanji, kana, digit and letter glyphs seen in the alpha image |
| Code changes | none (two analysis scripts only); no build verification needed |

## Open

* The glyph mapping, the `$` codes' meaning, what the lettering pictures show, how subtitles are timed to voice ([text-and-font §5](../analysis/text-and-font.md)).
* Whether to pursue a Korean patch as a separate project (the user's decision); if so, the first task is decompiling the text renderer.
