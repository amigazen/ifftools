# iff2png / IFFPicture — future work

---

## Multi-image IFF (`CAT` / `LIST`)

- **Current behaviour:** `ParseIFFPicture` walks the stream until the **first** supported picture `FORM`, then loads that single image. `iff2png` writes **one PNG** per run.
- **Decide product behaviour** for containers with multiple `FORM ILBM` (and similar):
  - **Numbered PNGs** (e.g. `base-000.png`, `base-001.png`, …) with a naming pattern flag.
  - **Optional directory** per input when count > 1.
  - **Animation formats** (MNG, APNG) only if we treat the sequence as temporal; many `CAT ILBM` files are brush banks, not animations—avoid surprising defaults.
- **Library API:** iterator or `ParseNextPicture` / multi-pass stream support so the CLI (or host apps) can load each `FORM` without re-reading from scratch where possible.
- **`LIST` + `PROP`:** merge shared `PROP` chunks into each nested `FORM` per EA IFF rules (iffparse scopes); today only the first picture `FORM` is used and `PROP` inheritance is not explicitly implemented.

---

## ILBM / IFF spec items still shallow or missing

- **`CLUT`:** chunk is stored (raw); **no** deep-ILBM colour path uses it yet—need third-party / EA clarification and decode wiring if required.
- **`DYCP`:** stored raw only; **dynamic HAM** / hardware-style palette over time **not** applied to raster output.
- **Digi-View + `DGVW` without `nPlanes == 21`:** `isDigiViewRgb` may be set from `DGVW` alone; **decode** still uses the 21-plane path only when `nPlanes == 21`. Reconcile or document.
- **24-bit ILBM + `transparentColor` / lasso in BMHD:** decode path is mask-oriented for deep RGB; verify spec expectations for rare files.
- **Aspect ratio / positioning:** `xAspect`, `yAspect`, `x`, `y`, `pageWidth`, `pageHeight` are not used to rescale or pad output PNGs.
- **Ham / EHB edge cases:** verify against reference images (e.g. `island.iff` / PCHG interaction mentioned in project notes).
- **Composite IFF beyond first `FORM`:** full `CAT`/`LIST` enumeration (see above).

---

## IFFPicture **encoder** (write path)

The library is largely **read/decode** today. Future **writer** surface (names indicative):

- Push `FORM ILBM` / `BMHD` / `CMAP` / `CAMG` / `BODY` (and optional chunks) from decoded buffers or RGB + metadata.
- ByteRun1 row packing (standard and column-wise where applicable).
- Optional chunks: `GRAB`, `DEST`, `SPRT`, `CRNG`, `CCRT`, multipalette (`PCHG`/`SHAM`/`CTBL`)—incremental milestones.
- Round-trip tests: decode → encode → decode (pixel and metadata checks).

Mirror public API style of read side (`iffparse`-like `PushChunk` / `WriteChunk` helpers or dedicated `WriteILBM`-style entry points—design TBD).

---

## Build warnings (`Source/output.txt`)

Fix or suppress intentionally; line numbers drift—re-run build and adjust.

### `iffpicture_private.h`

- **W84:** `ID_CAT`, `ID_LIST` **redefine** symbols already defined in `libraries/iffparse.h`. **Fix:** `#ifndef ID_CAT` / `#ifndef ID_LIST` guards, or remove local defs and use iffparse’s.

### `image_decoder.c`

- **W93:** no reference to `bitOffset`, `shift` (~3009–3011).
- **W315:** static `bit_mask` unreachable (~4724).
- **W304:** dead assignments: `curpos` (~611); `alphaValues`, `cmapData`, `paletteOut`, `destM` (~1516–1519); `hamPal` (~2263); `paletteOut` (~2617, ~3289); `displayWidth`, `displayHeight` (~3027–3028); `redBits`, `greenBits`, `blueBits`, `alphaBits` (~3049–3071); `isLores` (~4370); `alphaBuf`, `alphaSize`, `cn` (~4561–4563). Refactor initialisation / branches or remove unused variables.

### `pchg_palette.c`

- **W85:** `IFFMultipalette_Active` return type **BOOL** vs `int` expression—return `(BOOL)(…)` or explicit compare.
- **W100 / W225:** `FindProp` no prototype / pointer mismatch—include correct iffparse header or forward-declare; ensure `struct StoredProperty *` typing.
- **W304:** dead assignment `data` (~361).

### `bitmap_renderer.c`

- **W100:** `FreeDisplayInfoData` — add prototype (e.g. `graphics/displayinfo.h` or appropriate Amiga SDK include).

### `utils.c`

- **W105 / W316:** module exports no symbols; `DummyUtilsFunction` unreachable—replace with real helpers or drop file from link if placeholder only.

### `main.c`

- **W315:** unreachable statics `verstag`, `stack_cookie` (~1065)—AmigaOS version/cookie pattern; confirm linker section / conditional compilation.
- **W304:** dead assignment `len` (~787, ~1040, ~1043).

### `png_encoder.c`

- **W304:** dead assignment `filehandle` (~109).

### `iff2aiff.c` (iffsound tool)

- **W100:** no prototype for `Strncpy`, `SNPrintf`—include or declare.
- **W315:** unreachable `verstag`, `stack_cookie` (~318).
- **W304:** dead `sourceFileSize` (~100, ~109).

---

## iff2png CLI / UX

- **Verbose / catalog:** mention `CCRT`, `CLUT`, `DGVW`, `DYCP` when present (parity with library getters).
- **Multi-output mode** once library supports enumerating `FORM`s (see above).
- **Documentation:** `README.md`, `SDK/Help/iff2png.guide`, `iffpicture.doc` — sync when encoder or CAT export lands.

---

## Testing / QA

- Regression corpus: standard ILBM, HAM, EHB, 24/32-bit, grey8, Digi-View 21-plane, mask, transparent index, lasso, `CAT`-wrapped single image, multipalette files.


