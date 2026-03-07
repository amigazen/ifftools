/*
** aiff_decompressor.c - AIFF/AIFC Decompression Implementation
**
** Handles all AIFF/AIFC compression formats:
** - Linear PCM (NONE, lpcm, twos, sowt)
** - G.711 mu-Law (ULAW, ulaw)
** - G.711 A-Law (ALAW, alaw)
** - 32-bit floating point (FL32, fl32)
** - 80-bit IEEE 754 extended precision sample rate
*/

#include "iffsound_private.h"
#include "/debug.h"
#include <proto/exec.h>
#include <proto/utility.h>
#include <math.h>

#ifndef HUGE_VAL
#ifdef HUGE
#define HUGE_VAL HUGE
#else
#define HUGE_VAL 1.7976931348623157e+308
#endif
#endif

/*
** IEEE754ReadExtended - Read 80-bit IEEE 754 extended precision float
** Returns: double value
** Based on Motorola 68881/68882/68040 FPU format
*/
double IEEE754ReadExtended(UBYTE *bytes)
{
    int sgn;
    int exp;
    ULONG low;
    ULONG high;
    double out;
    
    if (!bytes) {
        return 0.0;
    }
    
    /* Extract components from big-endian buffer */
    sgn = (int)(bytes[0] >> 7);
    exp = ((int)(bytes[0] & 0x7F) << 8) | ((int)bytes[1]);
    low = ((ULONG)bytes[2] << 24) | ((ULONG)bytes[3] << 16) |
          ((ULONG)bytes[4] << 8) | (ULONG)bytes[5];
    high = ((ULONG)bytes[6] << 24) | ((ULONG)bytes[7] << 16) |
           ((ULONG)bytes[8] << 8) | (ULONG)bytes[9];
    
    /* Handle zero */
    if (exp == 0 && low == 0 && high == 0) {
        return (sgn ? -0.0 : 0.0);
    }
    
    /* Handle infinity and NaN */
    if (exp == 32767) {
        if (low == 0 && high == 0) {
            return (sgn ? -HUGE_VAL : HUGE_VAL);
        } else {
            return (sgn ? -HUGE_VAL : HUGE_VAL); /* NaN */
        }
    }
    
    /* Unbias exponent */
    exp -= 16383;
    
    /* Reconstruct value */
    out = ldexp((double)low, -31 + exp);
    out += ldexp((double)high, -63 + exp);
    
    return (sgn ? -out : out);
}

/*
** DecodeULAW - Decompress G.711 mu-Law to 16-bit PCM
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Converts 8-bit mu-Law samples to 16-bit PCM
*/
LONG DecodeULAW(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize)
{
    UBYTE *src;
    WORD *dst;
    ULONG i;
    UBYTE x;
    WORD sample;
    int sgn;
    int exp;
    int mant;
    int y;
    
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid parameters for ULAW decode");
        }
        return RETURN_FAIL;
    }
    
    /* Output must be at least 2x input size (8-bit -> 16-bit) */
    if (outputSize < inputSize * 2) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Output buffer too small for ULAW decode");
        }
        return RETURN_FAIL;
    }
    
    src = input;
    dst = (WORD *)output;
    
    /* Decode each mu-Law sample */
    for (i = 0; i < inputSize; i++) {
        x = ~src[i]; /* Bits are sent reversed */
        sgn = (int)(x & 0x80);
        x &= 0x7F;
        mant = ((int)(x & 0xF) << 1) | 0x21; /* Mantissa plus hidden bits */
        exp = (int)((x & 0x70) >> 4); /* Exponent */
        mant <<= exp;
        
        /* Subtract 33 and convert 14-bit to 16-bit */
        y = (sgn ? (-mant + 33) : (mant - 33));
        sample = (WORD)(y << 2);
        
        /* Write as big-endian 16-bit */
        dst[i] = (WORD)((sample << 8) | ((ULONG)sample >> 8));
    }
    
    return RETURN_OK;
}

/*
** DecodeALAW - Decompress G.711 A-Law to 16-bit PCM
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Converts 8-bit A-Law samples to 16-bit PCM
*/
LONG DecodeALAW(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize)
{
    UBYTE *src;
    WORD *dst;
    ULONG i;
    UBYTE x;
    WORD sample;
    int sgn;
    int exp;
    int mant;
    int y;
    
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid parameters for ALAW decode");
        }
        return RETURN_FAIL;
    }
    
    /* Output must be at least 2x input size (8-bit -> 16-bit) */
    if (outputSize < inputSize * 2) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Output buffer too small for ALAW decode");
        }
        return RETURN_FAIL;
    }
    
    src = input;
    dst = (WORD *)output;
    
    /* Decode each A-Law sample */
    for (i = 0; i < inputSize; i++) {
        /* Bits at even positions are inverted, sign bit is inverted */
        x = ((~src[i] & 0xD5) | (src[i] & 0x2A));
        sgn = (int)(x & 0x80);
        x &= 0x7F;
        mant = ((int)(x & 0xF) << 1) | 0x21;
        exp = (int)((x & 0x70) >> 4);
        
        /* Denormalized numbers: remove hidden bit */
        if (exp == 0) {
            mant &= ~0x20;
        } else {
            mant <<= (exp - 1);
        }
        
        y = (sgn ? -mant : mant);
        sample = (WORD)(y << 3); /* Convert 13-bit to 16-bit */
        
        /* Write as big-endian 16-bit */
        dst[i] = (WORD)((sample << 8) | ((ULONG)sample >> 8));
    }
    
    return RETURN_OK;
}

