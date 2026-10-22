# 작업 로그: Info 탭에 rexbox 버전과 커밋 표시, Stats 그래프 높이 절반 (issue #111)

설계: [20261023-i111](../design/20261023-i111-rexbox-version-info.md) · 작업 지시: [20261023-i111](../work-orders/20261023-i111-rexbox-version-info.md)

## 한 일

1. `src/app/build_stamp.cmake`
   * 커밋을 구하는 부분을 함수 `rejsrf_git_commit`로 묶었다.
   * reJSRF 커밋에 더해 `third_party/rexbox/VERSION`과 rexbox 커밋을 `REJSRF_REXBOX_VERSION`, `REJSRF_REXBOX_COMMIT`로 `rejsrf_build_stamp.h`에 쓴다.
2. `BuildInfo`에 `rexbox_version`, `rexbox_commit`를 더했고, `main.cpp`가 채운다.
3. Info 탭: `Game` 다음에 `rexbox`(`v0.0.9`), `rexbox commit`(`b2f2653`) 두 줄.
4. Stats 창: 세 그래프가 함께 쓰는 `DrawGraph`의 높이를 64에서 32로(사용자 추가 요청).
5. 문서: 설계 i084의 Info 항목, 가이드의 Info 탭 설명.

## 검증

| 검증 | 결과 |
|---|---|
| Release 빌드(`rejsrf`) | 경고 0 |
| 생성 헤더 | `REJSRF_REXBOX_VERSION "0.0.9"`, `REJSRF_REXBOX_COMMIT "b2f2653"`. `third_party/rexbox/VERSION`, `git -C third_party/rexbox rev-parse --short HEAD`와 같다 |
| 실행 화면(창 캡처) | Info 탭의 Game 아래에 `rexbox v0.0.9`, `rexbox commit b2f2653`. Stats 창의 FPS·Memory·Video memory 그래프가 절반 높이이고 눈금 이름표(60/30/0, 192/96/0 MiB 등)가 겹치지 않는다 |

캡처 방법: scratch 폴더의 실행 파일 사본에 Stats 세 항목을 켠 `rejsrf.ini`를 두고 창으로 실행했다. 20초 뒤 `PrintWindow`(PW_RENDERFULLCONTENT)로 창을 찍고, Tab 키 메시지를 보내 메뉴를 연 뒤 한 번 더 찍었다. 프레임 덤프는 오버레이를 그리기 전에 저장되므로 이 확인에 쓸 수 없다. 게임 화면이 담긴 캡처는 커밋하지 않았다.

---

# Work log: the rexbox version and commit in the Info tab, Stats graphs at half height (issue #111)

Design: [20261023-i111](../design/20261023-i111-rexbox-version-info.md) · Work order: [20261023-i111](../work-orders/20261023-i111-rexbox-version-info.md)

## Done

1. `src/app/build_stamp.cmake`
   * The commit lookup became the function `rejsrf_git_commit`.
   * Besides reJSRF's commit, it writes `third_party/rexbox/VERSION` and the rexbox commit into `rejsrf_build_stamp.h` as `REJSRF_REXBOX_VERSION` and `REJSRF_REXBOX_COMMIT`.
2. `rexbox_version` and `rexbox_commit` in `BuildInfo`, filled by `main.cpp`.
3. Info tab: two rows after `Game`, `rexbox` (`v0.0.9`) and `rexbox commit` (`b2f2653`).
4. Stats window: the height in `DrawGraph`, shared by the three graphs, from 64 to 32 (the user's added request).
5. Documents: the Info entry of design i084, the guide's Info tab description.

## Verification

| Check | Result |
|---|---|
| Release build (`rejsrf`) | zero warnings |
| Generated header | `REJSRF_REXBOX_VERSION "0.0.9"`, `REJSRF_REXBOX_COMMIT "b2f2653"`, equal to `third_party/rexbox/VERSION` and `git -C third_party/rexbox rev-parse --short HEAD` |
| The running app (window capture) | `rexbox v0.0.9` and `rexbox commit b2f2653` below Game in the Info tab; the Stats window's FPS, Memory and Video memory graphs at half height, with their tick labels (60/30/0, 192/96/0 MiB, ...) not overlapping |

How it was captured: a copy of the executable in a scratch folder, with a `rejsrf.ini` turning on the three Stats sections, ran in a window. After 20 s `PrintWindow` (PW_RENDERFULLCONTENT) captured it; then a Tab key message opened the menu and it was captured again. Frame dumps are saved before the overlay is drawn, so they cannot show this. Captures with game pictures in them are not committed.
