# 설계: 처음 쓰는 GL 프로그램의 컴파일 끊김 줄이기 (issue #51)

선행: [i040 그리기 경로 비용](20261013-i040-draw-performance.md), [frame-time §6](../analysis/frame-time.md)

## 1. 요구사항과 측정

* D3D8 HLE는 GL 프로그램(정점 셰이더 + 프래그먼트 셰이더)을 그 프로그램을 처음 쓰는 그리기에서 컴파일·링크한다. 게임은 장면 중간에 새 프로그램을 요구한다.
* 그 시간이 그대로 그 프레임의 끊김이 된다. 이를 줄인다.
* **측정 도구**:
  * `REXBOX_D3D8_PROFILE`가 구간마다 만든 프로그램 수와 걸린 시간을 한 줄로 낸다(`programs: N compiled, M from the binary cache, X ms`).
  * 구간 보고에 가장 긴 프레임과 33.4 ms(vblank 두 번)를 넘은 프레임 수를 더한다(`worst X (N over 33.4)`). 끊김은 평균이 아니라 한 프레임에 나타나기 때문이다.
* **기준**(Intel HD Graphics 620, 데모 끝까지 한 번):
  * 프로그램 57개, 합계 약 1.7~2.4초(실행마다 다름), 개당 평균 약 30~42 ms
  * 데모 첫 300프레임에 34개(1.1~1.6초)
  * 그 뒤로 1~8개씩 흩어져 개당 18~48 ms

## 2. 설계

* **바이너리 저장**: 링크한 프로그램을 드라이버가 돌려주는 바이너리로 디스크에 남긴다.
  * `glGetProgramBinary`/`glProgramBinary`/`glProgramParameteri`를 쓴다. GL 4.1, ARB_get_program_binary, GLES 3.0에 있다.
  * 다음 실행은 컴파일 대신 그 바이너리로 프로그램을 만든다.
  * 진입점이 없거나 바이너리 형식 수(`GL_NUM_PROGRAM_BINARY_FORMATS`)가 0이면 캐시는 꺼진다.
* **항목**: 파일 하나에 다음을 담는다.
  * magic, 배치 버전, 바이너리 형식
  * 드라이버 식별(vendor, renderer, version 문자열)
  * 정점·프래그먼트 소스 전문
  * 바이너리
  * 파일 이름은 셋(드라이버, 두 소스)의 64비트 해시다.
* **불러오기**: 해시로 파일을 찾은 뒤 드라이버 문자열과 두 소스가 **완전히 같을 때만** 바이너리를 쓴다. 해시 충돌이나 드라이버가 바뀐 경우는 다시 컴파일한다.
  * 드라이버가 바이너리를 거부하면(`GL_LINK_STATUS`가 0) 프로그램을 지우고 지금처럼 컴파일한다. 새 바이너리가 그 항목을 덮어쓴다.
* **저장**: 링크 전에 `GL_PROGRAM_BINARY_RETRIEVABLE_HINT`를 켜고, 링크가 성공하면 저장한다. 임시 파일에 쓴 뒤 이름을 바꾼다. 그래서 읽는 쪽이 반쯤 쓴 파일을 보지 않는다.
* **위치**: 현재 폴더의 `cache\shaders\`(gitignore, HDD의 `hdd\`와 같은 방식)다.
  * `REXBOX_SHADER_CACHE=<폴더>`로 바꾸고, `REXBOX_SHADER_CACHE=0`으로 끈다.
  * 폴더를 만들 수 없으면 한 번 보고하고 끈다.
* **한계**:
  * 첫 실행은 지금처럼 컴파일한다(미리 빌드하지 않음).
  * 바이너리는 드라이버·GPU마다 다르므로 다른 기계로 옮겨도 다시 컴파일될 뿐이다.

## 3. 검증

| 검증 | 기대 |
|---|---|
| 두 번 실행 | 두 번째 실행의 프로그램이 모두 캐시에서 만들어지고, 시간이 크게 줄어듦 |
| 캐시 끔/켬 번갈아 | 데모 첫 구간과 그 뒤의 가장 긴 프레임이 줄어듦 |
| 망가진 항목(바이너리 일부 변조, 잘린 파일) | 다시 컴파일하고 항목을 다시 씀 |
| Debug/Release, 회귀 도구 | 경고 0, 회귀 없음 |

---

# Design: less stutter from compiling GL programs on first use (issue #51)

Prerequisites: [i040 draw-path cost](20261013-i040-draw-performance.md), [frame-time §6](../analysis/frame-time.md)

## 1. Requirements and measurement

* The D3D8 HLE compiles and links a GL program (vertex plus fragment shader) on the first draw that uses it, and the title asks for new programs in the middle of scenes.
* That time becomes a stutter in that frame. The goal is to reduce it.
* **Tools**:
  * `REXBOX_D3D8_PROFILE` reports, per window, the programs made and the time they took (`programs: N compiled, M from the binary cache, X ms`).
  * The window report gains the longest frame and the number of frames over 33.4 ms, two vertical blanks (`worst X (N over 33.4)`), since stutter shows in single frames, not in averages.
* **Baseline** (Intel HD Graphics 620, one run through the demo):
  * 57 programs, about 1.7–2.4 s in all (varies by run), about 30–42 ms each on average
  * 34 of them in the first 300 demo frames (1.1–1.6 s)
  * the rest in groups of 1–8, 18–48 ms each

## 2. Design

* **Stored binaries**: a linked program is kept on disk as the binary the driver hands back.
  * This uses `glGetProgramBinary`/`glProgramBinary`/`glProgramParameteri`, found in GL 4.1, ARB_get_program_binary and GLES 3.0.
  * The next run creates the program from that binary instead of compiling.
  * Without the entry points, or with no binary formats (`GL_NUM_PROGRAM_BINARY_FORMATS` is 0), the cache is off.
* **Entries**: one file holds:
  * a magic, a layout version and the binary format
  * the driver identity (vendor, renderer and version strings)
  * the full vertex and fragment sources
  * the binary
  * Its name is a 64-bit hash of the driver identity and both sources.
* **Loading**: the binary is used only when the driver identity and both sources match **exactly**. A hash collision or a changed driver leads to compiling.
  * If the driver rejects the binary (`GL_LINK_STATUS` is 0), the program is deleted and compiled as before, and the new binary overwrites the entry.
* **Storing**: `GL_PROGRAM_BINARY_RETRIEVABLE_HINT` is set before linking, and a successful link is stored. It is written to a temporary file and renamed, so a reader never sees a half-written entry.
* **Location**: `cache\shaders\` in the current folder (gitignored, like the HDD's `hdd\`).
  * `REXBOX_SHADER_CACHE=<folder>` moves it, and `REXBOX_SHADER_CACHE=0` turns it off.
  * If the folder cannot be created, this is reported once and the cache is off.
* **Limits**:
  * The first run still compiles; programs are not built ahead.
  * Binaries are specific to the driver and GPU, so on another machine they are only recompiled.

## 3. Verification

The table above applies:

* Two runs: every program of the second run comes from the cache, in much less time.
* Alternating runs with the cache off and on: lower worst frames in the first demo window and after.
* Damaged entries (a binary with altered bytes, a truncated file): recompiled and rewritten.
* Debug and Release with zero warnings; no regressions in the tools.
