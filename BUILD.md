## Building from Source

### Requirements
- SAS/C compiler 6.58
- NDK 3.2 R4
- An Amiga computer

### Build Options

The SCOPTIONS file contains build options for the main iff2png executable and iffpicture.lib library

The default CPU target is 68020. 

Adding
```
define DEBUG
```

to SCOPTIONS will make a build with more console output to help debug transcoding problems

zlib and libpng can be found in their respective directories, with their own smakefile and SCOPTIONS.

If changing the math target from the current IEEE setting, make sure to change all 3 components and rebuild.

### Building iff2png

You must build in this order-> zlib -> libpng -> iff2png

```
cd Source/zlib
smake

cd /libpng
smake

cd /
smake iff2png
smake install
```

### Building iff2aiff

You must build in this order-> iffsound.lib -> iff2aiff

```
cd Source/iffsoundlib
smake

cd /
smake iff2aiff
smake install

## Installation

1. Find the iff2png and iff2aiff executables in SDK/C/ in this distribution and copy it to wherever you want to usually run it from
