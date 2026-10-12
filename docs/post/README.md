# 진행 기록 작성 지침

[rePIU의 지침](https://github.com/reexec/rePIU/blob/main/docs/post/README.md)을 따른다.

## 파일 위치와 이름

글은 `docs/post/` 아래에 Markdown 파일로 쓴다. 파일명은 `yyyy-mm-dd-hhmmss-title.md`다. `title`은 영어 소문자 단어를 하이픈으로 잇는다. 진행 기록(work in progress)은 `-wip`로 끝낸다.

```text
docs/post/2026-10-13-031546-from-xbe-to-first-screen-wip.md
```

## 문서 언어 순서

* 글 제목은 영어만 쓴다.
* 본문은 한국어 전체 문서를 먼저 쓰고, 같은 내용의 영어 전체 문서를 뒤에 쓴다.
* 두 문서 사이에는 `---` 한 줄만 둔다. 각 문서의 첫 줄은 `#` 제목이다.
* 제목 다음에는 범위(첫 커밋과 마지막 커밋 링크) 줄을 두고, 그 다음에 글을 소개하는 문단을 둔다.

## 필수 구성

한국어 문서는 아래 소제목을 유지한다.

```markdown
## 주요 변경 사항
## 사용된 기술 스택
```

영어 문서는 아래 소제목을 쓴다.

```markdown
## Major Changes
## Technology Stack Used
```

## 도식과 화면

* 도식은 Mermaid로 그린다. 코드 흐름은 `flowchart`나 `sequenceDiagram`, 누적 수치는 `xychart-beta`를 먼저 쓴다.
* 화면은 [`docs/screenshots/`](../screenshots/README.md)에 두고 상대 경로로 넣는다. 캡처 방법은 그 README에 적는다.
* 원본 게임 데이터(XBE, 자산, ISO)와 그것에서 생성한 코드는 글에도 넣지 않는다.

## 글 목록

| 날짜 | 글 |
|---|---|
| 2026-10-13 | [From XBE to First Screen — Work in Progress 1](2026-10-13-031546-from-xbe-to-first-screen-wip.md) |

---

# Progress post guide

This follows [rePIU's guide](https://github.com/reexec/rePIU/blob/main/docs/post/README.md).

## Location and file name

Posts are Markdown files under `docs/post/`, named `yyyy-mm-dd-hhmmss-title.md`. `title` is lowercase English words joined by hyphens. Work-in-progress posts end in `-wip`.

## Language order

* The post title is English only.
* The body is the complete Korean document first, then the complete English document with the same content.
* A single `---` line separates the two. Each document starts with a `#` title.
* After the title comes the range line (links to the first and last commits), then a paragraph that introduces the post.

## Required sections

The Korean document keeps `## 주요 변경 사항` and `## 사용된 기술 스택`. The English document uses `## Major Changes` and `## Technology Stack Used`.

## Diagrams and screens

* Diagrams use Mermaid: `flowchart` or `sequenceDiagram` for code flow, and `xychart-beta` first for cumulative numbers.
* Screens live in [`docs/screenshots/`](../screenshots/README.md) and are linked with relative paths. That README says how they were taken.
* Original game data (XBE, assets, ISO) and code generated from it never go into a post either.

## Posts

| Date | Post |
|---|---|
| 2026-10-13 | [From XBE to First Screen — Work in Progress 1](2026-10-13-031546-from-xbe-to-first-screen-wip.md) |
