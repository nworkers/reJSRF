# 언어 분석 스크립트 (issue #104)

US판과 일본판 XBE의 언어 처리를 비교할 때 쓴 스크립트다. 모두 사용자 덤프를 읽기만 하고 아무것도 쓰지 않는다. 결과는 [docs/analysis/language.md](../../../docs/analysis/language.md)에 정리했다.

| 스크립트 | 하는 일 | 실행(저장소 루트에서) |
|---|---|---|
| `xbe_strings.py` | 두 XBE의 출력 가능 문자열 비교, 파일 이름·언어 관련 문자열 목록 | `python -I scripts/analysis/language/xbe_strings.py <us.xbe> <jp.xbe>` |
| `find_lang.py` | 언어 접미사(`_eng`, `_Jpn` ...) 자산 문자열과 그것을 고르는 코드 위치. `gen/jsrf_us/disasm`이 있어야 한다 | `python -I scripts/analysis/language/find_lang.py <us.xbe>` |
| `read_lang_var.py` | 언어 전역의 초기값(US `0x1FC724`)과 일본판의 대응 전역, 섹션 헤더 비교 | `python -I scripts/analysis/language/read_lang_var.py <us.xbe> <jp.xbe>` |
| `code_diff.py` | 실행 섹션을 디스어셈블해 주소 이동을 정규화한 구조 비교, 주소 대응표. **10분 넘게 걸린다** | `python -I scripts/analysis/language/code_diff.py <us.xbe> <jp.xbe> > diff.txt` |

`<us.xbe>`는 `roms/Jet Set Radio Future/default_eng.xbe`(=`default.xbe`), `<jp.xbe>`는 `default_jpn.xbe`다.

---

# Language analysis scripts (issue #104)

Scripts used to compare how the US and Japanese XBEs handle language. They only read the user's dump and write nothing. Findings are in [docs/analysis/language.md](../../../docs/analysis/language.md).

| Script | What it does | Run (from the repo root) |
|---|---|---|
| `xbe_strings.py` | Compares printable strings of the two XBEs; lists file-name and language strings | `python -I scripts/analysis/language/xbe_strings.py <us.xbe> <jp.xbe>` |
| `find_lang.py` | Language-suffixed asset strings (`_eng`, `_Jpn` ...) and the code that picks them; needs `gen/jsrf_us/disasm` | `python -I scripts/analysis/language/find_lang.py <us.xbe>` |
| `read_lang_var.py` | The language global's initial value (US `0x1FC724`), the Japanese build's counterpart, section headers side by side | `python -I scripts/analysis/language/read_lang_var.py <us.xbe> <jp.xbe>` |
| `code_diff.py` | Structural diff of the executable sections with relocated addresses normalized, plus the address map. **Takes over ten minutes** | `python -I scripts/analysis/language/code_diff.py <us.xbe> <jp.xbe> > diff.txt` |

`<us.xbe>` is `roms/Jet Set Radio Future/default_eng.xbe` (= `default.xbe`), `<jp.xbe>` is `default_jpn.xbe`.
