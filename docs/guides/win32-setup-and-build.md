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
  default.xbe        Japan-region build (recognized only)
  default_eng.xbe    USA build, MD5 f4e366bfdceee551a09f42219c90d84e (primary target)
  Media/...
```

## 4. 재컴파일 코드 생성

```powershell
pip install capstone
# 처음 한 번: XDK 함수 식별 도구를 먼저 빌드한다
.\scripts\build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols
python scripts\recomp\regenerate.py      # gen/jsrf_us/ 에 생성, 약 2분
```

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
.\build\win32_x86\tools\Debug\rexbox_xbe_info.exe "roms\Jet Set Radio Future\default_eng.xbe"

# 앱: XBE 식별, 배치, HLE 등록 후 재컴파일 코드를 진입점부터 실행하고 멈춘 지점을 보고한다.
# D3D8 디바이스 생성 시 640x480 SDL3 창이 열리고 검게 지워진다(멈추면 프로세스와 함께 닫힌다).
.\build\win32_x86\src\app\Debug\rejsrf.exe "roms\Jet Set Radio Future\default_eng.xbe"
```

기대 출력(앱, 재컴파일 코드 포함 빌드). 현재 frontier는 [docs/analysis/boot-frontier.md](../analysis/boot-frontier.md):

```text
reJSRF 0.0.8
xbe: USA (region 1)
guest memory: 64 MiB, 11 sections placed, 62940 KiB free
hle: 283 exports, kernel imports 107/120 bound, 43 title bindings
boot: entry 00148023, flat dispatch
thread 1 (start 00148023): returned, eax 00000001
thread 2 (start 00147EBB): fault C0000005, read at guest 0FE80026, outside guest RAM (host ...)
```

(첫 프레임과 폰트·효과음 버퍼 생성 뒤, 약 50초 만에 `D3DVertexBuffer_Lock` 다음의 fault로 끝나는 현재 frontier다. 그 전에 멈추게 하려면 `REXBOX_WATCHDOG=<초>`를 쓴다. 입력은 호스트 게임패드, 없으면 키보드다: 방향키, Enter = START, Backspace = BACK, Z/X/C/V = A/B/X/Y, A/S = 트리거, Q/W = Black/White. [kb: Xbox XInput §4](../kb/xbox-xinput.md))

stderr에는 `[d3d8] Direct3D_CreateDevice: 640x480 format 11, ...`, `[d3d8] display 640x480, OpenGL 3.3, <GPU 이름>`, `[dsound] audio output: host device, 48000 Hz stereo`, `[dsound] DirectSoundCreate -> ...`, `[dsound] DownloadEffectsImage: 20 effects, ...`, `[d3d8] first frame presented ...` 줄이 나온다. 게임이 버퍼를 만들지만 fault 전까지 `Play`를 부르지 않으므로 소리는 아직 나지 않는다. 오디오 장치가 열렸는지는 `audio output` 줄로 확인한다.

