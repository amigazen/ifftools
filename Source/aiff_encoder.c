/*
** aiff_encoder.c - AIFF Encoder Implementation
**
** Writes uncompressed AIFF (FORM AIFF) with COMM and SSND chunks.
** Uses optimal profile: same sample rate, channels, and bit depth as source.
*/

#include "aiff_encoder.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <math.h>

/* IFF chunk IDs */
#define ID_FORM  0x464F524DUL  /* 'FORM' */
#define ID_AIFF  0x41494646UL  /* 'AIFF' */
#define ID_AIFC  0x41494643UL  /* 'AIFC' */
#define ID_COMM  0x434F4D4DUL  /* 'COMM' */
#define ID_SSND  0x53534E44UL  /* 'SSND' */
#define ID_NONE  0x4E4F4E45UL  /* 'NONE' - uncompressed (AIFC) */

/* Write a 32-bit big-endian word to buffer */
static VOID PutLong(UBYTE *p, ULONG val)
{
    p[0] = (UBYTE)(val >> 24);
    p[1] = (UBYTE)(val >> 16);
    p[2] = (UBYTE)(val >> 8);
    p[3] = (UBYTE)(val);
}

/* Write a 16-bit big-endian word to buffer */
static VOID PutWord(UBYTE *p, UWORD val)
{
    p[0] = (UBYTE)(val >> 8);
    p[1] = (UBYTE)(val);
}

/*
** IEEE754WriteExtended - Write double as 80-bit IEEE 754 extended (big-endian)
** Used for AIFF COMM chunk sample rate.
*/
static VOID IEEE754WriteExtended(double value, UBYTE *bytes)
{
    int exp;
    double frac;
    ULONG mantissaHigh;
    ULONG mantissaLow;
    int exponent15;

    if (!bytes) {
        return;
    }

    if (value == 0.0 || value != value) {
        bytes[0] = 0;
        bytes[1] = 0;
        PutLong(bytes + 2, 0);
        PutLong(bytes + 6, 0);
        return;
    }

    if (value < 0) {
        bytes[0] = 0x80;
        value = -value;
    } else {
        bytes[0] = 0;
    }

    frac = frexp(value, &exp);
    /* frac in [0.5, 1.0), value = frac * 2^exp
     * 80-bit: value = 2^(e-16383) * 1.m => 1.m = frac * 2^(exp - (e-16383))
     * Set e = exp + 16383, then 1.m = frac * 2
     */
    exponent15 = exp + 16383;
    bytes[0] |= (UBYTE)((exponent15 >> 8) & 0x7F);
    bytes[1] = (UBYTE)(exponent15 & 0xFF);

    /* Mantissa: 1.m = 2*frac, so m = 2*frac - 1 in [0, 1). Store 64-bit m * 2^64. */
    frac = 2.0 * frac - 1.0;
    if (frac >= 1.0) {
        frac = 0.0;
        exponent15++;
        bytes[1] = (UBYTE)(exponent15 & 0xFF);
    }
    /* Split frac * 2^64 into high and low 32 bits */
    frac *= 4294967296.0;
    mantissaHigh = (ULONG)frac;
    frac -= (double)mantissaHigh;
    frac *= 4294967296.0;
    mantissaLow = (ULONG)frac;

    PutLong(bytes + 2, mantissaHigh);
    PutLong(bytes + 6, mantissaLow);
}

