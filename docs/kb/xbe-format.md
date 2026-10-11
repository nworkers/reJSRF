# XBE 실행 형식

XBE(Xbox Executable)는 원본 Xbox의 실행 형식이다. PE에서 파생되었지만 헤더가 다르고, 커널이 한 번에 메모리에 올린다. 아래는 `rexbox::xbe::XbeImage`가 읽는 부분이다.

출처: [xboxdevwiki: Xbe](https://xboxdevwiki.net/Xbe), [xboxdevwiki: Kernel](https://xboxdevwiki.net/Kernel), xboxrecomp `docs/formats/xbe.md`(MIT).

## 이미지 헤더

| 오프셋 | 필드 | 비고 |
|---|---|---|
| `0x000` | magic `XBEH` | |
| `0x004` | RSA 서명 256바이트 | 로더 검증용, 우리는 쓰지 않는다 |
| `0x104` | base address | 소매판 타이틀은 `0x00010000` |
| `0x108` | size of headers | 헤더는 base에 그대로 매핑된다 |
| `0x10C` | size of image | |
| `0x114` | time/date | |
| `0x118` | certificate 주소 | |
| `0x11C` / `0x120` | 섹션 수 / 섹션 헤더 주소 | |
| `0x124` | init flags | |
| `0x128` | entry point (XOR 인코딩) | 소매 `0xA8FC57AB`, 디버그 `0x94859D4B` |
| `0x12C` | TLS 디렉터리 주소 | |
| `0x130`–`0x138` | stack commit, heap reserve, heap commit | |
| `0x158` | kernel thunk 표 주소 (XOR 인코딩) | 소매 `0x5B6D40B6`, 디버그 `0xEFB1F152` |
| `0x160` / `0x164` | 라이브러리 버전 수 / 주소 | |

주소 필드는 파일 오프셋이 아니라 가상 주소다. 헤더 범위의 주소는 `주소 - base`로, 섹션 안의 주소는 그 섹션의 raw 데이터로 바꾼다.

소매/디버그 키는 entry point를 두 키로 풀어 이미지 범위에 들어가는 쪽으로 고른다.

## 섹션 헤더 (56바이트)

flags(1 쓰기, 2 preload, 4 실행, 8 inserted file, 0x10/0x20 head/tail page 읽기 전용), virtual address, virtual size, raw address, raw size, 이름 주소, 이름 참조 수, head/tail 공유 페이지 참조 수 주소, SHA-1 digest 20바이트. virtual size가 raw size보다 크면 나머지는 0으로 채운다.

## 인증서

`+0x08` title id, `+0x0C` 타이틀 이름(UTF-16 40자), `+0x9C` 허용 미디어, `+0xA0` 지역(1 북미, 2 일본, 4 그 외), `+0xA4` 등급, `+0xA8` 디스크 번호, `+0xAC` 버전.

## 라이브러리 버전 (16바이트)

이름 8바이트, major, minor, build, flags(각 16비트). 타이틀이 정적 링크한 XDK 라이브러리와 그 빌드 번호를 알려 준다. HLE가 맞춰야 할 API 버전을 정하는 근거다.

## 커널 thunk 표

32비트 값의 배열이며 0으로 끝난다. 각 값은 `0x80000000 | ordinal`이다. Xbox 커널은 이름 없이 ordinal로만 export하며, 로더가 각 슬롯을 커널 함수 주소로 덮어쓴다. 타이틀 코드는 `call [slot]`로 부른다. 이름 표는 `rexbox/hle/kernel/kernel_exports.h`에 있다(xboxrecomp에서 가져옴).

---

# The XBE executable format

XBE (Xbox Executable) is the original Xbox's executable format. It derives from PE but has its own headers, and the kernel loads it into memory in one pass. Below is what `rexbox::xbe::XbeImage` reads.

Sources: [xboxdevwiki: Xbe](https://xboxdevwiki.net/Xbe), [xboxdevwiki: Kernel](https://xboxdevwiki.net/Kernel), xboxrecomp `docs/formats/xbe.md` (MIT).

## Image header

See the table above. Address fields are virtual addresses, not file offsets: an address inside the headers maps as `address - base`, one inside a section through that section's raw data. The retail/debug key set is chosen by decoding the entry point with both keys and keeping the one that lands inside the image.

## Section header (56 bytes)

flags (1 writable, 2 preload, 4 executable, 8 inserted file, 0x10/0x20 head/tail page read-only), virtual address, virtual size, raw address, raw size, name address, name reference count, head/tail shared page reference count addresses, a 20-byte SHA-1 digest. When the virtual size exceeds the raw size the rest is zero-filled.

## Certificate

`+0x08` title id, `+0x0C` title name (40 UTF-16 characters), `+0x9C` allowed media, `+0xA0` region (1 North America, 2 Japan, 4 rest of world), `+0xA4` ratings, `+0xA8` disk number, `+0xAC` version.

## Library versions (16 bytes)

An 8-byte name, then major, minor, build and flags (16 bits each). They tell which XDK libraries the title linked statically and at which build, which fixes the API version the HLE has to match.

## Kernel thunk table

An array of 32-bit values ending in zero, each `0x80000000 | ordinal`. The Xbox kernel exports by ordinal only, without names; the loader overwrites each slot with the kernel function's address, and title code calls `call [slot]`. The name table is in `rexbox/hle/kernel/kernel_exports.h` (taken from xboxrecomp).
