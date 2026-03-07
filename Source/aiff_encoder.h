/*
** aiff_encoder.h - AIFF Encoder Interface
**
** Writes uncompressed AIFF (FORM AIFF) with COMM and SSND chunks.
** Profile matches source: sample rate, channels, and bit depth (8 or 16).
*/

#ifndef AIFF_ENCODER_H
#define AIFF_ENCODER_H

#include <exec/types.h>

/* Forward declaration - full type in iffsound.h */
struct IFFSound;

/* AIFF encoder profile - matches source audio properties */
struct AIFFEncoderProfile {
    ULONG sampleRate;   /* Sample rate in Hz */
    ULONG channels;     /* 1 = mono, 2 = stereo */
    ULONG bitDepth;     /* 8 or 16 bits per sample */
    ULONG numFrames;    /* Number of sample frames (samples per channel) */
};

/*
** AIFFEncoder_Write - Write decoded audio to an AIFF file
**
** sound: IFFSound after ParseIFFSound(), AnalyzeFormat(), and Decode() (or DecodeTo8Bit/DecodeTo16Bit)
** filename: Output path
** pcmData: Pointer to PCM buffer (8-bit unsigned or 16-bit signed BE per AIFF spec)
** dataSize: Size in bytes of pcmData
** profile: Sample rate, channels, bit depth, frame count (must match pcmData)
**
** For 8-bit: pcmData is signed -128..127 from DecodeTo8Bit; encoder converts to unsigned 0..255 for AIFF.
** For 16-bit: pcmData is signed 16-bit big-endian; written as-is.
**
** useAIFC: if TRUE, write FORM AIFC (with NONE compression in COMM); if FALSE, write FORM AIFF.
**
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
LONG AIFFEncoder_Write(const char *filename,
                       UBYTE *pcmData,
                       ULONG dataSize,
                       struct AIFFEncoderProfile *profile,
                       BOOL useAIFC);

#endif /* AIFF_ENCODER_H */
