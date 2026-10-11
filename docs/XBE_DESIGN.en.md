# JSRF XBE Structure and Design (English)

Accumulates the structure confirmed by analysing the original XBE. Detailed evidence and confirmation status live in [`docs/analysis/`](analysis/README.md); only conclusions that affect the design are collected here. Korean version: [XBE_DESIGN.ko.md](XBE_DESIGN.ko.md).

## 1. Target build

* The primary target is the US build `default_eng.xbe` (region 1, MD5 `f4e366bf…`). The Japan-region `default.xbe` has different code and shares no addresses with it. → [jsrf-xbe-overview §1](analysis/jsrf-xbe-overview.md)
* Both builds: XDK 4134, title id `5345000A`, entry point `0x00148023`.

## 2. Memory layout (US)

The diagram in the Korean version applies: headers at `0x00010000`, `.text` at `0x00011000`, `D3D` at `0x0018CB40`, `DSOUND` at `0x0019E340`, `MMATRIX`/`XGRPH`/`XPP` from `0x001BA8A0`, `.rdata` at `0x001C3F60` (kernel thunks), `.data` at `0x001EB760`, `DOLBY` at `0x0027E080`, then `$$XTIMAGE` and `$$XSIMAGE`.

* The image ends at `0x00288620` (base + `0x00278620`), well inside 64 MiB of guest memory.
* The kernel thunk table is at the first address of `.rdata`, `0x001C3F60`, with 120 slots.

## 3. Conclusions for the HLE design

* XDK library code is separated into named sections (`D3D`, `DSOUND`, `XGRPH`, `XPP`), so function identification can be narrowed per section.
* HLE modules match the XDK 4134 API (render-state numbers and so on).
* Among the kernel imports, the interrupt, device and GPU-memory entries may need only minimal implementations once D3D8/DSOUND are HLE'd at the API level (**inferred**).
