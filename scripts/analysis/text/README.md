# 텍스트 분석 스크립트 (issue #105)

게임 문장과 글꼴을 조사할 때 쓴 스크립트다. 사용자 덤프를 읽기만 하고 아무것도 쓰지 않는다. 결과는 [docs/analysis/text-and-font.md](../../../docs/analysis/text-and-font.md)에 정리했다. 출력에는 원문 문장이 들어가므로 저장소에 넣지 않는다.

| 스크립트 | 하는 일 | 실행(저장소 루트에서) |
|---|---|---|
| `find_text.py` | XBE와 `Media`(오디오 제외)에서 Shift-JIS 일본어와 영어 문장이 있는 파일을 찾고 표본을 보인다. **4분쯤 걸린다** | `python -I scripts/analysis/text/find_text.py "roms/Jet Set Radio Future" out.txt` |
| `text_records.py` | 이벤트·대화·미션 파일의 문장 레코드(`$e`로 끝나는 문자열)를 세고 일본어 글자 수, 서로 다른 글자 수, 영어 글자 수, 제어 코드를 집계한다 | `python -I scripts/analysis/text/text_records.py "roms/Jet Set Radio Future" out.txt` |

---

# Text analysis scripts (issue #105)

Scripts used to survey the game's text and font. They only read the user's dump and write nothing. Findings are in [docs/analysis/text-and-font.md](../../../docs/analysis/text-and-font.md). Their output contains original text, so it is never committed.

| Script | What it does | Run (from the repo root) |
|---|---|---|
| `find_text.py` | Finds the files in the XBE and `Media` (audio excluded) holding Shift-JIS Japanese and English text, with samples. **Takes about four minutes** | `python -I scripts/analysis/text/find_text.py "roms/Jet Set Radio Future" out.txt` |
| `text_records.py` | Counts the text records (strings ending in `$e`) of the event, talk and mission files: Japanese characters, distinct characters, English letters, control codes | `python -I scripts/analysis/text/text_records.py "roms/Jet Set Radio Future" out.txt` |
