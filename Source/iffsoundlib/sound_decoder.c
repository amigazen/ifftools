/*
** sound_decoder.c - Sound Decoder Implementation (Internal to Library)
**
** Decodes IFF audio formats to PCM sample data
** Supports 8SVX, AIFF, MAUD formats with various compression methods
*/

#include "iffsound_private.h"
#include "/debug.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/utility.h>

/* Forward declarations */
static LONG DecodeFibonacciDelta(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize);
static LONG DecodeADPCM2(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize, LONG *jc);
static LONG DecodeADPCM3(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize, LONG *jc);

/*
** Decode - Decode IFF audio data into internal format
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Must be called after ParseIFFSound()
*/
LONG Decode(struct IFFSound *sound)
{
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Sound not opened");
        }
        return RETURN_FAIL;
    }
    
    if (!sound->isLoaded) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "Sound not loaded - call ParseIFFSound() first");
        return RETURN_FAIL;
    }
    
    /* Decode based on format type */
    if (sound->formtype == ID_8SVX) {
        return Decode8SVX(sound);
    } else if (sound->formtype == ID_16SV) {
        return Decode16SV(sound);
    } else if (sound->formtype == ID_AIFF || sound->formtype == ID_AIFC) {
        return DecodeAIFF(sound);
    } else if (sound->formtype == ID_MAUD) {
        return DecodeMAUD(sound);
    } else {
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported format type");
        return RETURN_FAIL;
    }
}

/*
** DecodeToPCM - Decode IFF audio and convert to PCM format
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Allocates PCM buffer - caller is responsible for freeing if separate from sound->sampleData
*/
LONG DecodeToPCM(struct IFFSound *sound, UBYTE **pcmData, ULONG *size)
{
    LONG error;
    
    if (!sound || !pcmData || !size) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid parameters");
        }
        return RETURN_FAIL;
    }
    
    /* Decode if not already decoded */
    if (!sound->isDecoded) {
        error = Decode(sound);
        if (error != RETURN_OK) {
            return RETURN_FAIL;
        }
    }
    
    /* Return pointer to decoded data */
    *pcmData = sound->sampleData;
    *size = sound->sampleDataSize;
    
    return RETURN_OK;
}

/*
** Decode8SVX - Decode 8SVX format audio
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
LONG Decode8SVX(struct IFFSound *sound)
{
    struct ContextNode *cn;
    UBYTE *compressedData;
    UBYTE *sampleData;
    ULONG compressedSize;
    ULONG sampleSize;
    LONG bytesRead;
    LONG error;
    LONG jc;
    
    if (!sound || !sound->iff || !sound->vhdr) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or missing VHDR");
        }
        return RETURN_FAIL;
    }
    
    /* Get current chunk (should be BODY) */
    cn = CurrentChunk(sound->iff);
    if (!cn || cn->cn_ID != ID_BODY) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Expected BODY chunk");
        return RETURN_FAIL;
    }
    
    compressedSize = cn->cn_Size;
    
    /* Calculate sample size */
    sampleSize = sound->vhdr->oneShotHiSamples + sound->vhdr->repeatHiSamples;
    
    /* Allocate compressed data buffer */
    compressedData = (UBYTE *)AllocMem(compressedSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!compressedData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate compressed data buffer");
        return RETURN_FAIL;
    }
    
    /* Read compressed data */
    bytesRead = ReadChunkBytes(sound->iff, compressedData, compressedSize);
    if (bytesRead != compressedSize) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to read BODY chunk");
        return RETURN_FAIL;
    }
    
    /* Allocate sample data buffer */
    sampleData = (UBYTE *)AllocMem(sampleSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!sampleData) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate sample data buffer");
        return RETURN_FAIL;
    }
    
    /* Decompress based on compression type */
    if (sound->vhdr->sCompression == sCmpFibDelta) {
        /* Fibonacci-delta decompression */
        error = DecodeFibonacciDelta(compressedData, compressedSize, sampleData, sampleSize);
        if (error != RETURN_OK) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Fibonacci-delta decompression failed");
            return RETURN_FAIL;
        }
    } else if (sound->vhdr->sCompression == sCmpNone) {
        /* No compression - copy directly */
        if (compressedSize != sampleSize) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_BADFILE, "BODY size mismatch");
            return RETURN_FAIL;
        }
        CopyMem(compressedData, sampleData, sampleSize);
    } else {
        FreeMem(compressedData, compressedSize);
        FreeMem(sampleData, sampleSize);
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported compression type");
        return RETURN_FAIL;
    }
    
    /* Free compressed data */
    FreeMem(compressedData, compressedSize);
    
    /* Store decoded data */
    sound->sampleData = sampleData;
    sound->sampleDataSize = sampleSize;
    sound->isDecoded = TRUE;
    
    return RETURN_OK;
}

