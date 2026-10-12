# DSOUND: 게임이 부르는 함수, 원본 의미, 효과 이미지, 부팅에서 본 호출

JSRF(US)에 정적 링크된 DSOUND 섹션(`0x0019E340`, 0x1C55C바이트)과 게임 쪽 호출을 정리한다. 일반 지식은 [kb: Xbox DirectSound](../kb/xbox-dsound.md), 설계는 [i017](../design/20261012-i017-dsound-hle.md).

This collects the facts about the DSOUND section statically linked into JSRF (US) (`0x0019E340`, 0x1C55C bytes) and the game's calls into it. General knowledge is in [kb: Xbox DirectSound](../kb/xbox-dsound.md), the design in [i017](../design/20261012-i017-dsound-hle.md).

## 1. 게임 → DSOUND 호출 / Game calls into DSOUND

* **확인됨**: `.text`에서 DSOUND 섹션으로 가는 `call`/`jmp rel32`(바이트 스캔)는 이름 있는 함수 50개와 이름 없는 대상 3개에 닿는다. `jmp`는 `IDirectSound_GetSpeakerConfig`(1)와 `IDirectSoundBuffer_SetFrequency`(2)의 꼬리 호출이다. 호출 수가 많은 것: `IDirectSound_SetEffectData` 30, `GetEffectData` 20, `IDirectSoundBuffer_SetCurrentPosition` 11, `Release` 10, `IDirectSound_Release` 9, `GetCurrentPosition` 7. 함수 목록과 `ret n`은 설계 §2의 표.
* **확인됨**: `.rdata`/`.data`의 DSOUND 함수 포인터는 DSOUND 자신의 vtable 9개(`DSound_CRefCount_AddRef/Release`)뿐이다. 게임은 DSOUND vtable을 직접 부르지 않는다.
* **확인됨**: 이름 없는 대상. `0x0019E4BC`(`xor eax,eax; ret 0xC`)는 `0x001684C5`에서 `DirectSoundCreate` 직후 `(ds, hwnd, 3)`으로, `0x00116EFC`에서 다른 COM 객체에 `(obj, 0, 2)`로 불린다(같은 바이트의 함수가 병합됨). `0x0019E4C1`(`xor eax,eax; ret 4`)은 `0x001684D3`, `0x00168516`. `0x0019E523`은 `DirectSoundUseFullHRTF`로, `DirectSoundEnterCriticalSection` 아래에서 `0x001BA080`~`0x001BA09C`에 HRTF 함수 8개를 써 넣는다. 임계 구역 `0x001BA050`은 DSOUND 섹션 안에 정적으로 초기화되어 있다(`01 00 04 00 …`, 리스트 머리가 자기 자신, LockCount −1). 셋 다 lift된 원본으로 둔다.
* **Confirmed**: byte-scanning `.text` for `call`/`jmp rel32` into the DSOUND section finds 50 named functions and three unnamed targets; the `jmp`s are tail calls to `IDirectSound_GetSpeakerConfig` (1) and `IDirectSoundBuffer_SetFrequency` (2). The most-called: `IDirectSound_SetEffectData` 30, `GetEffectData` 20, `IDirectSoundBuffer_SetCurrentPosition` 11, `Release` 10, `IDirectSound_Release` 9, `GetCurrentPosition` 7. The list and `ret n` are in design §2. The only DSOUND function pointers in `.rdata`/`.data` are DSOUND's own nine vtables (`DSound_CRefCount_AddRef/Release`); the game never calls DSOUND vtables. The unnamed targets: `0x0019E4BC` (`xor eax,eax; ret 0xC`), called at `0x001684C5` right after `DirectSoundCreate` with `(ds, hwnd, 3)` and at `0x00116EFC` on another COM object with `(obj, 0, 2)` (identical functions folded); `0x0019E4C1` (`xor eax,eax; ret 4`) at `0x001684D3` and `0x00168516`; `0x0019E523` is `DirectSoundUseFullHRTF`, writing eight HRTF function pointers to `0x001BA080`–`0x001BA09C` under `DirectSoundEnterCriticalSection`; its critical section `0x001BA050` is statically initialized inside the DSOUND section (`01 00 04 00 …`, list heads pointing at themselves, LockCount −1). All three stay lifted.

## 2. 원본 의미 / Original semantics

