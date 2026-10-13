# Win32 개발 환경 준비와 빌드

근거: [설계 i001](../design/20261012-i001-project-bootstrap.md), [작업 로그 i001](../work-logs/20261012-i001-project-bootstrap.md), [설계 i002](../design/20261012-i002-recomp-pipeline.md), [작업 로그 i002](../work-logs/20261012-i002-recomp-pipeline.md), [설계 i003](../design/20261012-i003-kernel-hle.md), [작업 로그 i003](../work-logs/20261012-i003-kernel-hle.md), [설계 i007](../design/20261012-i007-d3d8-hle-first-batch.md), [작업 로그 i007](../work-logs/20261012-i007-d3d8-hle-first-batch.md)

## 1. 필요한 것

* Windows 10/11
* Visual Studio 2022 또는 2026, "C++를 사용한 데스크톱 개발" 워크로드(x86/x64 MSVC 도구 포함)
* OpenGL 3.3 이상을 지원하는 그래픽 드라이버(창 표시용; `REXBOX_DISPLAY=none`으로 창 없이 실행 가능)
* CMake 3.20 이상(Visual Studio 내장 CMake도 가능)
* Git
* 재컴파일 파이프라인용: Python 3.10 이상과 `pip install capstone`
* 직접 소유한 Jet Set Radio Future 디스크의 덤프

## 2. 저장소 받기

```powershell
git clone --recurse-submodules https://github.com/nworkers/reJSRF.git
cd reJSRF
# submodule 없이 받았다면(xboxrecomp, XbSymbolDatabase, SDL):
git submodule update --init
```

## 3. 게임 파일 두기

