# 검토(초안): 한국어 패치 (issue #105)

상태: **검토 시작 전**. 범위와 출발점만 정리했다. 이 issue는 검토와 문서화만 하며, 구현은 필요하면 별도 프로젝트로 한다(사용자 결정).

## 1. 범위

* 한국어 패치를 만들 수 있는지, 어떤 방법이 있는지, 무엇이 막히는지를 정리한다.
* 결과물은 이 문서와 `docs/analysis/`의 분석 사실이다. 코드는 만들지 않는다.

## 2. 출발점 (#104 분석에서 나온 것)

* 게임은 언어 인덱스(US `0x1FC724`)로 자산을 고른다. 일본어(0)는 2바이트 문자 텍스트를 이미 다룬다. 한국어도 2바이트 문자이므로 **일본어 경로가 출발점으로 유력하다**(추정).
* 언어별 자산: `Media\Ending\tex_*.dat`, `Media\Mark\List_*.{bin,dat}`, `Media\Mark\tex_*.dat`, `_j` 접미사 파일들, `Media\Font\`([language.md](../analysis/language.md) §2).

## 3. 검토할 질문

| 질문 | 확인 방법 |
|---|---|
| 텍스트는 어디에 있나(자산 파일, XBE 안 문자열) | 일본어 실행 중 파일 열기 추적, 자산 형식 분석 |
| 텍스트 인코딩(Shift-JIS인지 자체 코드표인지) | 일본어 텍스트 자산의 바이트 분석 |
| 글꼴: 비트맵 아틀라스인지, 글리프 수 한도, 한글 2,350자(KS X 1001) 또는 사용 글자만으로 충분한지 | `Media\Font` 형식 분석, 렌더러 함수 디컴파일 |
| 그림 속 글자(로고, 메뉴 이미지, 낙서 이름) | 텍스처 목록과 언어별 텍스처 |
| 음성·자막 타이밍 | 자막이 음성에 묶여 있는지 |
| 배포 방법 | 게임 자산을 담지 않는 방식이어야 한다: 사용자 덤프에 적용하는 차분 도구, 또는 실행 중 자산 대체 폴더(`rexbox`의 파일 계층) |
| 라이선스·권리 | 번역문 자체의 권리, 원작 자산 비포함 원칙 |

## 4. 다음 단계

* #104에서 일본어 실행이 되면, 같은 실행에서 텍스트 자산과 글꼴 경로를 추적해 §3을 채운다.
* 판단(가능/불가능, 권장 방법, 작업량)을 이 문서에 적고 별도 프로젝트 여부를 사용자와 정한다.

---

# Review (draft): a Korean patch (issue #105)

Status: **review not started**; only the scope and the starting points are written down. This issue is review and documentation only; implementation, if wanted, is a separate project (the user's decision).

## 1. Scope

* Whether a Korean patch can be made, the possible ways, and what blocks them.
* The output is this document and analysis facts under `docs/analysis/`; no code.

## 2. Starting points (from the #104 analysis)

* The game picks assets by a language index (US `0x1FC724`). Japanese (0) already handles double-byte text, and Korean is double-byte too, so **the Japanese path is the likely starting point** (inferred).
* Per-language assets: `Media\Ending\tex_*.dat`, `Media\Mark\List_*.{bin,dat}`, `Media\Mark\tex_*.dat`, the `_j`-suffixed files, `Media\Font\` ([language.md](../analysis/language.md) §2).

## 3. Questions to answer

| Question | How to check |
|---|---|
| Where the text lives (asset files, strings in the XBE) | file-open trace while running in Japanese; asset format analysis |
| Text encoding (Shift-JIS or the game's own table) | byte analysis of the Japanese text assets |
| Font: a bitmap atlas or not, glyph-count limits, whether the 2,350 Hangul of KS X 1001 or only the characters used would fit | `Media\Font` format analysis; decompiling the text renderer |
| Text inside pictures (logos, menu images, graffiti names) | texture lists and per-language textures |
| Voice and subtitle timing | whether subtitles are tied to voice |
| Distribution | must carry no game assets: a diff tool applied to the user's dump, or a run-time asset override folder (`rexbox`'s file layer) |
| License and rights | rights to the translation itself; the rule of shipping no original assets |

## 4. Next steps

* Once #104 runs in Japanese, trace the text assets and the font path in the same run to fill §3.
* Write the verdict (feasible or not, the recommended way, the effort) here and decide with the user whether to make it a separate project.