| 함수 / Function | 확인한 동작 / Confirmed behavior |
|---|---|
| `CDirectSoundBuffer_Lock` (`0x0019ECF8`) | 플래그 1: `off` = `GetCurrentPosition`의 쓰기 커서. 플래그 2: 크기 = 버퍼 크기(`voice+0xBC`). `*p1 = data(voice+0xB8) + off`, `*n1 = min(n, size − off)`; `p2`·`n2`가 모두 있으면 남는 부분 `(data, n − n1)` 아니면 `(0, 0)` |
| `IDirectSoundBuffer_Unlock` (`0x0019E4F2`) | `xor eax,eax; ret 0x14` |
| `CMcpxBuffer_GetStatus` (`0x001A4391`) | 상태 비트 `1`과 `0x8002` 중 하나 → `PLAYING`(1); `0x200` → 5 |
| `CMcpxBuffer_GetCurrentPosition` (`0x001A43DA`) | 재생 중(상태 `& 3 == 3`): 재생 = 하드웨어 위치, 쓰기 = 재생 + max(32표본 바이트, `+0x14`); 반복(`0x200`)이고 재생이 구간 안이면 쓰기 = 구간 시작 + 쓰기 % 구간 길이, 아니면 쓰기 % 크기. 정지 중: 둘 다 `+0x108` |
| `CMcpxBuffer_Stop` (`0x001A46F0`) | 즉시 정지면 현재 위치를 `+0x108`에 저장(MIXIN/FXIN `0x82000` 제외) |
| `CMcpxBuffer_Play` (`0x001A490D`) | 시작 = (`flags & 2` 또는 상태 `& 2`) ? 0 : `+0x108`; 그다음 `+0x108 = 0` |
| `CMcpxBuffer_SetCurrentPosition` (`0x001A49A7`) | 재생 중이면 하드웨어 위치 설정(반복 끝을 넘으면 다시 Play), 아니면 `+0x108 = pos` |
| `CMcpxBuffer_SetBufferData` (`0x001A4B11`) | `+0x108 = 0` |
| `CDirectSound_GetSpeakerConfig` (`0x0019E6BD`) | 저장된 값 `& 0x7FFFFFFF` |
| `CDirectSound_GetEffectData` → `0x001A1B18` | `0x001A18DE`가 `index ≤ count − 1`을 검사(아니면 `0x88780032`), 서술자 `+8 + index × 0x20 + 8`의 상태 포인터 + `offset`에서 `rep movs` |

The table above applies. In particular, `Unlock` does nothing, `Stop` saves the position and `Play` resumes from it unless `FROMSTART`, a one-shot that ran out reads as 0, and `GetEffectData` copies from the descriptor's state pointer after an index check that returns `DSERR_INVALIDCALL`.

* **미확정**: `CMcpxBuffer_Play`는 음성 상태 비트 2가 서 있으면 시작 위치로 0을 고른다. 상태 비트 2가 "재생 중"이면 재생 중인 버퍼에 다시 `Play`하면 처음부터 다시 시작한다는 뜻이다. HLE는 위치를 유지한다(설계 §4).
* **Unresolved**: `CMcpxBuffer_Play` picks 0 as the start when voice state bit 2 is set; if that bit means "playing", a second `Play` restarts a playing buffer from the beginning. The HLE keeps the position (design §4).

## 3. 효과 이미지 `dsstdfx.bin` / The effects image

* **확인됨**: 게임 함수 `0x00118C40` 근처가 `0x00168130("D:\Media\Sounds\dsstdfx.bin")`을 부르고 결과 서술자를 객체 `+0x48`에 둔다. `0x00168130`은 게임의 파일 객체로 파일을 읽고(vtable `+0x1C` 크기, `+0x10` 읽기) `IDirectSound_DownloadEffectsImage(ds, image, size, loc, &desc)`를 부른다.
* **확인됨**(`0x001A19D6`, `0x001A1769`): 헤더 `+0x800` = `{0, 0xDB8, 0x3EF8, 0xD1E, 3, 0}`. 코드 0xDB8워드(`0x818`~`0x3EF8`), 상태 0xD1E워드(`0x3EF8`~`0x7370`), 효과 표 `0x7370`: 효과 20개, 스크래치 0xC2000바이트. 효과 0과 16은 코드 0x12EC바이트·상태 0x8A8·스크래치 0x5CC60, 그 밖은 코드 0x90–0x32C바이트·상태 0x68–0x174바이트. 표 뒤 `0x75F8`에 효과별 8바이트 키 20개(160바이트, 파일 끝). 원본은 키로 코드 구간만 복호화하고 상태는 평문 그대로 X 메모리에 둔다.
* **확인됨**: 원본은 포인터를 APU MMIO로 옮긴다: Y 메모리 `+= 0xFE836000`, 스크래치 `+= scratch_base − 0xC000`, 상태 `+= (0xFFA0BE7A − code_words) × 4`, 코드 `+= dsp_base − 0x17C6818`.
* **확인됨**: 게임의 `GetEffectData` 호출(예: `0x0016C101`~)은 효과 번호와 오프셋 0x1C–0x28, 크기 4–0x2C로 상태를 읽어 자기 구조체에 캐시한다.

