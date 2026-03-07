/*
** audio_converter.c - Audio Format Converter Implementation (Internal to Library)
**
** Converts decoded audio data to ready-to-play formats (8-bit, 16-bit)
** Similar to bitmap_renderer.c for images
*/

#include "iffsound_private.h"
#include "/debug.h"
#include <proto/exec.h>
#include <proto/utility.h>

/* Forward declarations */
static LONG Convert16BitTo8Bit(UBYTE *src16, ULONG srcSize, UBYTE *dst8, ULONG dstSize);
static LONG Convert8BitTo16Bit(UBYTE *src8, ULONG srcSize, UWORD *dst16, ULONG dstSize);

/*
** DecodeTo8Bit - Decode audio to 8-bit PCM format ready for playback
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Converts audio to signed 8-bit samples (range -128 to 127)
** Suitable for Amiga audio.device playback
*/
LONG DecodeTo8Bit(struct IFFSound *sound, UBYTE **data, ULONG *size)
{
    LONG error;
    ULONG sampleCount;
    ULONG bitDepth;
    ULONG channels;
    ULONG outputSize;
    UBYTE *outputData;
    UBYTE *inputData;
    ULONG inputSize;
    
    if (!sound || !data || !size) {
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
    
    if (!sound->sampleData || sound->sampleDataSize == 0) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "No sample data available");
        return RETURN_FAIL;
    }
    
    sampleCount = GetSampleCount(sound);
    bitDepth = GetBitDepth(sound);
    channels = GetChannels(sound);
    
    if (sampleCount == 0 || channels == 0) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sample count or channels");
        return RETURN_FAIL;
    }
    
    /* Calculate output size: sampleCount * channels * 1 byte per sample */
    outputSize = sampleCount * channels;
    
    /* If already 8-bit, return existing data */
    if (bitDepth == 8) {
        *data = sound->sampleData;
        *size = sound->sampleDataSize;
        return RETURN_OK;
    }
    
    /* Allocate output buffer for 8-bit samples */
    outputData = (UBYTE *)AllocMem(outputSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!outputData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate 8-bit output buffer");
        return RETURN_FAIL;
    }
    
    inputData = sound->sampleData;
    inputSize = sound->sampleDataSize;
    
    /* Convert based on source bit depth */
    if (bitDepth == 16) {
        /* Convert 16-bit to 8-bit */
        error = Convert16BitTo8Bit(inputData, inputSize, outputData, outputSize);
        if (error != RETURN_OK) {
            FreeMem(outputData, outputSize);
            SetIFFSoundError(sound, IFFSOUND_ERROR, "16-bit to 8-bit conversion failed");
            return RETURN_FAIL;
        }
    } else {
        /* Unsupported bit depth for conversion */
        FreeMem(outputData, outputSize);
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported bit depth for 8-bit conversion");
        return RETURN_FAIL;
    }
    
    *data = outputData;
    *size = outputSize;
    
    return RETURN_OK;
}

/*
** DecodeTo16Bit - Decode audio to 16-bit PCM format ready for playback
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Converts audio to signed 16-bit samples in big-endian format (range -32768 to 32767)
** Suitable for high-quality playback or conversion to other formats
*/
LONG DecodeTo16Bit(struct IFFSound *sound, UWORD **data, ULONG *size)
{
    LONG error;
    ULONG sampleCount;
    ULONG bitDepth;
    ULONG channels;
    ULONG outputSize;
    UWORD *outputData;
    UBYTE *inputData;
    ULONG inputSize;
    
    if (!sound || !data || !size) {
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
    
    if (!sound->sampleData || sound->sampleDataSize == 0) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "No sample data available");
        return RETURN_FAIL;
    }
    
    sampleCount = GetSampleCount(sound);
    bitDepth = GetBitDepth(sound);
    channels = GetChannels(sound);
    
    if (sampleCount == 0 || channels == 0) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sample count or channels");
        return RETURN_FAIL;
    }
    
    /* Calculate output size: sampleCount * channels * 2 bytes per sample */
    outputSize = sampleCount * channels * 2;
    
    /* If already 16-bit, return existing data */
    if (bitDepth == 16) {
        *data = (UWORD *)sound->sampleData;
        *size = sound->sampleDataSize;
        return RETURN_OK;
    }
    
    /* Allocate output buffer for 16-bit samples */
    outputData = (UWORD *)AllocMem(outputSize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!outputData) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate 16-bit output buffer");
        return RETURN_FAIL;
    }
    
    inputData = sound->sampleData;
    inputSize = sound->sampleDataSize;
    
    /* Convert based on source bit depth */
    if (bitDepth == 8) {
        /* Convert 8-bit to 16-bit */
        error = Convert8BitTo16Bit(inputData, inputSize, outputData, outputSize);
        if (error != RETURN_OK) {
            FreeMem(outputData, outputSize);
            SetIFFSoundError(sound, IFFSOUND_ERROR, "8-bit to 16-bit conversion failed");
            return RETURN_FAIL;
        }
    } else {
        /* Unsupported bit depth for conversion */
        FreeMem(outputData, outputSize);
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported bit depth for 16-bit conversion");
        return RETURN_FAIL;
    }
    
    *data = outputData;
    *size = outputSize;
    
    return RETURN_OK;
}

