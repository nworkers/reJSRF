# Issue #51 작업 로그 — 처음 쓰는 GL 프로그램의 컴파일 끊김 줄이기

작업 지시: [20261014-i051](../work-orders/20261014-i051-program-cache.md) · 설계: [20261014-i051](../design/20261014-i051-program-cache.md)

## 요약

GL 프로그램을 드라이버의 바이너리로 디스크에 저장하고, 다음 실행에서 컴파일 대신 쓰게 했다.

두 번째 실행부터 데모의 프로그램 57개가 약 2.4초 대신 약 0.13초(개당 약 2.3 ms)에 만들어진다. 데모 시작의 가장 긴 프레임은 약 2.0초에서 0.9~1.1초로 줄었다. 그 뒤 구간의 끊김(33.4 ms를 넘는 프레임)도 줄었다.

남은 약 1초 끊김은 셰이더가 아니다. 처음 나오는 텍스처의 디코드·업로드로 본다(**추정**).

## 과정

1. **측정 도구**: 프로파일에 구간별 프로그램 수와 시간을 더했다. 끊김은 평균에 묻히므로 구간의 가장 긴 프레임과 33.4 ms를 넘은 프레임 수도 더했다.
2. **기준**: 데모 동안 프로그램 57개를 컴파일했다.
   * 첫 300프레임에 34개(1.1~1.6초)
   * 그 뒤 1~8개씩, 개당 18~48 ms
3. **캐시**(`d3d8_gl_program_cache`):
   * 바이너리 진입점은 선택 로드한다. 진입점이 없거나 형식이 0개면 캐시는 꺼진다.
   * 항목에 드라이버 문자열과 두 소스 전문을 넣어 완전히 같을 때만 쓴다.
   * 드라이버가 거부하면 컴파일로 돌아가 항목을 다시 쓴다.
   * `glDeleteProgram`을 새로 로드했다(거부된 프로그램을 지우는 데 필요).
4. **두 번 실행**:
   * 첫 실행은 57개를 컴파일해 항목 57개(약 780 KB)를 썼다.
   * 두 번째 실행은 57개 모두 캐시에서 만들었다. 첫 구간 34개는 82 ms였다(첫 실행 1,599 ms).
5. **끊김 비교**(캐시 끔/켬 번갈아 2회씩, 가장 긴 프레임):
   * 데모 첫 구간: 2,001/1,984 ms → 868/1,150 ms
   * 둘째 구간: 168/150 → 102/116 ms. 33.4 ms를 넘는 프레임은 10/7 → 4/3개
   * 셋째 구간: 118/117 → 100/101 ms
   * 두 모드 모두에 주기적인 83~100 ms 프레임이 남아 있다. 셰이더와는 다른 원인이다.
6. **첫 구간의 남은 시간**: 캐시를 켜도 첫 구간 setup이 프레임당 약 8.5 ms였다. 프로그램 몫은 82 ms/300프레임으로 작았다. 샘플에 보이던 DXT 디코드(`DecodeDxtBlock`)와 맞으므로 처음 쓰는 텍스처의 업로드로 본다(**추정**).
7. **망가진 항목**:
   * 한 항목은 바이너리 꼬리 64바이트를 바꾸고, 다른 항목은 3바이트로 잘랐다.
   * 실행하니 두 프로그램만 컴파일됐고(`1 compiled` 두 번), 두 항목은 정상 내용으로 다시 쓰였다.
8. 프로파일 코드의 지역 변수 이름이 함수 인자를 가리는 경고(C4457)를 고쳤다.

## 검증

| 검증 | 결과 |
|---|---|
| 두 번째 실행 | 57/57 캐시에서, 약 2.4초 → 약 0.13초 |
| 끊김(가장 긴 프레임, 데모 첫 구간) | 약 2.0초 → 0.9~1.1초 |
| 망가진 항목 2개 | 다시 컴파일, 다시 씀 |
| Win32 Debug / Release 빌드 | 성공, 경고 0 |
| 회귀 도구(Debug, Release) | kernel 56/56, d3d8 116/116, dsound 48/48, d3d8 check 14,018/0, decomp check 6/0 |

## 남은 일

* 처음 쓰는 텍스처의 디코드·업로드(DXT를 압축된 채로 올리기)로 데모 시작의 남은 끊김을 줄인다.
* 주기적인 83~100 ms 프레임의 원인을 찾는다.
* 첫 실행에서도 끊김을 없애려면 미리 빌드해야 한다. 지금은 하지 않는다.

---

# Issue #51 work log — less stutter from compiling GL programs on first use

Work order: [20261014-i051](../work-orders/20261014-i051-program-cache.md) · Design: [20261014-i051](../design/20261014-i051-program-cache.md)

## Summary

GL programs are now stored on disk as the driver's binaries and used instead of compiling on the next run.

From the second run on, the demo's 57 programs take about 0.13 s instead of about 2.4 s (about 2.3 ms each). The longest frame at the start of the demo fell from about 2.0 s to 0.9–1.1 s, and later windows have fewer frames over 33.4 ms.

The remaining stall of about a second is not shaders; it is taken to be the decoding and upload of textures used for the first time (**inferred**).

## Process

1. **Tools**: the profile now reports programs and their time per window. Because stutter hides in averages, it also reports each window's longest frame and the frames over 33.4 ms.
2. **Baseline**: 57 programs were compiled during the demo.
   * 34 in the first 300 frames (1.1–1.6 s)
   * then 1–8 at a time, 18–48 ms each
3. **Cache** (`d3d8_gl_program_cache`):
   * The binary entry points are loaded as optional; without them, or with no formats, the cache is off.
   * Entries hold the driver identity and both full sources and are used only on an exact match.
   * A rejected binary falls back to compiling and rewrites the entry.
   * `glDeleteProgram` is now loaded, to delete a rejected program.
4. **Two runs**:
   * The first compiled 57 programs and wrote 57 entries (about 780 KB).
   * The second made all 57 from the cache; the first window's 34 took 82 ms (1,599 ms in the first run).
5. **Stutter** (cache off and on alternately, twice each; longest frame):
   * first demo window: 2,001/1,984 ms → 868/1,150 ms
   * second window: 168/150 → 102/116 ms, with frames over 33.4 ms going from 10/7 to 4/3
   * third window: 118/117 → 100/101 ms
   * Periodic frames of 83–100 ms remain in both modes; they have another cause.
6. **What remains in the first window**: with the cache, setup in the first window was still about 8.5 ms per frame, while programs took only 82 ms over 300 frames. That matches the DXT decoding (`DecodeDxtBlock`) seen in samples, so it is taken to be textures uploaded on first use (**inferred**).
7. **Damaged entries**:
   * One entry had its last 64 bytes of binary changed; another was cut to 3 bytes.
   * A run compiled just those two programs (`1 compiled` twice) and rewrote both entries with valid contents.
8. A profile local variable that shadowed a function parameter (C4457) was renamed.

## Verification

The table in the Korean section applies:

* second run: 57/57 from the cache, about 2.4 s → about 0.13 s
* longest frame in the first demo window: about 2.0 s → 0.9–1.1 s
* two damaged entries: recompiled and rewritten
* Debug and Release builds with zero warnings
* regression tools in both configurations: kernel 56/56, D3D8 116/116, DSOUND 48/48, d3d8 check 14,018/0, decomp check 6/0

## Remaining

* Decoding and uploading textures on first use (uploading DXT still compressed) would cut the remaining stall at the start of the demo.
* Find the cause of the periodic 83–100 ms frames.
* Removing the stutter on the first run as well would need building programs ahead, which is not done.