The effects image facts above are confirmed: the header, the code (0xDB8 words) and state (0xD1E words) regions, the 20-effect table at `0x7370` with 0xC2000 bytes of scratch, the 20 per-effect keys after it, the relocation into APU MMIO, and the game's cached `GetEffectData` reads.

## 4. 버퍼를 만드는 게임 코드 / The game's buffer creation

* **확인됨**: 6곳(`0x00141434`, `0x0016C78F`, `0x0016C922`, `0x0016CD29`, `0x0016CECD`, `0x00170F8E`). `0x00141434`은 PCM 1채널(또는 인자)·44100 Hz·16비트, 플래그 `0x000400A0`(LOCDEFER | CTRLVOLUME | CTRLFREQUENCY), 조건에 따라 `0x000400B0`(+ CTRL3D). 나머지는 게임 구조체의 형식 포인터를 쓰고, 3D가 아니고 객체 플래그 `0x10000`이 서 있으면 `DSMIXBINS {2, 0x0022EBC4}`(`0x00170F8E`는 `0x0022EBD4`)를 붙인다. `0x00170F8E`의 버퍼 크기는 `(n >> 11 << 11) × 2`(2 KiB 단위 두 배, 스트리밍 이중 버퍼로 보임).
* **추정**: `.data`의 정적 값은 두 표 모두 `{2, 0}, {2, 0}`(가운데 빈 두 번)이다. 실행 중 바뀌는지는 버퍼 생성이 일어난 뒤 `REXBOX_DSOUND_TRACE`로 확인한다.
* **Confirmed**: six sites. `0x00141434` builds PCM, 44100 Hz, 16-bit with flags `0x000400A0` (LOCDEFER | CTRLVOLUME | CTRLFREQUENCY), or `0x000400B0` (+ CTRL3D). The others take the format pointer from a game structure and, for non-3D buffers whose object flag `0x10000` is set, attach `DSMIXBINS {2, 0x0022EBC4}` (`0x0022EBD4` at `0x00170F8E`). The buffer size at `0x00170F8E` is `(n >> 11 << 11) × 2` (twice a 2 KiB multiple, apparently a streaming double buffer). **Inferred**: both tables' static `.data` values are `{2, 0}, {2, 0}` (the center bin twice); whether they change at run time is to be seen with `REXBOX_DSOUND_TRACE` once buffers are created.

## 5. 부팅에서 본 것 / Seen at boot (issue #17)

