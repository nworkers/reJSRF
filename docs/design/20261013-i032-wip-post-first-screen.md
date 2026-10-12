# 설계: 첫 화면까지의 진행 기록과 스크린샷 (issue #32)

선행: [boot-frontier](../analysis/boot-frontier.md), 참고: rePIU `docs/design/20260710-053-wip-blog-post-guidelines.md`, `docs/screenshots/README.md`

## 1. 배경

부팅이 로고 시퀀스와 3D 어트랙트 데모를 그리게 되었다. 최초 커밋부터 이 지점까지의 과정을 한 편의 진행 기록(WIP)으로 남기고, 화면을 함께 싣는다.

## 2. 결정

* **형식**: rePIU의 지침을 따른다. 위치는 `docs/post/yyyy-mm-dd-hhmmss-title.md`이고, 제목은 영어다. 한국어 전문 다음에 영어 전문을 쓰고, 필수 소제목(주요 변경 사항 / 사용된 기술 스택)과 Mermaid 도식을 갖춘다. 지침은 `docs/post/README.md`에 둔다.
* **스크린샷**: `docs/screenshots/`에 둔다. 장면 사진은 frame dump(감마 램프 적용 back buffer, 640x480)로 1초마다 찍어 대조표에서 고른다. 창 사진은 `PrintWindow`로 제목줄까지 찍는다. JPEG 품질은 90이다. 캡처 방법은 README에 적는다.
* **게임 데이터 규칙과의 관계**: AGENTS.md는 XBE, 자산, ISO, 생성 코드를 금지한다. 스크린샷은 실행 화면의 사진이고 자산을 추출한 것이 아니므로 rePIU와 같이 넣는다. i023 작업 로그는 "덤프 이미지는 게임 자산이라 커밋하지 않는다"고 적었는데, 이번 결정으로 대체한다. 텍스처 덤프 같은 자산 추출물은 여전히 넣지 않는다.
* README의 현황 문구를 갱신하고 글과 창 사진을 연결한다.

## 3. 검증

* 글 구조: 두 언어 문서, 필수 소제목, Mermaid 블록.
* 이미지 링크가 모두 실제 파일을 가리키는지 확인한다.
* 글의 수치를 릴리스 노트와 작업 로그에 대조한다.

---

# Design: a progress post and screenshots up to the first screen (issue #32)

Prerequisite: [boot-frontier](../analysis/boot-frontier.md). References: rePIU `docs/design/20260710-053-wip-blog-post-guidelines.md` and `docs/screenshots/README.md`.

## 1. Background

The boot now draws the logo sequence and the 3D attract demo. The journey from the first commit to this point is recorded as a single work-in-progress post, with screens.

## 2. Decisions

* **Format.** rePIU's guide is followed:
  * the location `docs/post/yyyy-mm-dd-hhmmss-title.md` and an English title
  * the full Korean document, then the full English one
  * the required sections (Major Changes / Technology Stack Used) and Mermaid diagrams

  The guide lives in `docs/post/README.md`.
* **Screenshots** live in `docs/screenshots/`.
  * Scene shots are frame dumps (back buffers with the gamma ramp, 640x480), taken once a second, with one picked per scene from contact sheets.
  * The window shot is taken with `PrintWindow`, title bar included.
  * All are JPEG at quality 90. The README records how they were taken.
* **Relation to the game-data rule.** AGENTS.md forbids XBEs, assets, ISOs and generated code. Screenshots are pictures of the running game, not extracted assets, so they are included, as in rePIU. This replaces the note in the i023 work log that frame dumps are game assets and are not committed. Extracted assets such as texture dumps still stay out.
* The README's status line is updated and links to the post and the window shot.

## 3. Verification

* Post structure: two language documents, the required sections, Mermaid blocks.
* Every image link points at a real file.
* The post's numbers are checked against the release notes and work logs.
