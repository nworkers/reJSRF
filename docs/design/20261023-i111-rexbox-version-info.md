# 설계: Info 탭에 rexbox 버전과 커밋 표시, Stats 그래프 높이 절반 (issue #111)

상태: **구현됨**(사용자 요청). 작업 지시: [20261023-i111](../work-orders/20261023-i111-rexbox-version-info.md) · 작업 로그: [20261023-i111](../work-logs/20261023-i111-rexbox-version-info.md)

## 1. 요구

* 설정 메뉴(OSD) Info 탭의 **Game 항목 아래**에 공용 런타임 rexbox의 버전과 커밋 ID를 보인다(사용자 요청).
* rexbox는 submodule `third_party/rexbox`([설계 i102](20261022-i102-rexbox-submodule.md))다. 버그 보고에 어떤 런타임으로 빌드했는지 남게 한다.
* Stats 창의 FPS·Memory·Video memory 그래프 높이를 모두 절반으로 줄인다(사용자 요청, 같은 작업).

## 2. 방안

* 값은 빌드할 때 `src/app/build_stamp.cmake`가 정한다. 이 스크립트는 이미 빌드마다 reJSRF의 빌드 일시와 커밋을 `rejsrf_build_stamp.h`에 쓴다.
  * `REJSRF_REXBOX_VERSION`: `third_party/rexbox/VERSION`의 첫 줄. 없으면 `unknown`.
  * `REJSRF_REXBOX_COMMIT`: `git -C third_party/rexbox rev-parse --short HEAD`. submodule 안에 추적 파일 변경이 있으면 `+modified`, 실패하면 `unknown`(reJSRF 커밋과 같은 규칙).
* 빌드마다 다시 읽으므로 submodule 커밋을 옮기면 다시 configure하지 않아도 다음 빌드에 반영된다. 값이 같으면 헤더를 다시 쓰지 않아 재컴파일도 없다.
* 기각한 방안
  * CMake configure 때 정의(`target_compile_definitions`): submodule만 옮기면 갱신되지 않는다.
  * rexbox가 런타임 API로 버전을 노출: rexbox 저장소 변경이 필요하고, 표시 문자열이 타이틀 앱 일이라 지금은 앱에서 충분하다.
* 표시: Info 탭 첫 표의 `Game` 다음 줄에 `rexbox` = `v<버전>`, `rexbox commit` = `<커밋>`. 위쪽 `reJSRF`/`Commit` 줄과 같은 형식이다.
* 그래프: 세 그래프가 함께 쓰는 `DrawGraph`의 높이를 64에서 32(OSD 배율 곱)로 줄인다. 눈금은 그래프마다 위·가운데·아래 셋이라, 높이 32에서도 간격(16)이 글자 높이보다 커서 이름표가 겹치지 않는다.

## 3. 검증

* 빌드 경고 0. `rejsrf_build_stamp.h`에 두 값이 `third_party/rexbox/VERSION`과 `git -C third_party/rexbox rev-parse --short HEAD`와 같게 들어간다.
* 실행해 Info 탭에서 Game 아래 두 줄과, Stats 창의 낮아진 그래프를 확인한다.

---

# Design: the rexbox version and commit in the Info tab, Stats graphs at half height (issue #111)

Status: **implemented** (user request). Work order: [20261023-i111](../work-orders/20261023-i111-rexbox-version-info.md) · Work log: [20261023-i111](../work-logs/20261023-i111-rexbox-version-info.md)

## 1. Requirement

* The settings menu's (OSD) Info tab shows the shared runtime rexbox's version and commit ID **below the Game row** (user request).
* rexbox is the submodule `third_party/rexbox` ([design i102](20261022-i102-rexbox-submodule.md)); a bug report then tells which runtime the build used.
* The Stats window's FPS, Memory and Video memory graphs all become half as tall (user request, same task).

## 2. Approach

* `src/app/build_stamp.cmake` sets the values at build time. It already writes reJSRF's build time and commit into `rejsrf_build_stamp.h` on every build.
  * `REJSRF_REXBOX_VERSION`: the first line of `third_party/rexbox/VERSION`, or `unknown`.
  * `REJSRF_REXBOX_COMMIT`: `git -C third_party/rexbox rev-parse --short HEAD`, with `+modified` when tracked files inside the submodule changed, or `unknown` on failure (the same rule as reJSRF's commit).
* Read on every build, so moving the submodule commit shows in the next build without a reconfigure. An unchanged value leaves the header alone and recompiles nothing.
* Rejected
  * Definitions at CMake configure time (`target_compile_definitions`): moving only the submodule would not refresh them.
  * A runtime version API in rexbox: that needs a rexbox change, and the display string is the title app's business, so the app suffices for now.
* Display: right after `Game` in the Info tab's first table, `rexbox` = `v<version>` and `rexbox commit` = `<commit>`, in the same form as the `reJSRF`/`Commit` rows above.
* Graphs: the height in `DrawGraph`, which all three graphs use, goes from 64 to 32 (times the OSD scale). Each graph has three ticks (top, middle, bottom), so at 32 their spacing (16) still exceeds the text height and the labels do not overlap.

## 3. Verification

* Zero build warnings. `rejsrf_build_stamp.h` holds both values, equal to `third_party/rexbox/VERSION` and `git -C third_party/rexbox rev-parse --short HEAD`.
* Run the app and see the two rows below Game in the Info tab and the lower graphs in the Stats window.