* **확인됨**: `DirectSoundCreate`(HLE) → `0x0019E523`(lift) → `0x0019E4BC`(lift) → `0x0019E4C1`(lift)를 지난다. 호스트 장치는 48 kHz 스테레오로 열리고, `REXBOX_AUDIO=none`이면 null 출력이 커서를 움직인다.
* **확인됨**: 그 뒤 게임은 스레드 4개(`0x0013B180`, `0x0013B1C0`, `0x0013B230`, `0x0013B2A0`; `push offset`으로만 참조)와 파일 읽기 메서드 `0x00178F40`(`.rdata` vtable `0x001E407C`)을 부르는데, 디스어셈블러가 놓친 함수라 처음에는 건너뛰어졌다. 포인터로만 참조되는 함수 162개를 `seed_pointer_targets.json`(`scripts/recomp/find_pointer_targets.py`)으로 넣어 lift 함수가 8,852 → 9,015개, 미해결 스텁 109 → 105개가 되었다.
* **확인됨**: 현재 정지점: 게임이 `D:\Media\Sounds\dsstdfx.bin`을 `NtReadFile(h, NULL, ApcRoutine, …)`로 읽은 뒤 XAPI `WaitForSingleObjectEx`(`0x00145B5F`, `NtWaitForSingleObjectEx(0x1B0, UserMode, bAlertable, …)`)로 이벤트 `0x1B0`을 기다리며 멈춘다(watchdog). 이벤트는 완료 루틴이 신호할 것으로 보인다(**추정**: `bAlertable` = TRUE). 커널 HLE는 사용자 APC를 전달하지 않는다([boot-frontier](boot-frontier.md)).
* **확인됨**: 포인터 대상 후보 3개(`0x0001FD10`, `0x00020F10`, `0x0007BE30`)는 앞 함수의 switch 점프 표·색인 표 뒤에 있다. 선형 스윕이 그 표를 코드로 읽어 seed가 "명령 중간"으로 거부된다. `0x0007BE30`은 실행에서 닿아(issue #19) `observed` seed로 넣었다.

### 5.1 사용자 APC 뒤 / After user APCs (issue #19)

* **확인됨**: `DownloadEffectsImage`는 효과 20개, 스크래치 794,624바이트(0xC2000)로 서술자를 만든다(표와 같음).
* **확인됨**: 첫 버퍼 4개는 `0x00141434` 꼴의 스트리밍 버퍼다: 플래그 `0x000400A0`, 65,536바이트, PCM 2채널 44,100 Hz 16비트, 믹스 빈 없음. 각각 `Lock`, `Unlock`, `SetVolume`, `SetHeadroom`을 부르지만 fault 전까지 `Play`는 없다.
* **확인됨**: 폰트 뒤 효과음 버퍼 10개: 플래그 `0x00040000`, `dwBufferBytes` 0(데이터는 나중에 `SetBufferData`로 줄 것으로 보임), Xbox ADPCM 22,050 Hz(2채널 블록 72바이트 8개, 1채널 36바이트 1개)와 PCM 2채널 44,100 Hz 1개. 모두 믹스 빈 없음. HLE가 디코드할 수 있는 형식이다. `IDirectSound_SetPosition`(리스너)도 불린다.
* **Confirmed**: boot passes `DirectSoundCreate` (HLE) → `0x0019E523` (lifted) → `0x0019E4BC` (lifted) → `0x0019E4C1` (lifted); the host device opens at 48 kHz stereo, and with `REXBOX_AUDIO=none` the null output moves the cursors. The game then calls four threads (`0x0013B180`, `0x0013B1C0`, `0x0013B230`, `0x0013B2A0`; referenced only by `push offset`) and the file-read method `0x00178F40` (`.rdata` vtable `0x001E407C`), which the disassembler had missed and which were skipped at first; seeding the 162 pointer-only functions (`seed_pointer_targets.json`, from `scripts/recomp/find_pointer_targets.py`) took the lifted functions from 8,852 to 9,015 and the unresolved stubs from 109 to 105. The current stop: the game reads `D:\Media\Sounds\dsstdfx.bin` with `NtReadFile(h, NULL, ApcRoutine, …)` and stops waiting on event `0x1B0` in XAPI's `WaitForSingleObjectEx` (`0x00145B5F`, `NtWaitForSingleObjectEx(0x1B0, UserMode, bAlertable, …)`) (watchdog); the completion routine is expected to signal it (**inferred**: `bAlertable` = TRUE), and the kernel HLE delivers no user APCs ([boot-frontier](boot-frontier.md)). **Confirmed**: the three pointer-target candidates (`0x0001FD10`, `0x00020F10`, `0x0007BE30`) follow the previous function's switch jump and index tables, which the linear sweep decodes as code, so their seeds are rejected as mid-instruction; `0x0007BE30` was reached at run time (issue #19) and is now an `observed` seed. After user APCs (issue #19, **confirmed**): `DownloadEffectsImage` builds the descriptor with 20 effects and 794,624 (0xC2000) bytes of scratch; the first four buffers are `0x00141434`-style streaming buffers (flags `0x000400A0`, 65,536 bytes, 2-channel 44,100 Hz 16-bit PCM, no mix bins), each getting `Lock`, `Unlock`, `SetVolume` and `SetHeadroom` but no `Play` before the fault; after the fonts come ten sound-effect buffers (flags `0x00040000`, `dwBufferBytes` 0, apparently filled later through `SetBufferData`): Xbox ADPCM at 22,050 Hz (eight 2-channel with 72-byte blocks, one 1-channel with 36-byte blocks) and one 2-channel 44,100 Hz PCM, all without mix bins and all formats the HLE decodes; `IDirectSound_SetPosition` (the listener) is called too.