/*
** DecodeFL32 - Decompress 32-bit floating point to 16-bit PCM
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Converts IEEE 754 32-bit float samples to 16-bit PCM
*/
LONG DecodeFL32(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize)
{
    ULONG *src;
    WORD *dst;
    ULONG i;
    ULONG floatBits;
    ULONG sampleCount;
    int sgn;
    int exp;
    ULONG mantissa;
    LONG sample;
    float floatVal;
    
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid parameters for FL32 decode");
        }
        return RETURN_FAIL;
    }
    
    /* Input must be multiple of 4 bytes (32-bit floats) */
    if (inputSize % 4 != 0) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_BADFILE, "FL32 input size not multiple of 4");
        }
        return RETURN_FAIL;
    }
    
    sampleCount = inputSize / 4;
    
    /* Output must be at least 2x sample count (32-bit -> 16-bit) */
    if (outputSize < sampleCount * 2) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Output buffer too small for FL32 decode");
        }
        return RETURN_FAIL;
    }
    
    src = (ULONG *)input;
    dst = (WORD *)output;
    
    /* Decode each float sample */
    for (i = 0; i < sampleCount; i++) {
        /* Read big-endian 32-bit float */
        floatBits = (ULONG)((src[i] << 24) | ((src[i] & 0xFF00) << 8) |
                            ((src[i] & 0xFF0000) >> 8) | (src[i] >> 24));
        
        /* Handle zero */
        if (floatBits == 0 || floatBits == 0x80000000UL) {
            sample = 0;
        } else {
            sgn = (int)(floatBits >> 31);
            exp = (int)((floatBits >> 23) & 0xFF);
            mantissa = floatBits & 0x7FFFFF;
            
            /* Handle infinity and NaN */
            if (exp == 255) {
                if (mantissa == 0) {
                    sample = (sgn ? -32767 : 32767);
                } else {
                    sample = 0; /* NaN */
                }
            } else if (exp == 0) {
                /* Denormalized number */
                exp = -126;
                floatVal = ldexp((float)mantissa, -23 + exp);
            } else {
                /* Normalized number */
                mantissa |= 0x800000;
                exp -= 127;
                floatVal = ldexp((float)mantissa, -23 + exp);
            }
            
            /* Quantize to 16-bit PCM */
            if (exp == 255) {
                /* Already handled above */
            } else {
                /* Multiply by 2^31 and extract 16 bits */
                exp += 8;
                if (exp < 0) {
                    sample = 0;
                } else if (exp > 31) {
                    sample = (sgn ? -32767 : 32767);
                } else {
                    sample = (LONG)(floatVal * 32767.0);
                    if (sample > 32767) {
                        sample = 32767;
                    } else if (sample < -32768) {
                        sample = -32768;
                    }
                }
            }
        }
        
        /* Write as big-endian 16-bit */
        dst[i] = (WORD)((sample << 8) | ((ULONG)sample >> 8));
    }
    
    return RETURN_OK;
}

/*
** DecodePCM - Handle PCM data with optional byte swapping
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Handles both big-endian and little-endian PCM
*/
LONG DecodePCM(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize, ULONG bitDepth, ULONG channels, BOOL littleEndian)
{
    ULONG sampleCount;
    ULONG bytesPerSample;
    ULONG i;
    UBYTE *src;
    UBYTE *dst;
    UBYTE b0, b1, b2, b3;
    
    if (!input || !output || inputSize == 0 || outputSize == 0) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid parameters for PCM decode");
        }
        return RETURN_FAIL;
    }
    
    bytesPerSample = (bitDepth + 7) / 8;
    sampleCount = inputSize / bytesPerSample;
    
    if (outputSize < inputSize) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Output buffer too small for PCM decode");
        }
        return RETURN_FAIL;
    }
    
    src = input;
    dst = output;
    
    /* If little-endian and multi-byte samples, swap bytes */
    if (littleEndian && bytesPerSample > 1) {
        for (i = 0; i < sampleCount; i++) {
            if (bytesPerSample == 2) {
                /* 16-bit: swap bytes */
                b0 = src[i * 2];
                b1 = src[i * 2 + 1];
                dst[i * 2] = b1;
                dst[i * 2 + 1] = b0;
            } else if (bytesPerSample == 3) {
                /* 24-bit: swap bytes */
                b0 = src[i * 3];
                b1 = src[i * 3 + 1];
                b2 = src[i * 3 + 2];
                dst[i * 3] = b2;
                dst[i * 3 + 1] = b1;
                dst[i * 3 + 2] = b0;
            } else if (bytesPerSample == 4) {
                /* 32-bit: swap bytes */
                b0 = src[i * 4];
                b1 = src[i * 4 + 1];
                b2 = src[i * 4 + 2];
                b3 = src[i * 4 + 3];
                dst[i * 4] = b3;
                dst[i * 4 + 1] = b2;
                dst[i * 4 + 2] = b1;
                dst[i * 4 + 3] = b0;
            } else {
                /* Unsupported bit depth */
                if (sound) {
                    SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported bit depth for PCM");
                }
                return RETURN_FAIL;
            }
        }
    } else {
        /* Big-endian or 8-bit: copy directly */
        CopyMem(input, output, inputSize);
    }
    
    return RETURN_OK;
}