게임의 HDD는 현재 폴더의 `hdd\`(gitignore)에 만들어진다. 다른 곳을 쓰려면 `REJSRF_HDD_ROOT`를 지정한다. 모든 파일 열기를 보려면 `REXBOX_TRACE_FILES=1`을 켠다. `REXBOX_HLE_FALLBACK=lifted`이면 구현 없는 XDK 함수를 멈추지 않고 원본대로 실행하며 이름을 기록한다(호출 순서를 보기 위한 진단 모드로, 결국 하드웨어 접근에서 fault가 난다). `REXBOX_DISPLAY=none`이면 창을 만들지 않는다(헤드리스 진단). `REXBOX_AUDIO=none`이면 오디오 장치를 열지 않는다(재생 커서는 실시간으로 움직인다). `REXBOX_DSOUND_TRACE=1`이면 버퍼 생성의 형식·믹스 빈과 `Play`를 기록한다.

시험 도구:

```powershell
.\build\win32_x86\tools\Debug\rexbox_kernel_probe.exe   # "0 failure(s)", exit code 0 (게임 데이터 불필요)
.\build\win32_x86\tools\Debug\rexbox_dsound_probe.exe   # DSOUND HLE: "48 check(s), 0 failure(s)", exit code 0 (게임 데이터·오디오 장치 불필요)
.\build\win32_x86\tools\Debug\rejsrf_decomp_check.exe   # "0 difference(s)", exit code 0
.\build\win32_x86\tools\Debug\rejsrf_d3d8_check.exe     # D3D8 HLE vs. 원본: "13557 case(s), 0 difference(s)", exit code 0
.\build\win32_x86\tools\Debug\rejsrf_d3d8_check.exe --dump   # 원본의 모드 표·Caps·포맷 판정 출력
```

`rejsrf_d3d8_check`는 XBE가 `roms\Jet Set Radio Future\default_eng.xbe`에 있다고 가정하며, 다른 경로는 인자로 준다. `python scripts\recomp\check_hle_pops.py`는 D3D8·XInput·DSOUND HLE export의 pop 개수와 lift 원본의 `ret n`을 대조한다. `python scripts\recomp\find_pointer_targets.py`는 데이터나 즉시값으로만 참조되어 lift되지 않은 함수를 찾는다(`--write`로 seed 파일 갱신 뒤 재생성).

멈춤이나 메모리 손상을 추적하는 진단 환경 변수는 [ARCHITECTURE.md §5](../../ARCHITECTURE.md)에 있다(`REXBOX_HLE_TRACE`, `REXBOX_TRACE_MEMORY`, `REXBOX_WATCHDOG`, `REXBOX_WATCHDOG_DUMP`, `REXBOX_HEAP_CHECK`; 직접 호출 프로파일은 `cmake -DREJSRF_RECOMP_CALL_PROFILE=ON`으로 생성 코드를 빌드했을 때). 기대 실행은 이제 `dsstdfx.bin` 읽기의 사용자 APC 대기에서 멈춘다([boot-frontier](../analysis/boot-frontier.md)).

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
3. **Place the game files** under `roms/Jet Set Radio Future/` (gitignored; never commit them). Extract an ISO with [extract-xiso](https://github.com/XboxDev/extract-xiso) or [xdvdfs](https://github.com/antangelo/xdvdfs). `default_eng.xbe` (USA, MD5 `f4e366bfdceee551a09f42219c90d84e`) is the primary target; `default.xbe` (Japan region) is recognized only.
4. **Generate the recompiled code**: `pip install capstone`; the first time, build the XDK identification tool with `.\scripts\build_win32_x86.ps1 -Configuration Release -Target rexbox_xdk_symbols`; then `python scripts\recomp\regenerate.py` (into `gen/jsrf_us/`, about two minutes). Generated code is never committed; regenerate after changing `src/game/jsrf/decomp/` or updating the submodule. Without it the app stops at the load stage.
5. **Build**: `.\scripts\build_win32_x86.ps1` for Debug, add `-Configuration Release` for Release. Output goes under `build\win32_x86\`.
6. **Check**: run `rexbox_xbe_info.exe` and `rejsrf.exe` on `default_eng.xbe`; the app opens a 640x480 SDL3 window at D3D8 device creation, clears it to black and should print the expected output above (current frontier: [docs/analysis/boot-frontier.md](../analysis/boot-frontier.md)). The game's hard disk is created under `hdd\` in the current folder (gitignored); set `REJSRF_HDD_ROOT` to use another place and `REXBOX_TRACE_FILES=1` to log every file open. `REXBOX_HLE_FALLBACK=lifted` runs unimplemented XDK functions as originals and logs them instead of stopping (a diagnostic for call order; it ends in a hardware-access fault); `REXBOX_DISPLAY=none` runs without a window; `REXBOX_AUDIO=none` opens no audio device (play cursors still move in real time); `REXBOX_DSOUND_TRACE=1` logs buffer formats, mix bins and `Play`. `rexbox_kernel_probe.exe` (needs no game data) should report "0 failure(s)", `rexbox_dsound_probe.exe` (needs no game data or audio device) "48 check(s), 0 failure(s)", `rejsrf_decomp_check.exe` "0 difference(s)" and `rejsrf_d3d8_check.exe` (HLE exports against the lifted XDK originals; `--dump` prints the originals' answers) "13557 case(s), 0 difference(s)", all exiting 0. `python scripts\recomp\check_hle_pops.py` compares the D3D8, XInput and DSOUND exports' pop counts with the lifted originals' `ret n`, and `python scripts\recomp\find_pointer_targets.py` finds unlifted functions referenced only from data or immediates (`--write` updates the seed file; regenerate afterwards). The diagnostic environment variables for hangs and memory corruption (`REXBOX_HLE_TRACE`, `REXBOX_TRACE_MEMORY`, `REXBOX_WATCHDOG`, `REXBOX_WATCHDOG_DUMP`, `REXBOX_HEAP_CHECK`, and the `REJSRF_RECOMP_CALL_PROFILE` CMake option) are described in ARCHITECTURE.md §5; the boot now passes the first frame, fonts and sound-effect buffer creation and ends after about 50 s with thread 2 faulting at `0FE80026` after `D3DVertexBuffer_Lock` (`REXBOX_WATCHDOG=<seconds>` stops it earlier); stderr shows `[dsound] audio output: host device, 48000 Hz stereo`, `[dsound] DownloadEffectsImage: 20 effects, ...` and `[d3d8] first frame presented ...`, and no sound plays yet because the game calls no `Play` before the fault ([boot-frontier](../analysis/boot-frontier.md)). Input comes from a host gamepad, else the keyboard: arrows, Enter = START, Backspace = BACK, Z/X/C/V = A/B/X/Y, A/S = triggers, Q/W = Black/White ([kb: Xbox XInput §4](../kb/xbox-xinput.md)).
7. **Reference repositories (optional)**: `.\scripts\fetch_references.ps1` clones reference repositories such as jsrf-recomp into `third_party/refs/`. They are not built.
