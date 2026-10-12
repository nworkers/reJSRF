# 스크린샷 / Screenshots

진행 기록과 README에 쓰는 화면이다. 재컴파일된 JSRF가 reJSRF 위에서 그린 출력을 캡처했다. 원본 XBE, 자산, ISO와 생성 코드는 저장소에 넣지 않는다. 이 파일들은 자산을 추출한 것이 아니라 실행 화면을 찍은 것이다.

*Screens used in progress posts and the README, captured from what the recompiled JSRF draws on reJSRF. The original XBE, assets, ISO and generated code are not in the repository; these files are pictures of the running game, not extracted assets.*

| 파일 / File | 장면 / Scene | present |
|---|---|---:|
| `first-screen-window.jpg` | 창 전체(제목줄 포함), 어트랙트 데모 / the whole window with its title bar, attract demo | 약 88초 / about 88 s |
| `first-screen-01-sega.jpg` | Presented by SEGA | 180 |
| `first-screen-02-smilebit.jpg` | Created by Smilebit | 540 |
| `first-screen-03-adx.jpg` | ADX | 900 |
| `first-screen-04-dolby.jpg` | Dolby Digital | 1,260 |
| `first-screen-05-graffiti-notice.jpg` | 낙서 경고문 / anti-graffiti notice | 1,860 |
| `first-screen-06-attract-overpass.jpg` | 어트랙트 데모, 고가도로 / attract demo, overpass | 2,700 |
| `first-screen-07-attract-skyline.jpg` | 어트랙트 데모, 스카이라인 / attract demo, skyline | 5,280 |
| `first-screen-08-attract-plaza.jpg` | 어트랙트 데모, 광장 / attract demo, plaza | 5,640 |
| `first-screen-09-attract-bridge.jpg` | 어트랙트 데모, 고가 아래 / attract demo, under the bridge | 7,440 |
| `first-screen-10-attract-street.jpg` | 어트랙트 데모, 거리 / attract demo, street | 8,640 |

## 캡처 방법 / How they were taken

- 커밋 `7fe2e90`(v0.0.12 + issue #31)의 Win32 x86 Release 빌드로 US판 `default_eng.xbe`를 입력 없이 실행했다.
- `first-screen-01`~`10`: `REXBOX_DISPLAY=hidden`, `REXBOX_AUDIO=none`, `REXBOX_FRAME_DUMP=60,120,…,9000`(1초마다)으로 back buffer를 감마 램프까지 적용해 640x480 BMP로 저장했다. 대조표에서 장면마다 한 장을 골라 JPEG(품질 90)로 바꿨다.
- `first-screen-window.jpg`: 기본 창(2배, 1280x960)을 88초 뒤 DPI 인식 프로세스에서 `PrintWindow(PW_RENDERFULLCONTENT)`로 제목줄까지 찍어 JPEG(품질 90)로 바꿨다.

*Taken with the Win32 x86 Release build of commit `7fe2e90` (v0.0.12 + issue #31), running the US `default_eng.xbe` with no input. `first-screen-01` to `10` are back buffers, with the gamma ramp applied, saved as 640x480 BMPs with `REXBOX_DISPLAY=hidden`, `REXBOX_AUDIO=none` and `REXBOX_FRAME_DUMP=60,120,…,9000` (one per second). One frame per scene was chosen from contact sheets and saved as JPEG (quality 90). `first-screen-window.jpg` is the default window (2x, 1280x960) after 88 s, captured with its title bar by `PrintWindow(PW_RENDERFULLCONTENT)` in a DPI-aware process, saved as JPEG (quality 90).*
