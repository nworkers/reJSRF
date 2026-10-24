# Linux 개발 환경 준비와 빌드

근거: [설계 i119](../design/20261025-i119-linux.md), [작업 로그 i119](../work-logs/20261025-i119-linux.md). Windows 절차는 [Win32 가이드](win32-setup-and-build.md)이고, 실행 옵션·환경 변수·설정 메뉴는 그쪽 설명이 Linux에도 그대로 맞는다.

Ubuntu 24.04(WSL2 포함), GCC 13에서 x64와 x86(`-m32`) 모두 확인했다.

## 1. 필요한 것

```bash
sudo apt-get install -y build-essential cmake ninja-build git python3-venv \
  libasound2-dev libpulse-dev libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev \
  libxtst-dev libxfixes-dev libxkbcommon-dev libwayland-dev libdecor-0-dev libdrm-dev libgbm-dev libgl-dev libegl-dev
# x86(-m32) 빌드라면 추가로 i386 개발 패키지(목록은 .github/workflows/ci.yml의 linux 작업):
sudo dpkg --add-architecture i386 && sudo apt-get update
sudo apt-get install -y gcc-multilib g++-multilib libasound2-dev:i386 libx11-dev:i386 libgl-dev:i386 # …
```

재컴파일 파이프라인에는 Python 3.10 이상과 capstone이 필요하다. Ubuntu 24.04는 시스템 `pip install`을 막으므로 가상 환경을 쓴다. Windows와 같은 버전(5.0.7)을 쓰면 생성 코드가 Windows와 같다.

```bash
python3 -m venv ~/rejsrf-venv
~/rejsrf-venv/bin/pip install capstone==5.0.7
```

## 2. 저장소와 게임 파일

```bash
git clone --recurse-submodules https://github.com/nworkers/reJSRF.git
cd reJSRF
```

게임 파일은 Windows와 같이 `roms/Jet Set Radio Future/`에 둔다(gitignore). 파일 이름의 대소문자는 디스크 그대로 두면 된다. 게임이 여는 경로는 대소문자를 가리지 않고 찾는다.

## 3. 재컴파일 코드 생성

```bash
scripts/build_linux.sh x64 release --target rexbox_xdk_symbols   # 처음 한 번: XDK 식별 도구
~/rejsrf-venv/bin/python scripts/recomp/regenerate.py              # US판 → gen/jsrf_us/
~/rejsrf-venv/bin/python scripts/recomp/regenerate.py --xbe "roms/Jet Set Radio Future/default_jpn.xbe"   # 일본판 → gen/jsrf_jp/
```

`gen/`은 Windows와 같은 형식이라, 같은 폴더를 두 OS가 함께 써도 된다(WSL에서 `/mnt/…`의 저장소를 빌드할 때 등).

## 4. 빌드

```bash
scripts/build_linux.sh <x64|x86> <debug|release> [us|jp] [--no-test] [--target <이름>]
```

* `CMakePresets.json`의 `linux-<arch>-<config>[-jp]` 프리셋으로 configure하고 빌드한 뒤, rexbox probe를 `ctest`로 돌린다(`--target`을 주면 건너뛴다).
* 결과는 `build/linux-<arch>-<config>[-jp]/`에 생긴다. 앱은 `src/app/rejsrf`, rexbox 도구와 probe는 `third_party/rexbox/tools/`, 타이틀 도구는 `tools/`에 있다.
* `jp`는 일본판 디스크용 빌드다(`-DREJSRF_VARIANT=jsrf_jp`, `gen/jsrf_jp` 필요).
* 프리셋을 직접 써도 된다: `cmake --preset linux-x64-release && cmake --build --preset linux-x64-release`.

## 5. 실행과 확인

```bash
build/linux-x64-release/src/app/rejsrf "roms/Jet Set Radio Future"
build/linux-x64-release/tools/rejsrf_decomp_check                  # "129 case(s), 0 difference(s)"
build/linux-x64-release/tools/rejsrf_d3d8_check "roms/Jet Set Radio Future/default.xbe"   # "14018 case(s), 0 difference(s)"
```

* 창, 설정 메뉴(Tab), 언어(한국어 폴더 포함), 키보드·게임패드 입력, 환경 변수는 [Win32 가이드](win32-setup-and-build.md)와 같다.
* 설정 파일 `rejsrf.ini`는 실행 파일 옆에 저장된다. 그 폴더에 쓸 수 없으면 SDL이 정하는 사용자 폴더(`~/.local/share/` 아래)에 저장된다. HDD는 현재 폴더의 `hdd/`다(`REJSRF_HDD_ROOT`로 바꾼다).
* 소리는 SDL3가 PulseAudio·PipeWire·ALSA 중 있는 것으로 낸다.

## 6. WSL2에서

* 창과 소리는 WSLg가 Windows 데스크톱에 보여 준다.
* 기본 GL은 소프트웨어 렌더러(llvmpipe)라 느리다. GPU를 쓰려면 Mesa의 D3D12 드라이버를 고른다: `GALLIUM_DRIVER=d3d12 build/linux-x64-release/src/app/rejsrf …`. stderr의 `[d3d8] display …` 줄에 GPU 이름이 나오면 된 것이다.
* 저장소를 Windows 디스크(`/mnt/c`, `/mnt/e`)에 두면 빌드가 느리다. 빌드 폴더만 WSL 디스크에 두려면 `cmake -S . -B ~/build/linux-x64-release -G Ninja -DCMAKE_BUILD_TYPE=Release`처럼 직접 configure한다.