/*
** Decode16SV - Decode 16SV format audio
** Returns: RETURN_OK on success, RETURN_FAIL on error
** 16SV is identical to 8SVX except BODY contains 16-bit samples
*/
LONG Decode16SV(struct IFFSound *sound)
{
    struct ContextNode *cn;
    UBYTE *compressedData;
    UBYTE *sampleData;
    ULONG compressedSize;
    ULONG sampleSize;
    LONG bytesRead;
    LONG error;
    UWORD *src16;
    UBYTE *dst8;
    ULONG i;
    
    if (!sound || !sound->iff || !sound->vhdr) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or missing VHDR");
        }
        return RETURN_FAIL;
    }
    
    /* Get current chunk (should be BODY) */
    cn = CurrentChunk(sound->iff);
    if (!cn || cn->cn_ID != ID_BODY) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Expected BODY chunk");
        return RETURN_FAIL;
    }
    
    compressedSize = cn->cn_Size;
    
    /* Calculate sample size - 16SV has 16-bit samples */
    sampleSize = (sound->vhdr->oneShotHiSamples + sound->vhdr->repeatHiSamples) * 2;
    
    /* Allocate compressed data buffer */
    compressedData = (UBYTE *)AllocMem(compressedSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!compressedData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate compressed data buffer");
        return RETURN_FAIL;
    }
    
    /* Read compressed data */
    bytesRead = ReadChunkBytes(sound->iff, compressedData, compressedSize);
    if (bytesRead != compressedSize) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to read BODY chunk");
        return RETURN_FAIL;
    }
    
    /* Allocate sample data buffer (16-bit samples) */
    sampleData = (UBYTE *)AllocMem(sampleSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!sampleData) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate sample data buffer");
        return RETURN_FAIL;
    }
    
    /* Decompress based on compression type */
    if (sound->vhdr->sCompression == sCmpNone) {
        /* No compression - copy directly */
        if (compressedSize != sampleSize) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_BADFILE, "BODY size mismatch");
            return RETURN_FAIL;
        }
        CopyMem(compressedData, sampleData, sampleSize);
    } else {
        /* Compression not supported for 16SV in this implementation */
        FreeMem(compressedData, compressedSize);
        FreeMem(sampleData, sampleSize);
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Compression not supported for 16SV");
        return RETURN_FAIL;
    }
    
    /* Free compressed data */
    FreeMem(compressedData, compressedSize);
    
    /* Store decoded data */
    sound->sampleData = sampleData;
    sound->sampleDataSize = sampleSize;
    sound->isDecoded = TRUE;
    
    return RETURN_OK;
}