/*
** AIFFEncoder_Write - Write PCM data to an AIFF or AIFC file
*/
LONG AIFFEncoder_Write(const char *filename,
                      UBYTE *pcmData,
                      ULONG dataSize,
                      struct AIFFEncoderProfile *profile,
                      BOOL useAIFC)
{
    BPTR fh;
    ULONG formSize;
    ULONG commSize;
    ULONG ssndSize;
    ULONG ssndDataSize;
    UBYTE commChunk[32];  /* 18 base + 4 type + up to 10 for Pascal name */
    UBYTE ssndHeader[16];
    ULONG offset;
    ULONG blockSize;
    ULONG toWrite;
    ULONG i;
    ULONG numFrames;
    ULONG channels;
    ULONG bitDepth;

    if (!filename || !pcmData || !profile) {
        return RETURN_FAIL;
    }

    numFrames = profile->numFrames;
    channels = profile->channels;
    bitDepth = profile->bitDepth;

    if (channels == 0 || (bitDepth != 8 && bitDepth != 16) ||
        numFrames == 0 || profile->sampleRate == 0) {
        return RETURN_FAIL;
    }

    if (dataSize != numFrames * channels * (bitDepth / 8)) {
        return RETURN_FAIL;
    }

    fh = Open((STRPTR)filename, MODE_NEWFILE);
    if (!fh) {
        return RETURN_FAIL;
    }

    /* COMM chunk: 18 bytes base; for AIFC add 4-byte compression ID + Pascal string "None" (1+4 bytes) */
    commSize = 18;
    PutWord(commChunk + 0, (UWORD)channels);
    PutLong(commChunk + 2, numFrames);
    PutWord(commChunk + 6, (UWORD)bitDepth);
    IEEE754WriteExtended((double)profile->sampleRate, commChunk + 8);
    if (useAIFC) {
        PutLong(commChunk + 18, ID_NONE);
        commChunk[22] = 4;  /* Pascal string length */
        commChunk[23] = 'N';
        commChunk[24] = 'o';
        commChunk[25] = 'n';
        commChunk[26] = 'e';
        commSize = 27;
    }

    /* SSND: 8 bytes header (offset, blockSize) + sound data */
    offset = 0;
    blockSize = 0;
    ssndDataSize = dataSize;
    ssndSize = 8 + ssndDataSize;

    /* FORM size: 4 (form type) + COMM_chunk(8+commSize) + SSND_chunk(8+8+dataSize) */
    formSize = 4 + (8 + commSize) + (8 + ssndSize);

    /* Write FORM header */
    PutLong(ssndHeader + 0, ID_FORM);
    PutLong(ssndHeader + 4, formSize);
    toWrite = Write(fh, ssndHeader, 8);
    if (toWrite != 8) {
        Close(fh);
        return RETURN_FAIL;
    }

    /* Write form type: AIFF or AIFC */
    PutLong(ssndHeader + 0, useAIFC ? ID_AIFC : ID_AIFF);
    toWrite = Write(fh, ssndHeader, 4);
    if (toWrite != 4) {
        Close(fh);
        return RETURN_FAIL;
    }

    /* Write COMM chunk */
    PutLong(ssndHeader + 0, ID_COMM);
    PutLong(ssndHeader + 4, commSize);
    toWrite = Write(fh, ssndHeader, 8);
    if (toWrite != 8) {
        Close(fh);
        return RETURN_FAIL;
    }
    toWrite = Write(fh, commChunk, commSize);
    if (toWrite != (LONG)commSize) {
        Close(fh);
        return RETURN_FAIL;
    }

    /* Write SSND chunk header */
    PutLong(ssndHeader + 0, ID_SSND);
    PutLong(ssndHeader + 4, ssndSize);
    PutLong(ssndHeader + 8, offset);
    PutLong(ssndHeader + 12, blockSize);
    toWrite = Write(fh, ssndHeader, 16);
    if (toWrite != 16) {
        Close(fh);
        return RETURN_FAIL;
    }

    /* Write sound data. For 8-bit AIFF we need unsigned 0..255; input is signed -128..127. */
    if (bitDepth == 8) {
        UBYTE *outBuf;
        outBuf = (UBYTE *)AllocMem(dataSize, MEMF_PUBLIC | MEMF_CLEAR);
        if (!outBuf) {
            Close(fh);
            return RETURN_FAIL;
        }
        for (i = 0; i < dataSize; i++) {
            outBuf[i] = (UBYTE)((LONG)((char)pcmData[i]) + 128);
        }
        toWrite = Write(fh, outBuf, dataSize);
        FreeMem(outBuf, dataSize);
        if (toWrite != (LONG)dataSize) {
            Close(fh);
            return RETURN_FAIL;
        }
    } else {
        /* 16-bit: already big-endian signed in buffer (UWORD array when from DecodeTo16Bit) */
        toWrite = Write(fh, pcmData, dataSize);
        if (toWrite != (LONG)dataSize) {
            Close(fh);
            return RETURN_FAIL;
        }
    }

    Close(fh);
    return RETURN_OK;
}
