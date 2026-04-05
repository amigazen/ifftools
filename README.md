# ifftools

This is ifftools for Amiga. It includes:
- iff2png for Amiga. Convert IFF Picture format images (including ILBM, DEEP, RGB8, RGBN, FAXX, PBM, ACBM) to PNG format
- IFFPicture.lib a static link library modelled on the iffparse.library API extending iffparse's ability to include reading and writing all known IFF bitmap image formats
- iff2aiff for Amiga. Convert IFF Sound format samples (including 8SVX, MAUD to the industry standard AIFF (Audio IFF) format)
- IFFSound.lib a static link library modelled on the iffparse.library API extending iffparse's ability to include reading and writing all known IFF sound sample formats

It also includes by necessity the libpng and zlib dependencies needed by iff2png. These are not modified at all except by the inclusion of build configurations for SAS/C on Amiga:
- libpng 1.6 for Amiga - the standard implementation of PNG, buildable with SAS/C as a static link library

## [amigazen project](http://www.amigazen.com)

*A web, suddenly*

*Forty years meditation*

*Minds awaken, free*

**amigazen project** uses modern software development tools and methods to update and rerelease classic Amiga open source software. Upcoming releases include a new AWeb, and a new Amiga Python 2.

Key to the amigazen project approach is ensuring every project can be built with the same common set of development tools and configurations, so we created the ToolKit project to provide a standard configuration for Amiga development. All *amigazen project* releases will be guaranteed to build against the ToolKit standard so that anyone can download and begin contributing straightaway without having to tailor the toolchain for their own setup.

This software is Free and Open Source Software distributed on terms described in the documentation, particularly the file LICENSE.md

amigazen project philosophy is based on openness:

*Open* to anyone and everyone	- *Open* source and free for all	- *Open* your mind and create!