디스크에서 추출한 파일을 `roms/Jet Set Radio Future/`에 둔다(`roms/`는 gitignore 대상이며 절대 커밋하지 않는다). ISO만 있다면 [extract-xiso](https://github.com/XboxDev/extract-xiso)나 [xdvdfs](https://github.com/antangelo/xdvdfs)로 추출한다.

```text
roms/Jet Set Radio Future/
  default.xbe        the game's XBE: the USA build (MD5 f4e366bfdceee551a09f42219c90d84e) is the
                     primary target; the Japan-region build is recognized only
  Media/...
```

## 4. 재컴파일 코드 생성

```powershell
pip install capstone
# 처음 한 번: XDK 함수 식별 도구를 먼저 빌드한다
.\scripts\build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols
python scripts\recomp\regenerate.py      # gen/jsrf_us/ 에 생성, 약 2분
```

`regenerate.py`는 기본으로 `roms\Jet Set Radio Future\default.xbe`를 읽는다. 다른 경로는 `--xbe <경로>`로 준다. 읽기 전에 MD5가 US판(`src/game/jsrf/recomp/variants.json`)과 같은지 확인하고, 다르면 생성하지 않고 멈춘다. seed와 타이틀 바인딩이 US판의 주소이기 때문이다. `find_pointer_targets.py`도 같은 검사를 한다.

생성 코드는 커밋하지 않는다. `src/game/jsrf/decomp/`를 바꾸거나 submodule을 갱신한 뒤에는 다시 생성한다. 생성하지 않으면 앱은 로드 단계에서 멈춘다.

## 5. 빌드

```powershell
.\scripts\build_win32_x86.ps1                       # Debug
.\scripts\build_win32_x86.ps1 -Configuration Release
```

결과는 `build\win32_x86\<구성>\` 아래 각 타깃 디렉터리에 생긴다.

## 6. 확인

```powershell
# XBE 정보 출력
.\build\win32_x86\tools\Debug\rexbox_xbe_info.exe "roms\Jet Set Radio Future\default.xbe"

# 앱: XBE 식별, 배치, HLE 등록 후 재컴파일 코드를 진입점부터 실행하고 멈춘 지점을 보고한다.
# D3D8 디바이스 생성 시 1280x960(640x480의 2배) SDL3 창이 열리고 검게 지워진다(멈추면 프로세스와 함께 닫힌다).
# 인자는 게임 디렉터리다. 앱은 그 폴더를 게임의 D:로 쓰고 default.xbe를 읽는다.
.\build\win32_x86\src\app\Debug\rejsrf.exe "roms\Jet Set Radio Future"
# 다른 XBE는 --xbe로 지정한다(예: Sega GT·JSRF 합본 디스크).
.\build\win32_x86\src\app\Debug\rejsrf.exe "roms\Jet Set Radio Future" --xbe SegaJSRF.xbe
```

앱은 읽은 XBE를 인증서로 식별한다. 재컴파일 코드는 US판에서 만들었으므로, US판이면 실행하고 지원하는 다른 판(일본판)이면 읽기만 하고 멈춘다. 디스크의 `default.xbe`가 US판이 아니면 US판 XBE를 `--xbe`로 지정한다.

기대 출력(US판, 재컴파일 코드 포함 빌드). 현재 frontier는 [docs/analysis/boot-frontier.md](../analysis/boot-frontier.md):

```text
reJSRF 0.0.15
xbe: USA (region 1) (<XBE 파일 이름>)
guest memory: 128 MiB, 11 sections placed, 128476 KiB free
hle: 283 exports, kernel imports 107/120 bound, 44 title bindings
boot: entry 00148023, flat dispatch
```

(그 뒤 프로세스는 끝나지 않는다. 창에 로고 시퀀스가 나온 뒤 3D 어트랙트 데모가 이어진다. 게임은 60 Hz로 프레임 루프를 돈다([boot-frontier](../analysis/boot-frontier.md)). 창을 닫거나(X 버튼, Alt+F4) Ctrl+C로 끝내고, 시간을 정해 끝내려면 `REXBOX_WATCHDOG=<초>`를 쓴다. 창 이벤트는 게임이 화면을 넘길 때 처리되므로, 화면을 넘기지 않는 동안에는 닫기가 그때까지 미뤄진다. 루프가 도는지는 `REXBOX_FRAME_LOG=<n>`(n번째 present마다 한 줄)로 본다. 입력은 호스트 게임패드, 없으면 키보드다: 방향키, Enter = START, Backspace = BACK, Z/X/C/V = A/B/X/Y, A/S = 트리거, Q/W = Black/White. [kb: Xbox XInput §4](../kb/xbox-xinput.md))

stderr에는 `[d3d8] Direct3D_CreateDevice: 640x480 format 11, ...`, `[d3d8] display 1280x960, OpenGL 3.3, <GPU 이름>`, `[dsound] audio output: host device, 48000 Hz stereo`, `[dsound] DirectSoundCreate -> ...`, `[dsound] DownloadEffectsImage: 20 effects, ...`, `[d3d8] first frame presented ...` 줄이 나온다. 게임이 사운드 버퍼를 만들지만 아직 `Play`를 부르지 않으므로 소리는 나지 않는다. 오디오 장치가 열렸는지는 `audio output` 줄로 확인한다.

창 제목은 `reJSRF v<버전> (Win32/x86 <구성>) - Build <빌드 일시> - OpenGL 3.3 - FPS : 60.0` 꼴이다. 창은 종횡비를 지키며 크기를 바꾸거나 최대화할 수 있다. Alt+1/2/3은 640x480의 1~3배 크기, 더블클릭이나 Alt+Enter는 테두리 없는 전체화면 전환이다(Alt+Enter는 게임의 START로 가지 않는다).

게임의 HDD는 현재 폴더의 `hdd\`(gitignore)에 만들어진다. 다른 곳을 쓰려면 `REJSRF_HDD_ROOT`를 지정한다. 모든 파일 열기를 보려면 `REXBOX_TRACE_FILES=1`을 켠다. `REXBOX_HLE_FALLBACK=lifted`이면 구현 없는 XDK 함수를 멈추지 않고 원본대로 실행하며 이름을 기록한다(호출 순서를 보기 위한 진단 모드로, 결국 하드웨어 접근에서 fault가 난다). `REXBOX_DISPLAY=none`이면 창을 만들지 않는다(헤드리스 진단). `REXBOX_AUDIO=none`이면 오디오 장치를 열지 않는다(재생 커서는 실시간으로 움직인다). `REXBOX_DSOUND_TRACE=1`이면 버퍼 생성의 형식·믹스 빈과 `Play`를 기록한다. `REXBOX_DISPLAY=hidden`이면 창을 보이지 않게 만들고 그리기는 그대로 한다. `REXBOX_FRAME_DUMP=<n>[,<n>…]`이면 n번째 present의 화면을 `frame_<n>.bmp`로 저장한다(폴더는 `REXBOX_FRAME_DUMP_DIR`, 기본은 현재 폴더). 둘을 함께 쓰면 창을 보지 않고 화면을 확인할 수 있다(예: `REXBOX_DISPLAY=hidden`, `REXBOX_FRAME_DUMP=600`, `REXBOX_WATCHDOG=20`이면 `frame_600.bmp`에 로고가 저장된다). `REXBOX_D3D8_TRACE=1`이면 처음 보는 텍스처 포맷·정점 형식·셰이더 키를 기록한다. `REXBOX_SHADER_DUMP=1`이면 게임이 만드는 정점 셰이더의 선언·디스어셈블과 픽셀 셰이더 정의를 출력한다. `REXBOX_D3D8_PROFILE=<n>`이면 n번째 present마다 프레임 시간이 어디에 쓰였는지(그리기 단계, present, 나머지)를 한 줄로 낸다(예: `REXBOX_DISPLAY=hidden`, `REXBOX_AUDIO=none`, `REXBOX_D3D8_PROFILE=300`, Release). 아직 그리지 못하는 경로(프로그래머블 셰이더 등)는 이 변수 없이도 한 번 보고된다.

시험 도구:

```powershell
.\build\win32_x86\tools\Debug\rexbox_kernel_probe.exe   # "0 failure(s)", exit code 0 (게임 데이터 불필요)
.\build\win32_x86\tools\Debug\rexbox_dsound_probe.exe   # DSOUND HLE: "48 check(s), 0 failure(s)", exit code 0 (게임 데이터·오디오 장치 불필요)
.\build\win32_x86\tools\Debug\rexbox_d3d8_probe.exe     # D3D8 HLE 순수 부분: "116 check(s), 0 failure(s)", exit code 0 (게임 데이터·GPU 불필요)
.\build\win32_x86\tools\Debug\rejsrf_decomp_check.exe   # "0 difference(s)", exit code 0
.\build\win32_x86\tools\Debug\rejsrf_d3d8_check.exe     # D3D8 HLE vs. 원본: "14018 case(s), 0 difference(s)", exit code 0
.\build\win32_x86\tools\Debug\rejsrf_d3d8_check.exe --dump   # 원본의 모드 표·Caps·포맷 판정 출력
```

`rejsrf_d3d8_check`는 XBE가 `roms\Jet Set Radio Future\default.xbe`에 있다고 가정하며, 다른 경로는 인자로 준다. `python scripts\recomp\check_hle_pops.py`는 D3D8·XInput·DSOUND HLE export의 pop 개수와 lift 원본의 `ret n`을 대조한다. `python scripts\recomp\find_pointer_targets.py`는 데이터나 즉시값으로만 참조되어 lift되지 않은 함수를 찾는다(`--write`로 seed 파일 갱신 뒤 재생성).

멈춤이나 메모리 손상을 추적하는 진단 환경 변수는 [ARCHITECTURE.md §5](../../ARCHITECTURE.md)에 있다(`REXBOX_HLE_TRACE`, `REXBOX_TRACE_MEMORY`, `REXBOX_WATCHDOG`, `REXBOX_WATCHDOG_DUMP`, `REXBOX_HEAP_CHECK`; 직접 호출 프로파일은 `cmake -DREJSRF_RECOMP_CALL_PROFILE=ON`으로 생성 코드를 빌드했을 때). 현재 기대 실행은 [boot-frontier](../analysis/boot-frontier.md)에 있다.

## 7. 참조 저장소(선택)

```powershell
.\scripts\fetch_references.ps1
```

`third_party/refs/`에 jsrf-recomp 등 참고 저장소를 받는다. 빌드에는 쓰지 않는다.

---

# Win32 development setup and build

Based on: [design i001](../design/20261012-i001-project-bootstrap.md), [work log i001](../work-logs/20261012-i001-project-bootstrap.md), [design i002](../design/20261012-i002-recomp-pipeline.md), [work log i002](../work-logs/20261012-i002-recomp-pipeline.md), [design i003](../design/20261012-i003-kernel-hle.md), [work log i003](../work-logs/20261012-i003-kernel-hle.md)

1. **Requirements**: Windows 10/11; Visual Studio 2022 or 2026 with the "Desktop development with C++" workload (x86/x64 MSVC tools); a graphics driver with OpenGL 3.3 or newer for the window (`REXBOX_DISPLAY=none` runs without one); CMake 3.20+ (the one bundled with Visual Studio works); Git; for the recompilation pipeline, Python 3.10+ with `pip install capstone`; a dump of a Jet Set Radio Future disc you own.
2. **Get the repository**: `git clone --recurse-submodules https://github.com/nworkers/reJSRF.git` (or `git submodule update --init` after a plain clone; the submodules are xboxrecomp, XbSymbolDatabase and SDL).
3. **Place the game files** under `roms/Jet Set Radio Future/` (gitignored; never commit them). Extract an ISO with [extract-xiso](https://github.com/XboxDev/extract-xiso) or [xdvdfs](https://github.com/antangelo/xdvdfs). `default.xbe` (USA, MD5 `f4e366bfdceee551a09f42219c90d84e`) is the primary target; `default.xbe` (Japan region) is recognized only.
4. **Generate the recompiled code**: `pip install capstone`; the first time, build the XDK identification tool with `.\scripts\build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols`; then `python scripts\recomp\regenerate.py` (into `gen/jsrf_us/`, about two minutes). It reads `roms\Jet Set Radio Future\default.xbe` unless `--xbe <path>` names another. It stops without generating anything when the file's MD5 is not the US build's (`src/game/jsrf/recomp/variants.json`), because the seeds and title bindings are US addresses. `find_pointer_targets.py` checks the same. Generated code is never committed; regenerate after changing `src/game/jsrf/decomp/` or updating the submodule. Without it the app stops at the load stage.
5. **Build**: `.\scripts\build_win32_x86.ps1` for Debug, add `-Configuration Release` for Release. Output goes under `build\win32_x86\`.
6. **Check**: run `rexbox_xbe_info.exe` on `default.xbe` and `rejsrf.exe` on the game directory (`rejsrf.exe "roms\Jet Set Radio Future"`). The app serves the folder as the game's `D:` and loads `default.xbe`; `--xbe <file>` loads another XBE (e.g. `--xbe SegaJSRF.xbe` on a Sega GT and JSRF disc). It identifies the XBE by its certificate and runs only the US build, which the recompiled code comes from, so name the US XBE with `--xbe` when `default.xbe` is another build; the app opens a 1280x960 SDL3 window (twice 640x480) at D3D8 device creation, titled `reJSRF v<version> (Win32/x86 <config>) - Build <build time> - OpenGL 3.3 - FPS : 60.0`; it resizes and maximizes with the aspect ratio kept, Alt+1/2/3 size it to 1x–3x 640x480, and a double click or Alt+Enter toggles borderless fullscreen (Alt+Enter does not reach the game as START). The app clears the window to black and should print the expected output above (current frontier: [docs/analysis/boot-frontier.md](../analysis/boot-frontier.md)). The game's hard disk is created under `hdd\` in the current folder (gitignored); set `REJSRF_HDD_ROOT` to use another place and `REXBOX_TRACE_FILES=1` to log every file open. `REXBOX_HLE_FALLBACK=lifted` runs unimplemented XDK functions as originals and logs them instead of stopping (a diagnostic for call order; it ends in a hardware-access fault); `REXBOX_DISPLAY=none` runs without a window; `REXBOX_AUDIO=none` opens no audio device (play cursors still move in real time); `REXBOX_DSOUND_TRACE=1` logs buffer formats, mix bins and `Play`. `rexbox_kernel_probe.exe` (needs no game data) should report "56 check(s), 0 failure(s)", `rexbox_dsound_probe.exe` (needs no game data or audio device) "48 check(s), 0 failure(s)", `rexbox_d3d8_probe.exe` (needs no game data or GPU) "116 check(s), 0 failure(s)", `rejsrf_decomp_check.exe` "0 difference(s)" and `rejsrf_d3d8_check.exe` (HLE exports against the lifted XDK originals; `--dump` prints the originals' answers) "14018 case(s), 0 difference(s)", all exiting 0. `python scripts\recomp\check_hle_pops.py` compares the D3D8, XInput and DSOUND exports' pop counts with the lifted originals' `ret n`, and `python scripts\recomp\find_pointer_targets.py` finds unlifted functions referenced only from data or immediates (`--write` updates the seed file; regenerate afterwards). The diagnostic environment variables for hangs and memory corruption (`REXBOX_HLE_TRACE`, `REXBOX_TRACE_MEMORY`, `REXBOX_WATCHDOG`, `REXBOX_WATCHDOG_DUMP`, `REXBOX_HEAP_CHECK`, and the `REJSRF_RECOMP_CALL_PROFILE` CMake option) are described in ARCHITECTURE.md §5; the boot now keeps running its frame loop, so the process does not end by itself (close the window with its X button or Alt+F4, press Ctrl+C, or set `REXBOX_WATCHDOG=<seconds>`; window events are handled when the game presents a frame, so a close waits until the next present; `REXBOX_FRAME_LOG=<n>` prints a line every n presents); the window shows the logo sequence and then the 3D attract demo while the frame loop runs at 60 Hz; `REXBOX_DISPLAY=hidden` draws into an invisible window, `REXBOX_FRAME_DUMP=<n>[,<n>...]` writes the n-th present to `frame_<n>.bmp` (in `REXBOX_FRAME_DUMP_DIR`, default the current folder; e.g. hidden, `REXBOX_FRAME_DUMP=600` and `REXBOX_WATCHDOG=20` give the logo in `frame_600.bmp`), and `REXBOX_D3D8_TRACE=1` logs each texture format, vertex format and shader key the first time it appears (paths not drawn yet are reported once anyway), and `REXBOX_SHADER_DUMP=1` prints each created vertex shader's declaration and disassembly and each pixel shader's definition, and `REXBOX_D3D8_PROFILE=<n>` prints every n presents where the frame time went (draw phases, present, the rest; e.g. hidden, `REXBOX_AUDIO=none`, `REXBOX_D3D8_PROFILE=300`, Release); stderr shows `[dsound] audio output: host device, 48000 Hz stereo`, `[dsound] DownloadEffectsImage: 20 effects, ...` and `[d3d8] first frame presented ...`, and no sound plays yet because the game calls no `Play` ([boot-frontier](../analysis/boot-frontier.md)). Input comes from a host gamepad, else the keyboard: arrows, Enter = START, Backspace = BACK, Z/X/C/V = A/B/X/Y, A/S = triggers, Q/W = Black/White ([kb: Xbox XInput §4](../kb/xbox-xinput.md)).
7. **Reference repositories (optional)**: `.\scripts\fetch_references.ps1` clones reference repositories such as jsrf-recomp into `third_party/refs/`. They are not built.