/*
** DecodeAIFF - Decode AIFF format audio
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
LONG DecodeAIFF(struct IFFSound *sound)
{
    struct ContextNode *cn;
    UBYTE *compressedData;
    UBYTE *sampleData;
    ULONG compressedSize;
    ULONG sampleSize;
    LONG bytesRead;
    ULONG offset;
    ULONG blockSize;
    UBYTE *src;
    UBYTE *dst;
    ULONG i;
    ULONG channels;
    ULONG bitDepth;
    ULONG sampleFrames;
    
    if (!sound || !sound->iff || !sound->comm) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or missing COMM");
        }
        return RETURN_FAIL;
    }
    
    /* Get current chunk (should be SSND) */
    cn = CurrentChunk(sound->iff);
    if (!cn || cn->cn_ID != ID_SSND) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Expected SSND chunk");
        return RETURN_FAIL;
    }
    
    compressedSize = cn->cn_Size;
    
    /* SSND chunk has 8-byte header: offset (ULONG) and blockSize (ULONG) */
    if (compressedSize < 8) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "SSND chunk too small");
        return RETURN_FAIL;
    }
    
    /* Allocate compressed data buffer */
    compressedData = (UBYTE *)AllocMem(compressedSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!compressedData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate compressed data buffer");
        return RETURN_FAIL;
    }
    
    /* Read compressed data */
    bytesRead = ReadChunkBytes(sound->iff, compressedData, compressedSize);
    if (bytesRead != compressedSize) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to read SSND chunk");
        return RETURN_FAIL;
    }
    
    /* Extract offset and blockSize from SSND header */
    src = compressedData;
    offset = (ULONG)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
    blockSize = (ULONG)((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
    
    /* Calculate sample size */
    channels = sound->comm->numChannels;
    bitDepth = sound->comm->sampleSize;
    sampleFrames = sound->comm->numSampleFrames;
    sampleSize = sampleFrames * channels * ((bitDepth + 7) / 8);
    
    /* Allocate sample data buffer */
    sampleData = (UBYTE *)AllocMem(sampleSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!sampleData) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate sample data buffer");
        return RETURN_FAIL;
    }
    
    /* Extract audio data (skip 8-byte header) */
    {
        UBYTE *audioData;
        ULONG audioDataSize;
        LONG decodeResult;
        
        audioData = compressedData + 8;
        audioDataSize = compressedSize - 8;
        
        /* Decompress based on compression format */
        if (sound->aiffCompression == AIFF_COMPRESSION_NONE ||
            sound->aiffCompression == AIFF_COMPRESSION_LPCM ||
            sound->aiffCompression == AIFF_COMPRESSION_TWOS) {
            /* Uncompressed PCM - may need byte swapping for little-endian */
            decodeResult = DecodePCM(sound, audioData, audioDataSize, sampleData, sampleSize,
                                     bitDepth, channels, sound->isLittleEndian);
            if (decodeResult != RETURN_OK) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                return RETURN_FAIL;
            }
        } else if (sound->aiffCompression == AIFF_COMPRESSION_ULAW ||
                   sound->aiffCompression == AIFF_COMPRESSION_ulaw) {
            /* G.711 mu-Law - converts 8-bit to 16-bit */
            if (bitDepth != 8) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                SetIFFSoundError(sound, IFFSOUND_BADFILE, "ULAW requires 8-bit samples");
                return RETURN_FAIL;
            }
            decodeResult = DecodeULAW(sound, audioData, audioDataSize, sampleData, sampleSize);
            if (decodeResult != RETURN_OK) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                return RETURN_FAIL;
            }
            /* Update sample size - ULAW expands 8-bit to 16-bit */
            sound->sampleDataSize = audioDataSize * 2;
        } else if (sound->aiffCompression == AIFF_COMPRESSION_ALAW ||
                   sound->aiffCompression == AIFF_COMPRESSION_alaw) {
            /* G.711 A-Law - converts 8-bit to 16-bit */
            if (bitDepth != 8) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                SetIFFSoundError(sound, IFFSOUND_BADFILE, "ALAW requires 8-bit samples");
                return RETURN_FAIL;
            }
            decodeResult = DecodeALAW(sound, audioData, audioDataSize, sampleData, sampleSize);
            if (decodeResult != RETURN_OK) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                return RETURN_FAIL;
            }
            /* Update sample size - ALAW expands 8-bit to 16-bit */
            sound->sampleDataSize = audioDataSize * 2;
        } else if (sound->aiffCompression == AIFF_COMPRESSION_FL32 ||
                   sound->aiffCompression == AIFF_COMPRESSION_fl32) {
            /* 32-bit floating point - converts to 16-bit PCM */
            if (bitDepth != 32) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                SetIFFSoundError(sound, IFFSOUND_BADFILE, "FL32 requires 32-bit samples");
                return RETURN_FAIL;
            }
            decodeResult = DecodeFL32(sound, audioData, audioDataSize, sampleData, sampleSize);
            if (decodeResult != RETURN_OK) {
                FreeMem(compressedData, compressedSize);
                FreeMem(sampleData, sampleSize);
                return RETURN_FAIL;
            }
            /* Update sample size - FL32 converts 32-bit to 16-bit */
            sound->sampleDataSize = (audioDataSize / 4) * 2;
        } else {
            /* Unsupported compression format */
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported AIFC compression format");
            return RETURN_FAIL;
        }
    }
    
    /* Free compressed data */
    FreeMem(compressedData, compressedSize);
    
    /* Store decoded data */
    sound->sampleData = sampleData;
    sound->isDecoded = TRUE;
    
    return RETURN_OK;
}