## 7. 아직 없는 것

* 샘플링 프로파일러(`REXBOX_SAMPLE`)는 rexbox의 Linux 계층에 아직 없다.
* soft-dirty 페이지 추적이 없는 커널(WSL2)에서는 D3D8이 정점·인덱스 버퍼를 늘 비교한다. 결과는 같고 조금 느리다.
* 실제 Linux 데스크톱(X11, Wayland, 실제 GPU 드라이버)은 아직 확인하지 않았다. WSLg에서만 확인했다.

---

# Linux development setup and build

Basis: [design i119](../design/20261025-i119-linux.md), [work log i119](../work-logs/20261025-i119-linux.md). The Windows procedure is the [Win32 guide](win32-setup-and-build.md); its descriptions of run options, environment variables and the settings menu apply to Linux as they are.

Checked on Ubuntu 24.04 (WSL2 included) with GCC 13, for both x64 and x86 (`-m32`).

1. **Requirements**
   * The packages in the first block above: build tools, CMake, Ninja, `python3-venv`, and the development libraries SDL3 compiles against.
   * For an x86 build, also `gcc-multilib`, `g++-multilib` and the `:i386` development packages after `dpkg --add-architecture i386`; the list is in the `linux` job of `.github/workflows/ci.yml`.
   * The recompilation pipeline needs Python 3.10+ with capstone. Ubuntu 24.04 refuses a system-wide `pip install`, so use a virtual environment (`python3 -m venv ~/rejsrf-venv`, then `~/rejsrf-venv/bin/pip install capstone==5.0.7`). Windows uses the same version, 5.0.7, so the generated code is the same as on Windows.
2. **Repository and game files**
   * Clone with `git clone --recurse-submodules https://github.com/nworkers/reJSRF.git`.
   * Put the game files under `roms/Jet Set Radio Future/` (gitignored), as on Windows. Keep file names as on the disc; the paths the game opens are matched without regard to case.
3. **Generate the recompiled code**
   * The first time, build the XDK identification tool: `scripts/build_linux.sh x64 release --target rexbox_xdk_symbols`.
   * Then run `~/rejsrf-venv/bin/python scripts/recomp/regenerate.py` for the US build (`gen/jsrf_us/`), and add `--xbe "roms/Jet Set Radio Future/default_jpn.xbe"` for the Japanese one (`gen/jsrf_jp/`).
   * `gen/` has the same format as on Windows, so both systems can share one folder (for example when WSL builds a checkout under `/mnt/…`).
4. **Build**: `scripts/build_linux.sh <x64|x86> <debug|release> [us|jp] [--no-test] [--target <name>]`.
   * It configures and builds through the `linux-<arch>-<config>[-jp]` presets of `CMakePresets.json`, then runs rexbox's probes with `ctest` (skipped when `--target` is given).
   * Output goes to `build/linux-<arch>-<config>[-jp]/`: the app is `src/app/rejsrf`, rexbox's tools and probes are under `third_party/rexbox/tools/`, and the title tools under `tools/`.
   * `jp` builds for the Japanese disc (`-DREJSRF_VARIANT=jsrf_jp`; needs `gen/jsrf_jp`).
   * The presets also work directly: `cmake --preset linux-x64-release && cmake --build --preset linux-x64-release`.
5. **Run and check**
   * Run `build/linux-x64-release/src/app/rejsrf "roms/Jet Set Radio Future"`.
   * `tools/rejsrf_decomp_check` should report "129 case(s), 0 difference(s)" and `tools/rejsrf_d3d8_check "roms/Jet Set Radio Future/default.xbe"` "14018 case(s), 0 difference(s)".
   * The window, settings menu (Tab), languages (the Korean folder included), keyboard and gamepad input, and environment variables are as in the Win32 guide.
   * `rejsrf.ini` is saved next to the executable, or in the user folder SDL picks (under `~/.local/share/`) when that one is not writable. The HDD is `hdd/` in the current folder (`REJSRF_HDD_ROOT` moves it).
   * Sound goes through whichever of PulseAudio, PipeWire or ALSA SDL3 finds.
6. **On WSL2**
   * WSLg shows the window and plays the sound on the Windows desktop.
   * The default GL is a software renderer (llvmpipe) and slow. Pick Mesa's D3D12 driver for the GPU: `GALLIUM_DRIVER=d3d12 build/linux-x64-release/src/app/rejsrf …`. The GPU's name then appears in stderr's `[d3d8] display …` line.
   * A checkout on a Windows disk (`/mnt/c`, `/mnt/e`) builds slowly. To keep only the build folder on WSL's disk, configure directly, for example `cmake -S . -B ~/build/linux-x64-release -G Ninja -DCMAKE_BUILD_TYPE=Release`.
7. **Not there yet**
   * The sampling profiler (`REXBOX_SAMPLE`) does not exist in rexbox's Linux layer yet.
   * On a kernel without soft-dirty page tracking (WSL2), D3D8 always compares vertex and index buffers; same result, a little slower.
   * A real Linux desktop (X11, Wayland, a real GPU driver) is not checked yet; only WSLg is.
