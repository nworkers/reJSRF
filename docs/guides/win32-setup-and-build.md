# Win32 개발 환경 준비와 빌드

근거: [설계 i001](../design/20261012-i001-project-bootstrap.md), [작업 로그 i001](../work-logs/20261012-i001-project-bootstrap.md)

## 1. 필요한 것

* Windows 10/11
* Visual Studio 2022 또는 2026, "C++를 사용한 데스크톱 개발" 워크로드(x86/x64 MSVC 도구 포함)
* CMake 3.20 이상(Visual Studio 내장 CMake도 가능)
* Git
* 재컴파일 파이프라인용(다음 task부터): Python 3.10 이상과 `pip install capstone`
* 직접 소유한 Jet Set Radio Future 디스크의 덤프

## 2. 저장소 받기

```powershell
git clone --recurse-submodules https://github.com/nworkers/reJSRF.git
cd reJSRF
# submodule 없이 받았다면:
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

## 4. 빌드

```powershell
.\scripts\build_win32_x86.ps1                       # Debug
.\scripts\build_win32_x86.ps1 -Configuration Release
```

결과는 `build\win32_x86\<구성>\` 아래 각 타깃 디렉터리에 생긴다.

## 5. 확인

```powershell
# XBE 정보 출력
.\build\win32_x86\tools\Debug\rexbox_xbe_info.exe "roms\Jet Set Radio Future\default_eng.xbe"

# 앱: XBE 식별, guest memory 배치, HLE 등록까지 수행하고 멈춘다
.\build\win32_x86\src\app\Debug\rejsrf.exe "roms\Jet Set Radio Future\default_eng.xbe"
```

기대 출력(앱):

```text
reJSRF 0.0.0
xbe: USA (region 1)
guest memory: 64 MiB, 11 sections placed
hle: 0 exports, kernel imports 0/120 bound, 0 title bindings
recompiled code is not built yet; stopping after load
```

## 6. 참조 저장소(선택)

```powershell
.\scripts\fetch_references.ps1
```

`third_party/refs/`에 jsrf-recomp 등 참고 저장소를 받는다. 빌드에는 쓰지 않는다.

---

# Win32 development setup and build

Based on: [design i001](../design/20261012-i001-project-bootstrap.md), [work log i001](../work-logs/20261012-i001-project-bootstrap.md)

1. **Requirements**: Windows 10/11; Visual Studio 2022 or 2026 with the "Desktop development with C++" workload (x86/x64 MSVC tools); CMake 3.20+ (the one bundled with Visual Studio works); Git; for the recompilation pipeline (from the next task) Python 3.10+ with `pip install capstone`; a dump of a Jet Set Radio Future disc you own.
2. **Get the repository**: `git clone --recurse-submodules https://github.com/nworkers/reJSRF.git` (or `git submodule update --init` after a plain clone).
3. **Place the game files** under `roms/Jet Set Radio Future/` (gitignored; never commit them). Extract an ISO with [extract-xiso](https://github.com/XboxDev/extract-xiso) or [xdvdfs](https://github.com/antangelo/xdvdfs). `default_eng.xbe` (USA, MD5 `f4e366bfdceee551a09f42219c90d84e`) is the primary target; `default.xbe` (Japan region) is recognized only.
4. **Build**: `.\scripts\build_win32_x86.ps1` for Debug, add `-Configuration Release` for Release. Output goes under `build\win32_x86\`.
5. **Check**: run `rexbox_xbe_info.exe` and `rejsrf.exe` on `default_eng.xbe` as shown above; the app should print the expected output above.
6. **Reference repositories (optional)**: `.\scripts\fetch_references.ps1` clones reference repositories such as jsrf-recomp into `third_party/refs/`. They are not built.
