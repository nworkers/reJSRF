# Issue #25 작업 로그 — 소리가 나지 않는 문제: 호스트 스레드 스택과 자기 일시정지

작업 지시: [20261013-i025](../work-orders/20261013-i025-audio-host-threads.md) · 설계: [20261013-i025](../design/20261013-i025-audio-host-threads.md)

## 요약

v0.0.10에서는 오디오 출력 스레드가 만들어지지 않아 재생 커서가 멈춰 있었다. 원인은 `rejsrf.exe`의 64 MiB 이미지 기본 스택이다. 32비트 프로세스에서 GL 드라이버·SDL 스레드까지 각 64 MiB를 예약하면서 주소 공간이 바닥났다. `/LARGEADDRESSAWARE`와 1 MiB 기본 스택으로 바꾸자 출력이 열리고, 숨김 창의 간헐 멈춤(8번 중 1번)도 사라졌다. 커널 HLE는 이제 자기 자신에 대한 `NtSuspendThread`를 지킨다. 게임의 프레임 스레드는 매 프레임 자기 일시정지 후 vblank 스레드의 재개를 기다린다.

소리는 여전히 나지 않는다. 게임이 `Play`를 부르지 않기 때문이다. 부팅은 "Presented by SEGA" 로고에 머물고, 주 스레드는 막혀 있지 않다(매 프레임 감마 램프와 그리기). 로고를 끝내는 조건은 다음 task에서 본다.

## 과정

1. DSOUND 추적(`REXBOX_DSOUND_TRACE`, `REXBOX_HLE_TRACE`)에서 `no audio output (... the pacing thread cannot start: resource unavailable try again)`를 보았다. `dumpbin`으로 스택 예약 64 MiB와 `LARGEADDRESSAWARE` 없음을 확인했다. 게스트 스레드는 `CreateHostThread`로 스택을 명시하므로 이미지 기본값이 필요 없다.
2. 커널 호출 수 1·2위가 `NtResumeThread`/`NtSuspendThread`였다. 게임 코드에서 `SuspendThread`(`0x00147DAC`)·`ResumeThread`(`0x00147DD2`) 호출자를 찾아 프레임 스레드의 자기 일시정지와 vblank 스레드의 재개를 확인했다.
3. 자기 일시정지를 지키게 한 뒤 프로파일 빌드(`REJSRF_RECOMP_CALL_PROFILE=ON`, 별도 폴더 `build/prof`)로 스레드별 호출을 보았다. 주 스레드의 상위 호출은 감마 램프 계산(`0x0014CCA0` clamp, `0x0014C6A0` float→int, 인자 0..255)과 `SetGammaRamp`, 로고 그리기였다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Debug / Release 빌드 | 성공, 우리 타깃 경고 0 |
| `rexbox_kernel_probe` / `rexbox_dsound_probe` / `rexbox_d3d8_probe` | 42/0, 48/0, 73/0 |
| 부팅(호스트 오디오) | `[dsound] audio output: host device, 48000 Hz stereo`(Release 3번, Debug 1번) |
| 부팅(`REXBOX_AUDIO=none`) | `audio output: none ...; cursors run in real time` |
| 숨김 창 반복 | 8번 연속 첫 프레임 도달(수정 전 8번 중 1번 멈춤) |
| 일시정지 호출 | 핸들 `0x140`의 `NtSuspendThread`가 초당 약 84번(vblank 주기) |

## 남은 일

* 로고에서 넘어가지 않는 이유(다음 task). 감마 램프가 페이드에 쓰이므로 `SetGammaRamp`를 표시에 반영하는 것도 함께 본다.
* 다른 실행 중 스레드의 일시정지(안전 지점 필요).
* 조사 중 실행 파일을 잡고 끝나지 않는 좀비 프로세스(앞선 숨김 창 멈춤에서 남음)가 하나 생겼다. 강제 종료가 되지 않아 링크 전에 실행 파일 이름을 바꿔 피했다. 재부팅하면 사라진다.

---

# Issue #25 work log — no sound: host-thread stacks and self-suspension

Work order: [20261013-i025](../work-orders/20261013-i025-audio-host-threads.md) · Design: [20261013-i025](../design/20261013-i025-audio-host-threads.md)

## Summary

In v0.0.10 the audio output thread could not be created, so the play cursors stood still. The cause was `rejsrf.exe`'s 64 MiB image-default stack: in a 32-bit process, the GL driver and SDL threads also reserved 64 MiB each, and the address space ran out. With `/LARGEADDRESSAWARE` and a 1 MiB default the output opens, and the intermittent hidden-window stall (one run in eight) is gone. The kernel HLE now honors `NtSuspendThread` on the calling thread. The game's frame thread suspends itself every frame and waits for the vblank thread to resume it.

There is still no sound, because the game calls no `Play`. The boot stays on the "Presented by SEGA" logo, and the main thread is not blocked (gamma ramp and drawing every frame). What ends the logo is the next task.

## Process

1. The DSOUND trace (`REXBOX_DSOUND_TRACE`, `REXBOX_HLE_TRACE`) showed `no audio output (... the pacing thread cannot start: resource unavailable try again)`. `dumpbin` showed the 64 MiB stack reserve and no `LARGEADDRESSAWARE`. Guest threads get explicit stacks through `CreateHostThread`, so the image default is not needed.
2. `NtResumeThread`/`NtSuspendThread` were the two most frequent kernel calls. The callers of `SuspendThread` (`0x00147DAC`) and `ResumeThread` (`0x00147DD2`) in game code showed the frame thread suspending itself and the vblank thread resuming it.
3. With self-suspension honored, a profiling build (`REJSRF_RECOMP_CALL_PROFILE=ON`, separate folder `build/prof`) showed the per-thread calls. The main thread's top calls are the gamma-ramp computation (`0x0014CCA0` clamp, `0x0014C6A0` float→int, arguments 0..255), `SetGammaRamp` and the logo draw.

## Verification

The table in the Korean section applies: Debug and Release builds with zero warnings on our targets; probes 42/0, 48/0 and 73/0; `[dsound] audio output: host device, 48000 Hz stereo` in boots (three Release, one Debug); `audio output: none ...; cursors run in real time` with `REXBOX_AUDIO=none`; eight hidden-window runs in a row reaching the first frame (one in eight stalled before); `NtSuspendThread` on handle `0x140` at about 84 per second (the vblank rate).

## Remaining

* Why the boot does not move past the logo (next task). The gamma ramp drives the fades, so applying `SetGammaRamp` to the display is looked at together with it.
* Suspending another running thread (needs a safe point).
* During the investigation, a zombie process left over from an earlier hidden-window stall kept the executable locked and could not be killed; the executable was renamed before linking. A reboot clears it.
