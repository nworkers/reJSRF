# Issue #32 작업 로그 — 첫 화면까지의 진행 기록과 스크린샷

작업 지시: [20261013-i032](../work-orders/20261013-i032-wip-post-first-screen.md) · 설계: [20261013-i032](../design/20261013-i032-wip-post-first-screen.md)

## 요약

첫 진행 기록 [From XBE to First Screen — Work in Progress 1](../post/2026-10-13-031546-from-xbe-to-first-screen-wip.md)을 썼다. 범위는 최초 커밋부터 `7fe2e90`까지(v0.0.1 ~ v0.0.12, issue #31)다. 화면 11장을 `docs/screenshots/`에 넣었고(합 약 680 KB), 작성 지침(`docs/post/README.md`)과 README의 현황을 갱신했다.

## 과정

1. `REXBOX_FRAME_DUMP=60,120,…,9000`으로 150초 부팅에서 frame dump 150장을 남겼다. 대조표 두 장으로 로고 다섯 개와 어트랙트 데모 다섯 장면을 골랐다.
2. 보이는 창을 88초 뒤 `PrintWindow(PW_RENDERFULLCONTENT)`로 찍었다. 제목줄은 `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 59.0`였다.
3. 글의 내용은 릴리스 노트 v0.0.1~v0.0.12, 작업 로그 i019·i021·i023·i027·i029, boot-frontier에서 가져왔다. 경과 시간은 각 릴리스 커밋 시각과 최초 커밋(2026-10-12 00:05)의 차다.
4. 대조표에서 캐릭터가 보이지 않는 것을 확인했다. 정점 셰이더 프로그램 draw를 아직 그리지 않기 때문이고, 글의 "현재 한계"에 적었다.

## 검증

| 검증 | 결과 |
|---|---|
| 글 구조 | 한국어·영어 문서, 필수 소제목 4개, Mermaid 블록 10개(언어마다 5개), 구분선 1개 |
| 링크 | 글, README, 작성 지침의 상대 링크가 모두 실제 파일을 가리킴 |
| 이미지 | JPEG 11장, 640x480(장면), 1302x1016(창) |

## 남은 일

* 캐릭터를 그리면(정점 셰이더 프로그램) 다음 진행 기록을 쓴다.

---

# Issue #32 work log — a progress post and screenshots up to the first screen

Work order: [20261013-i032](../work-orders/20261013-i032-wip-post-first-screen.md) · Design: [20261013-i032](../design/20261013-i032-wip-post-first-screen.md)

## Summary

The first progress post, [From XBE to First Screen — Work in Progress 1](../post/2026-10-13-031546-from-xbe-to-first-screen-wip.md), covers the first commit to `7fe2e90` (v0.0.1 – v0.0.12 and issue #31). Eleven screens went into `docs/screenshots/` (about 680 KB in total). The writing guide (`docs/post/README.md`) and the README's status were updated.

## Process

1. `REXBOX_FRAME_DUMP=60,120,…,9000` kept 150 frame dumps from a 150 s boot. Two contact sheets gave five logos and five attract-demo scenes.
2. The visible window was captured after 88 s with `PrintWindow(PW_RENDERFULLCONTENT)`. Its title bar read `reJSRF v0.0.12 (Win32/x86 Release) - Build 2026-10-13 02:50 - OpenGL 3.3 - FPS : 59.0`.
3. The content comes from the release notes v0.0.1–v0.0.12, the work logs i019, i021, i023, i027 and i029, and boot-frontier. Elapsed hours are each release commit's time minus the first commit's (2026-10-12 00:05).
4. The contact sheets show no characters. Draws with vertex-shader programs are not drawn yet; the post lists this under its current limits.

## Verification

| Check | Result |
|---|---|
| post structure | Korean and English documents; the 4 required sections; 10 Mermaid blocks (5 per language); 1 separator |
| links | every relative link in the post, the README and the guide points at a real file |
| images | 11 JPEGs: 640x480 scenes and a 1302x1016 window |

## Remaining

* Write the next progress post once characters are drawn (vertex-shader programs).