/*
** DecodeMAUD - Decode MAUD format audio
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
LONG DecodeMAUD(struct IFFSound *sound)
{
    struct ContextNode *cn;
    UBYTE *compressedData;
    UBYTE *sampleData;
    ULONG compressedSize;
    ULONG sampleSize;
    LONG bytesRead;
    LONG error;
    LONG jc;
    
    if (!sound || !sound->iff || !sound->maud) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or missing MAUD header");
        }
        return RETURN_FAIL;
    }
    
    /* Get current chunk (should be BODY) */
    cn = CurrentChunk(sound->iff);
    if (!cn || cn->cn_ID != ID_BODY) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Expected BODY chunk");
        return RETURN_FAIL;
    }
    
    compressedSize = cn->cn_Size;
    
    /* Calculate sample size */
    sampleSize = sound->maud->numSampleFrames * sound->maud->numChannels * ((sound->maud->sampleSize + 7) / 8);
    
    /* Allocate compressed data buffer */
    compressedData = (UBYTE *)AllocMem(compressedSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!compressedData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate compressed data buffer");
        return RETURN_FAIL;
    }
    
    /* Read compressed data */
    bytesRead = ReadChunkBytes(sound->iff, compressedData, compressedSize);
    if (bytesRead != compressedSize) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to read BODY chunk");
        return RETURN_FAIL;
    }
    
    /* Allocate sample data buffer */
    sampleData = (UBYTE *)AllocMem(sampleSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!sampleData) {
        FreeMem(compressedData, compressedSize);
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate sample data buffer");
        return RETURN_FAIL;
    }
    
    /* Decompress based on compression type */
    if (sound->maud->compression == 0) {
        /* No compression - copy directly */
        if (compressedSize != sampleSize) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_BADFILE, "BODY size mismatch");
            return RETURN_FAIL;
        }
        CopyMem(compressedData, sampleData, sampleSize);
    } else if (sound->maud->compression == 2) {
        /* ADPCM2 compression */
        jc = 0;
        error = DecodeADPCM2(compressedData, compressedSize, sampleData, sampleSize, &jc);
        if (error != RETURN_OK) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_ERROR, "ADPCM2 decompression failed");
            return RETURN_FAIL;
        }
    } else if (sound->maud->compression == 3) {
        /* ADPCM3 compression */
        jc = 0;
        error = DecodeADPCM3(compressedData, compressedSize, sampleData, sampleSize, &jc);
        if (error != RETURN_OK) {
            FreeMem(compressedData, compressedSize);
            FreeMem(sampleData, sampleSize);
            SetIFFSoundError(sound, IFFSOUND_ERROR, "ADPCM3 decompression failed");
            return RETURN_FAIL;
        }
    } else {
        FreeMem(compressedData, compressedSize);
        FreeMem(sampleData, sampleSize);
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported compression type");
        return RETURN_FAIL;
    }
    
    /* Free compressed data */
    FreeMem(compressedData, compressedSize);
    
    /* Store decoded data */
    sound->sampleData = sampleData;
    sound->sampleDataSize = sampleSize;
    sound->isDecoded = TRUE;
    
    return RETURN_OK;
}

/*
** DecodeFibonacciDelta - Decompress Fibonacci-delta encoded data
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Source format: pad byte, initial value byte, then 2*(n-2) 4-bit encoded samples
** Output: 2*(n-2) decoded samples
*/
static LONG DecodeFibonacciDelta(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize)
{
    /* Fibonacci delta table - matches codeToDelta from iffcode.txt */
    static const LONG codeToDelta[] = {
        -34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21
    };
    
    ULONG i;
    UBYTE x;
    UBYTE d;
    ULONG lim;
    
    if (!src || !dst || srcSize < 2 || dstSize == 0) {
        return RETURN_FAIL;
    }
    
    /* First byte is pad byte, second byte is initial value */
    x = src[1];
    
    /* Process 2*(srcSize-2) samples from remaining bytes */
    lim = (srcSize - 2) << 1;
    if (lim > dstSize) {
        lim = dstSize;
    }
    
    for (i = 0; i < lim; i++) {
        /* Get byte containing nibble pair */
        d = src[2 + (i >> 1)];
        
        /* Extract nibble (high nibble for even i, low nibble for odd i) */
        if (i & 1) {
            d &= 0x0F;  /* Low nibble */
        } else {
            d >>= 4;    /* High nibble */
        }
        
        /* Add delta from table */
        x += (UBYTE)codeToDelta[d];
        
        /* Store decoded sample */
        dst[i] = x;
    }
    
    return RETURN_OK;
}