/*
** Convert16BitTo8Bit - Convert 16-bit samples to 8-bit
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Scales 16-bit samples down to 8-bit range
*/
static LONG Convert16BitTo8Bit(UBYTE *src16, ULONG srcSize, UBYTE *dst8, ULONG dstSize)
{
    UWORD *src;
    UBYTE *dst;
    ULONG i;
    ULONG sampleCount;
    WORD sample16;
    UBYTE sample8;
    
    if (!src16 || !dst8 || srcSize == 0 || dstSize == 0) {
        return RETURN_FAIL;
    }
    
    /* Calculate number of 16-bit samples */
    sampleCount = srcSize / 2;
    if (sampleCount > dstSize) {
        sampleCount = dstSize;
    }
    
    src = (UWORD *)src16;
    dst = dst8;
    
    /* Convert each sample */
    for (i = 0; i < sampleCount; i++) {
        /* Read 16-bit sample (big-endian format in IFF files) */
        /* IFF files store data in big-endian, Amiga is big-endian, so read directly */
        sample16 = (WORD)src[i];
        
        /* Scale signed 16-bit (-32768..32767) to unsigned 8-bit (0..255) for Amiga */
        /* Formula: ((sample16 + 32768) * 255) / 65535 */
        {
            ULONG unsigned16;
            unsigned16 = (ULONG)((LONG)sample16 + 32768L);
            sample8 = (UBYTE)((unsigned16 * 255UL) / 65535UL);
        }
        
        dst[i] = sample8;
    }
    
    return RETURN_OK;
}

/*
** Convert8BitTo16Bit - Convert 8-bit samples to 16-bit
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Scales 8-bit samples up to 16-bit range
*/
static LONG Convert8BitTo16Bit(UBYTE *src8, ULONG srcSize, UWORD *dst16, ULONG dstSize)
{
    UBYTE *src;
    UWORD *dst;
    ULONG i;
    ULONG sampleCount;
    UBYTE sample8;
    WORD sample16;
    
    if (!src8 || !dst16 || srcSize == 0 || dstSize == 0) {
        return RETURN_FAIL;
    }
    
    /* Calculate number of samples */
    sampleCount = srcSize;
    if (sampleCount * 2 > dstSize) {
        sampleCount = dstSize / 2;
    }
    
    src = src8;
    dst = dst16;
    
    /* Convert each sample */
    for (i = 0; i < sampleCount; i++) {
        /* Read 8-bit sample (unsigned 0..255 for Amiga) */
        sample8 = src[i];
        
        /* Convert from unsigned 8-bit (0..255) to signed 16-bit (-32768..32767) */
        /* Formula: ((sample8 * 65535) / 255) - 32768 */
        {
            ULONG unsigned16;
            unsigned16 = ((ULONG)sample8 * 65535UL) / 255UL;
            sample16 = (WORD)((LONG)unsigned16 - 32768L);
        }
        
        /* Write 16-bit sample (big-endian format for IFF files) */
        /* IFF files store data in big-endian, Amiga is big-endian, so write directly */
        dst[i] = (UWORD)sample16;
    }
    
    return RETURN_OK;
}