PRs for all of amigazen project releases are gratefully received at [GitHub](https://github.com/amigazen/). While the focus now is on classic 68k software, it is intended that all amigazen project releases can be ported to other Amiga-like systems including AROS and MorphOS where feasible.

## About ToolKit

**ToolKit** exists to solve the problem that most Amiga software was written in the 1980s and 90s, by individuals working alone, each with their own preferred setup for where their dev tools are run from, where their include files, static libs and other toolchain artifacts could be found, which versions they used and which custom modifications they made. Open source collaboration did not exist then as we know it now. 

**ToolKit** from amigazen project is a work in progress to make a standardised installation of not just the Native Developer Kit, but the compilers, build tools and third party components needed to be able to consistently build projects in collaboration with others, without each contributor having to change build files to work with their particular toolchain configuration. 

All *amigazen project* releases will release in a ready to build configuration according to the ToolKit standard.

Each component of **ToolKit** is open source and like *make* here will have it's own github repo, while ToolKit itself will eventually be released as an easy to install package containing the redistributable components, as well as scripts to easily install the parts that are not freely redistributable from archive.

## Requirements

- Amiga or Amiga-compatible computer with latest operating system software

## iff2png Usage

iff2png converts IFF (Interchange File Format) Picture bitmap images to PNG format. It supports all major IFF Picture bitmap image formats including ILBM, PBM, RGBN, RGB8, DEEP, ACBM, YUVN, and FAXX. It will automatically choose the optimal profile for the PNG file based on the source file's properties.

### Basic Syntax

```
iff2png SOURCE TARGET [OPTIONS]
```

### Arguments

- **SOURCE** - Input IFF image file (required)
- **TARGET** - Output PNG file (required)

### Options

- **FORCE** - Overwrite existing output file without prompting
- **QUIET** - Suppress normal output messages (errors will still be displayed)
- **OPAQUE** - Keep color 0 opaque instead of transparent. By default, iff2png honors the ILBM specification where palette index 0 can be transparent. Use this option to preserve legacy behavior where black (color 0) is always visible.
- **STRIP** or **NOMETADATA** - Prevents any metadata text (copyright, author, annotations) from the source IFF file being included in the target PNG

### Examples

Convert an IFF image to PNG:
```
iff2png source.iff target.png
```

Convert with forced overwrite and quiet mode:
```
iff2png source.iff target.png FORCE QUIET
```

Convert preserving black as opaque:
```
iff2png source.iff target.png OPAQUE
```

Convert without copying metadata:
```
iff2png source.iff target.png STRIP
```

### Supported IFF Picture Formats

- **ILBM** (InterLeaved BitMap) - Standard Amiga bitmap format with interleaved bitplanes. Supports HAM (Hold And Modify), EHB (Extra Half-Brite), and various bitplane counts. Also supports 24-bit ILBM (deep ILBM with 24 bitplanes for true-color RGB, where bitplanes 0-7 represent Red, 8-15 represent Green, and 16-23 represent Blue), and ILBM compression type 2 - column-wise ByteRun1
- **PBM** (Packed BitMap) - Similar to ILBM but with packed pixels (one byte per pixel) instead of bitplanes (N.B. this is not the same as NetPBM)
- **RGBN** - RGB format with N planes (true-color with separate RGB channels)
- **RGB8** - RGB 8-bit format (24-bit color, 32-bit with alpha)
- **DEEP** - High bit-depth format for professional graphics
- **ACBM** (Amiga Continuous BitMap) - Format with separate alpha channel data in an ABIT chunk.
- **YUVN** - YUV color space format for broadcast television (CCIR-601-2 standard). Stores luminance (Y) and color-difference signals (U, V) in separate chunks. Supports various YUV modes (400, 411, 422, 444, and lores variants)
- **FAXX** - Facsimile image format using ITU-T T.4 compression (Modified Huffman, Modified READ, Modified Modified READ)
- **ILBM Framestore** - NewTek VideoToaster's Framestore proprietary extensions to ILBM
- **Multipalette** - Support for SHAM, CTBL and PCHG multipalette IFF Picture files

### Output Information

When not in quiet mode, iff2png displays detailed information about the conversion:

- IFF source format, file size, dimensions, bit planes, compression method, and masking
- Optional IFF chunk catalog and multipalette summary (non-quiet)
- PNG target color type, bit depth, palette entries, transparency settings
- Conversion summary with file sizes and compression ratio

## iff2aiff Usage

iff2aiff converts IFF **audio** (FORM 8SVX, 16SV, AIFF, AIFC, MAUD, …) to Apple **AIFF** or **AIFC** with uncompressed PCM. Sample rate, channel count, and length follow the source; output is 8-bit when the decoded source is 8-bit, otherwise 16-bit. 

### Basic Syntax

```
iff2aiff SOURCE TARGET [OPTIONS]
```

### Arguments

- **SOURCE** — Input IFF sound file (required)
- **TARGET** — Output `.aiff` / `.aifc` file (required)

### Options

- **FORCE** — Overwrite an existing target file without prompting
- **QUIET** — Suppress normal progress text (errors still print)
- **AIFC** or **COMPRESS** — Write **AIFC** (`FORM AIFC`) instead of classic **AIFF** (`FORM AIFF`). Both keywords are equivalent (ReadArgs template `COMPRESS=AIFC/S`).

### Examples

```
iff2aiff sample.8svx ram:sample.aiff
iff2aiff music.maud ram:music.aiff FORCE
iff2aiff clip.aiff ram:clip.aifc AIFC
iff2aiff voice.iff ram:out.aiff QUIET
```

### Supported IFF sound inputs

Typical forms handled via **ParseIFFSound** / **IFFSound.lib** include **8SVX** (8-bit sampled voice), **16SV**, **AIFF** / **AIFC** (re-wrapped to your chosen output container), and **MAUD** (Amiga musical data). For chunk layouts and semantics, see **SDK/Help/IFF Sound Formats.guide** and **SDK/Help/iff2aiff.guide**.

### Output (non-quiet)

When **QUIET** is not set, iff2aiff prints the command line, source FORM name, source rate/channels/bit depth, target AIFF vs AIFC profile, frame count, PCM byte size, and output file size.

## Contact 

- At GitHub https://github.com/amigazen/ifftools/
- on the web at http://www.amigazen.com/toolkit/ (Amiga browser compatible)
- or email toolkit@amigazen.com


## Acknowledgements

*Amiga* is a trademark of **Amiga Inc**.

### zlib

iff2png uses **zlib**, a compression library written by Jean-loup Gailly and Mark Adler.

Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

For more information, visit https://zlib.net/

### libpng

iff2png uses **libpng**, the PNG Reference Library version 1.6.43.

Copyright (c) 1995-2024 The PNG Reference Library Authors.
Copyright (c) 2018-2024 Cosmin Truta.
Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson.
Copyright (c) 1996-1997 Andreas Dilger.
Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.

The software is supplied "as is", without warranty of any kind, express or implied, including, without limitation, the warranties of merchantability, fitness for a particular purpose, title, and non-infringement. In no event shall the Copyright owners, or anyone distributing the software, be liable for any damages or other liability, whether in contract, tort or otherwise, arising from, out of, or in connection with the software, or the use or other dealings in the software, even if advised of the possibility of such damage.

Permission is hereby granted to use, copy, modify, and distribute this software, or portions hereof, for any purpose, without fee, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated, but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This Copyright notice may not be removed or altered from any source or altered source distribution.

For more information, visit http://www.libpng.org/pub/png/libpng.html