/*
** DecodeADPCM2 - Decompress ADPCM2 encoded data
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
static LONG DecodeADPCM2(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize, LONG *jc)
{
    LONG step;
    LONG diff;
    LONG sample;
    ULONG i;
    UBYTE code;
    
    if (!src || !dst || srcSize == 0 || dstSize == 0 || !jc) {
        return RETURN_FAIL;
    }
    
    /* ADPCM2: 4 bits per sample, expands to 1 byte per sample */
    if (srcSize * 2 < dstSize) {
        return RETURN_FAIL;
    }
    
    sample = *jc;
    
    for (i = 0; i < srcSize && i * 2 < dstSize; i++) {
        code = src[i];
        
        /* Process high nibble */
        {
            step = 1;
            diff = (code >> 4) & 0x0F;
            if (diff & 0x08) {
                diff = diff | 0xFFFFFFF0; /* Sign extend */
            }
            sample += diff * step;
            if (sample > 127) {
                sample = 127;
            } else if (sample < -128) {
                sample = -128;
            }
            dst[i * 2] = (UBYTE)sample;
        }
        
        /* Process low nibble */
        if (i * 2 + 1 < dstSize) {
            step = 1;
            diff = code & 0x0F;
            if (diff & 0x08) {
                diff = diff | 0xFFFFFFF0; /* Sign extend */
            }
            sample += diff * step;
            if (sample > 127) {
                sample = 127;
            } else if (sample < -128) {
                sample = -128;
            }
            dst[i * 2 + 1] = (UBYTE)sample;
        }
    }
    
    *jc = sample;
    return RETURN_OK;
}

/*
** DecodeADPCM3 - Decompress ADPCM3 encoded data
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
static LONG DecodeADPCM3(UBYTE *src, ULONG srcSize, UBYTE *dst, ULONG dstSize, LONG *jc)
{
    LONG step;
    LONG diff;
    LONG sample;
    ULONG i;
    UBYTE code;
    
    if (!src || !dst || srcSize == 0 || dstSize == 0 || !jc) {
        return RETURN_FAIL;
    }
    
    /* ADPCM3: 3 bits per sample, expands to 1 byte per sample */
    /* 3 samples per byte: bits 7-5, 4-2, 1-0 (with padding) */
    if (srcSize * 8 / 3 < dstSize) {
        return RETURN_FAIL;
    }
    
    sample = *jc;
    
    for (i = 0; i < srcSize && (i * 8 / 3) < dstSize; i++) {
        code = src[i];
        
        /* Process first sample (bits 7-5) */
        {
            step = 1;
            diff = (code >> 5) & 0x07;
            if (diff & 0x04) {
                diff = diff | 0xFFFFFFF8; /* Sign extend */
            }
            sample += diff * step;
            if (sample > 127) {
                sample = 127;
            } else if (sample < -128) {
                sample = -128;
            }
            if ((i * 8 / 3) < dstSize) {
                dst[i * 8 / 3] = (UBYTE)sample;
            }
        }
        
        /* Process second sample (bits 4-2) */
        if ((i * 8 / 3 + 1) < dstSize) {
            step = 1;
            diff = (code >> 2) & 0x07;
            if (diff & 0x04) {
                diff = diff | 0xFFFFFFF8; /* Sign extend */
            }
            sample += diff * step;
            if (sample > 127) {
                sample = 127;
            } else if (sample < -128) {
                sample = -128;
            }
            dst[i * 8 / 3 + 1] = (UBYTE)sample;
        }
        
        /* Process third sample (bits 1-0, with padding) */
        if ((i * 8 / 3 + 2) < dstSize) {
            step = 1;
            diff = (code & 0x03) << 1; /* Shift left to make 3-bit */
            if (diff & 0x04) {
                diff = diff | 0xFFFFFFF8; /* Sign extend */
            }
            sample += diff * step;
            if (sample > 127) {
                sample = 127;
            } else if (sample < -128) {
                sample = -128;
            }
            dst[i * 8 / 3 + 2] = (UBYTE)sample;
        }
    }
    
    *jc = sample;
    return RETURN_OK;
}

