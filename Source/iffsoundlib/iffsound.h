/*
**	$VER: iffsound.h 1.0 (19.12.2025)
**
**      IFFSound library structures and constants
**
*/

#ifndef IFFSOUND_H
#define IFFSOUND_H

/*****************************************************************************/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif

/*****************************************************************************/

/* Forward declarations for opaque and external types */
struct IFFSound;      /* Opaque structure - use accessor functions */
struct Voice8Header;  /* Public structure defined below */
struct AIFFCommon;    /* Public structure defined below */
struct MAUDHeader;    /* Public structure defined below */
/* Note: struct IFFHandle is defined in libraries/iffparse.h */

/*****************************************************************************/

/* Fixed-point type definition - used for panning and volume values */
typedef LONG Fixed;     /* A fixed-point value, 16 bits to the left of
                         * the point and 16 to the right. A Fixed is a
                         * number of 2**16ths, i.e. 65536ths. */
#define Unity 0x10000L  /* Unity = Fixed 1.0 = maximum volume */

/*****************************************************************************/

/* MAKE_ID macro for creating IFF chunk identifiers */
#define MAKE_ID(a,b,c,d) \
        ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))

/*****************************************************************************/

/* Factory Functions - following iffparse.library pattern
 *
 * AllocIFFSound() - Creates and initializes a new IFFSound structure.
 *                  This is the only supported way to create an IFFSound
 *                  since there are private fields that need initialization.
 *                  Returns NULL if allocation fails.
 *
 * FreeIFFSound() - Deallocates all resources associated with an IFFSound
 *                 structure. The structure MUST have already been closed
 *                 with CloseIFFSound(). Frees all allocated memory
 *                 including headers and sample data.
 */
struct IFFSound *AllocIFFSound(VOID);
VOID FreeIFFSound(struct IFFSound *sound);

/*****************************************************************************/

/* Loading Functions - following iffparse.library pattern
 *
 * InitIFFSoundasDOS() - Initializes the IFFSound to operate on DOS streams.
 *                      Allocates and initializes an internal IFFHandle structure.
 *                      The iff_Stream field must be set by the caller after
 *                      calling Open() to get a BPTR file handle.
 *
 * OpenIFFSound() - Prepares an IFFSound to read or write a new IFF stream.
 *                 The direction of I/O is given by rwMode (IFFF_READ or
 *                 IFFF_WRITE). The IFFSound must have been initialized
 *                 with InitIFFSoundasDOS() and iff_Stream must be set
 *                 to a valid BPTR file handle before calling this function.
 *                 Returns 0 on success or an error code on failure.
 *
 * CloseIFFSound() - Completes an IFF read or write operation by closing the
 *                  IFF context. The IFFHandle structure is freed. The file
 *                  handle (iff_Stream) is NOT closed - the caller is
 *                  responsible for closing it with Close(). This matches
 *                  iffparse.library behavior where CloseIFF() doesn't close
 *                  the file handle.
 *
 * ParseIFFSound() - Parses the IFF file structure and reads property chunks
 *                  (VHDR, COMM, etc.) into the IFFSound structure.
 *                  Must be called after OpenIFFSound(). Returns 0 on
 *                  success or an error code on failure.
 */
VOID InitIFFSoundasDOS(struct IFFSound *sound);
LONG OpenIFFSound(struct IFFSound *sound, LONG rwMode);
VOID CloseIFFSound(struct IFFSound *sound);
LONG ParseIFFSound(struct IFFSound *sound);

/*****************************************************************************/

/* Chunk Reading Functions - following iffparse.library pattern
 *
 * These functions read specific IFF chunks from an opened IFFSound.
 * They use iffparse.library's FindProp() to locate stored property chunks
 * that were declared with PropChunk() during ParseIFFSound().
 *
 * ReadVHDR() - Reads the VHDR (Voice Header) chunk for 8SVX format.
 *             Stores it in the IFFSound structure. Contains sample rate,
 *             octave count, compression type, and volume information.
 *
 * ReadCOMM() - Reads the COMM (Common) chunk for AIFF format.
 *             Contains sample rate, channels, bit depth, and sample count.
 *
 * ReadMAUD() - Reads the MAUD header chunk for MAUD format.
 *             Contains sample rate, channels, bit depth, and format info.
 *
 * ReadBODY() - Reads the BODY chunk header information. The BODY chunk
 *             contains the actual audio sample data. This function stores
 *             the chunk size and position for later reading during decoding.
 *
 * Metadata Chunk Reading Functions:
 *
 * ReadNAME() - Reads the NAME chunk. Returns a pointer to a null-terminated
 *              string in IFFSound's memory, or NULL if not found. Pointer is
 *              valid until FreeIFFSound() is called. Library owns the memory.
 *
 * ReadCopyright() - Reads the Copyright chunk. Returns a pointer to a
 *                   null-terminated string in IFFSound's memory, or NULL if
 *                   not found. Pointer is valid until FreeIFFSound() is called.
 *
 * ReadAuthor() - Reads the AUTH chunk. Returns a pointer to a null-terminated
 *                string in IFFSound's memory, or NULL if not found. Pointer is
 *                valid until FreeIFFSound() is called. Library owns the memory.
 *
 * ReadAnnotation() - Reads the ANNO chunk (first instance). Returns a pointer
 *                    to a null-terminated string in IFFSound's memory, or NULL
 *                    if not found. Multiple ANNO chunks can exist; this returns
 *                    the first one. Pointer is valid until FreeIFFSound() is
 *                    called. Library owns the memory.
 *
 * ReadAllAnnotations() - Reads all ANNO chunks. Returns a pointer to a
 *                        TextList structure containing count and array pointer
 *                        into IFFSound's memory, or NULL if not found.
 *                        Pointers are valid until FreeIFFSound() is called.
 *
 * ReadATAK() - Reads the ATAK chunk (attack envelope) for 8SVX format.
 *             Returns a pointer to an EGPointList structure, or NULL if not found.
 *             Pointer is valid until FreeIFFSound() is called.
 *
 * ReadRLSE() - Reads the RLSE chunk (release envelope) for 8SVX format.
 *             Returns a pointer to an EGPointList structure, or NULL if not found.
 *             Pointer is valid until FreeIFFSound() is called.
 */
LONG ReadVHDR(struct IFFSound *sound);
LONG ReadCOMM(struct IFFSound *sound);
LONG ReadMAUD(struct IFFSound *sound);
LONG ReadBODY(struct IFFSound *sound);

/* Metadata chunk structures */
struct EGPoint {
    UWORD duration;     /* segment duration in milliseconds, > 0 */
    ULONG dest;         /* destination volume factor (Fixed point) */
};

struct EGPointList {
    ULONG count;                /* Number of envelope points */
    struct EGPoint *points;     /* Array of EGPoint structures */
};

struct TextList {
    ULONG count;                /* Number of text strings */
    STRPTR *texts;              /* Array of null-terminated strings */
};

/* 8SVX Loop structure (for SEQN chunk) */
struct LoopPair {
    ULONG start;                /* Loop start offset (LONGWORD aligned) */
    ULONG end;                  /* Loop end offset (LONGWORD aligned) */
};

/* 8SVX Loop List structure */
struct LoopList {
    ULONG count;                /* Number of loop pairs */
    struct LoopPair *loops;      /* Array of loop pairs */
};

/* AIFF Marker structure (public) */
struct AIFFMarker {
    UWORD id;                   /* Marker ID */
    ULONG position;             /* Position in sample frames */
    STRPTR name;                /* Marker name (null-terminated) */
};

/* AIFF Marker List structure */
struct AIFFMarkerList {
    ULONG count;                /* Number of markers */
    struct AIFFMarker *markers; /* Array of markers */
};

/* AIFF Loop structure (public) */
struct AIFFLoop {
    WORD playMode;              /* Play mode (0=no loop, 1=forward, 2=forward/backward) */
    UWORD beginLoop;            /* Begin loop marker ID */
    UWORD endLoop;              /* End loop marker ID */
};

/* AIFF Instrument structure (public) */
struct AIFFInstrument {
    UBYTE baseNote;             /* Base note (MIDI note number) */
    UBYTE detune;               /* Detune in cents (-50 to +50) */
    UBYTE lowNote;              /* Low note of range */
    UBYTE highNote;             /* High note of range */
    UBYTE lowVelocity;          /* Low velocity of range */
    UBYTE highVelocity;         /* High velocity of range */
    WORD gain;                  /* Gain in decibels */
    struct AIFFLoop sustainLoop; /* Sustain loop */
    struct AIFFLoop releaseLoop; /* Release loop */
};

/* AIFF Comment structure (public) */
struct AIFFComment {
    ULONG timeStamp;            /* Timestamp (seconds since 1904) */
    UWORD marker;               /* Associated marker ID (0 if none) */
    STRPTR text;                /* Comment text (null-terminated) */
};

/* AIFF Comment List structure */
struct AIFFCommentList {
    ULONG count;                /* Number of comments */
    struct AIFFComment *comments; /* Array of comments */
};

/* Metadata chunk reading functions
 * 
 * All functions return pointers into IFFSound's memory.
 * Pointers are valid until FreeIFFSound() is called.
 * Library owns all memory - caller must NOT free.
 * 
 * For chunks that can appear multiple times (ANNO, MARK, COMT),
 * use ReadAllX() functions to get all instances.
 */
STRPTR ReadNAME(struct IFFSound *sound);
STRPTR ReadCopyright(struct IFFSound *sound);
STRPTR ReadAuthor(struct IFFSound *sound);
STRPTR ReadAnnotation(struct IFFSound *sound);
struct TextList *ReadAllAnnotations(struct IFFSound *sound);
struct EGPointList *ReadATAK(struct IFFSound *sound);
struct EGPointList *ReadRLSE(struct IFFSound *sound);

/* 8SVX extended metadata chunk reading functions */
ULONG ReadCHAN(struct IFFSound *sound);  /* Returns channel assignment (RIGHT=4, LEFT=2, STEREO=6), 0 if not found */
Fixed ReadPAN(struct IFFSound *sound);   /* Returns panning position (0 to Unity), 0 if not found */
struct LoopList *ReadSEQN(struct IFFSound *sound);  /* Returns sequence/loop definitions */
ULONG ReadFADE(struct IFFSound *sound);  /* Returns fade loop number, 0 if not found */

/* FVER chunk reading functions (format-dependent) */
ULONG ReadFVER(struct IFFSound *sound);  /* Returns format version timestamp (AIFC only), 0 if not found */
STRPTR ReadFVERString(struct IFFSound *sound);  /* Returns version string (other formats), NULL if not found */
struct AIFFMarkerList *ReadAllMarkers(struct IFFSound *sound);
struct AIFFInstrument *ReadINST(struct IFFSound *sound);
struct AIFFCommentList *ReadAllComments(struct IFFSound *sound);

/*****************************************************************************/

/* Getter Functions - following iffparse.library pattern
 *
 * These functions provide read-only access to IFFSound data. They return
 * values from the internal structure without exposing implementation details.
 *
 * GetIFFHandle() - Returns a pointer to the internal IFFHandle structure.
 *                 This allows the caller to set iff_Stream after calling
 *                 InitIFFSoundasDOS() and before calling OpenIFFSound().
 *                 Returns NULL if the IFFSound is not initialized.
 *
 * GetSampleRate() - Returns the sample rate in Hz. Returns 0 if not loaded.
 *
 * GetChannels() - Returns the number of audio channels (1=mono, 2=stereo).
 *                Returns 0 if not loaded.
 *
 * GetBitDepth() - Returns the bit depth (8, 16, 24, 32). Returns 0 if not loaded.
 *
 * GetSampleCount() - Returns the total number of samples. Returns 0 if not loaded.
 *
 * GetFormType() - Returns the IFF FORM type identifier (e.g., ID_8SVX, ID_AIFF).
 *                This identifies the audio format variant.
 *
 * GetVHDR() - Return pointer to the internal Voice8Header structure (8SVX).
 *            This pointer remains valid until the IFFSound is freed.
 *            Returns NULL if not loaded or not 8SVX format.
 *
 * GetCOMM() - Return pointer to the internal AIFFCommon structure (AIFF).
 *             This pointer remains valid until the IFFSound is freed.
 *             Returns NULL if not loaded or not AIFF format.
 *
 * GetMAUD() - Return pointer to the internal MAUDHeader structure (MAUD).
 *             This pointer remains valid until the IFFSound is freed.
 *             Returns NULL if not loaded or not MAUD format.
 *
 * GetSampleData() - Returns a pointer to the decoded sample data buffer.
 *                   The data format depends on the format type.
 *                   Returns NULL if audio has not been decoded.
 *
 * GetSampleDataSize() - Returns the size in bytes of the decoded sample data.
 *                       This is useful for calculating buffer sizes.
 *
 * IsCompressed() - Boolean query about compression. Determined during format analysis.
 *
 * GetSoundInfo() - Returns a pointer to an IFFSoundInfo structure containing
 *                  all core audio properties (sample rate, channels, format, etc.)
 *                  in a single structure. This is useful for getting a complete
 *                  overview of the audio without making multiple function calls.
 *                  The structure is allocated statically and remains valid until
 *                  the next call to GetSoundInfo() or until the IFFSound is freed.
 *                  Returns NULL if the sound is invalid or not loaded.
 *
 * GetDuration() - Returns the duration of the audio in milliseconds.
 *                Calculated from sample count and sample rate.
 *                Returns 0 if sound is not loaded or sample rate is 0.
 *
 * GetDurationSeconds() - Returns the duration of the audio in seconds
 *                        (as a floating-point value). Returns 0.0 if sound
 *                        is not loaded or sample rate is 0.
 *
 * GetBitRate() - Returns the bit rate in bits per second (bps).
 *               Calculated from sample rate, bit depth, and channels.
 *               Returns 0 if sound is not loaded.
 *
 * GetByteRate() - Returns the byte rate in bytes per second.
 *                Calculated from sample rate, bit depth, and channels.
 *                Returns 0 if sound is not loaded.
 *
 * GetLength() - Returns the length of the audio in sample frames.
 *              This is the same as GetSampleCount() but with a more
 *              descriptive name. Returns 0 if sound is not loaded.
 */
struct IFFHandle *GetIFFHandle(struct IFFSound *sound);
ULONG GetSampleRate(struct IFFSound *sound);
ULONG GetChannels(struct IFFSound *sound);
ULONG GetBitDepth(struct IFFSound *sound);
ULONG GetSampleCount(struct IFFSound *sound);
ULONG GetFormType(struct IFFSound *sound);
struct Voice8Header *GetVHDR(struct IFFSound *sound);
struct AIFFCommon *GetCOMM(struct IFFSound *sound);
struct MAUDHeader *GetMAUD(struct IFFSound *sound);
UBYTE *GetSampleData(struct IFFSound *sound);
ULONG GetSampleDataSize(struct IFFSound *sound);
BOOL IsCompressed(struct IFFSound *sound);
ULONG GetDuration(struct IFFSound *sound);
double GetDurationSeconds(struct IFFSound *sound);
ULONG GetBitRate(struct IFFSound *sound);
ULONG GetByteRate(struct IFFSound *sound);
ULONG GetLength(struct IFFSound *sound);

/* IFFSoundInfo structure - aggregate of core audio properties */
struct IFFSoundInfo {
    ULONG sampleRate;           /* Sample rate in Hz */
    ULONG channels;             /* Number of channels (1=mono, 2=stereo) */
    ULONG bitDepth;            /* Bit depth (8, 16, 24, 32) */
    ULONG sampleCount;         /* Total number of sample frames */
    ULONG formType;            /* IFF FORM type (ID_8SVX, ID_16SV, ID_AIFF, ID_MAUD) */
    ULONG compressedSize;      /* Size of compressed audio data in bytes (BODY chunk size, 0 if not loaded) */
    ULONG decodedSize;         /* Size of decoded sample data in bytes (0 if not decoded) */
    ULONG duration;            /* Duration in milliseconds (0 if not loaded or sample rate is 0) */
    ULONG bitRate;             /* Bit rate in bits per second (0 if not loaded) */
    ULONG byteRate;            /* Byte rate in bytes per second (0 if not loaded) */
    BOOL isCompressed;         /* TRUE if audio data is compressed */
    BOOL isLoaded;             /* TRUE if audio has been loaded/parsed */
    BOOL isDecoded;            /* TRUE if audio has been decoded */
};

struct IFFSoundInfo *GetSoundInfo(struct IFFSound *sound);

/*****************************************************************************/

/* Decoding Functions
 *
 * Decode() - Decodes the IFF audio data into an internal format suitable
 *            for further processing. Handles all IFF audio formats (8SVX,
 *            16SV, AIFF, MAUD) and compression methods (Fibonacci-delta,
 *            ADPCM2, ADPCM3, etc.). Must be called after ParseIFFSound().
 *            Returns 0 on success or an error code on failure.
 *
 * DecodeToPCM() - Decodes the IFF audio and converts it to PCM format.
 *                Allocates a buffer containing PCM data suitable for playback
 *                or conversion to other formats. The caller is responsible
 *                for freeing the returned buffer.
 *                Returns 0 on success or an error code on failure.
 *                Note: The returned pcmData may point to sound->sampleData,
 *                which is freed by FreeIFFSound(). Do not free it separately.
 *
 * DecodeTo8Bit() - Decodes the IFF audio and converts it to 8-bit PCM format
 *                  ready for Amiga audio.device playback. For 16-bit sources,
 *                  samples are scaled down to 8-bit. The returned buffer
 *                  contains signed 8-bit samples (range -128 to 127).
 *                  Returns 0 on success or an error code on failure.
 *                  Note: The returned data may point to sound->sampleData,
 *                  which is freed by FreeIFFSound(). Do not free it separately.
 *
 * DecodeTo16Bit() - Decodes the IFF audio and converts it to 16-bit PCM format
 *                   ready for playback. For 8-bit sources, samples are scaled
 *                   up to 16-bit. The returned buffer contains signed 16-bit
 *                   samples in big-endian format (range -32768 to 32767).
 *                   Returns 0 on success or an error code on failure.
 *                   Note: The returned data may point to sound->sampleData,
 *                   which is freed by FreeIFFSound(). Do not free it separately.
 */
LONG Decode(struct IFFSound *sound);
LONG DecodeToPCM(struct IFFSound *sound, UBYTE **pcmData, ULONG *size);
LONG DecodeTo8Bit(struct IFFSound *sound, UBYTE **data, ULONG *size);
LONG DecodeTo16Bit(struct IFFSound *sound, UWORD **data, ULONG *size);

/*****************************************************************************/

/* Analysis Functions
 *
 * AnalyzeFormat() - Analyzes the loaded IFF audio to determine its properties
 *                   such as format type, compression, sample rate, channels,
 *                   and bit depth. This information is used to optimize decoding.
 *                   Must be called after ParseIFFSound() and before decoding.
 *                   Returns 0 on success or an error code on failure.
 */
LONG AnalyzeFormat(struct IFFSound *sound);

/*****************************************************************************/

/* Error Handling Functions
 *
 * IFFSoundError() - Returns the last error code that occurred during an
 *                   IFFSound operation. Error codes are negative values
 *                   (IFFSOUND_* constants) or 0 for success.
 *
 * IFFSoundErrorString() - Returns a pointer to a null-terminated string
 *                         describing the last error. The string is stored
 *                         in the IFFSound structure and remains valid
 *                         until the next operation or until the IFFSound
 *                         is freed.
 */
LONG IFFSoundError(struct IFFSound *sound);
const char *IFFSoundErrorString(struct IFFSound *sound);

/*****************************************************************************/

/* Voice8Header structure - public (8SVX format)
 *
 * This structure represents the IFF VHDR (Voice Header) chunk for 8SVX format.
 * It contains all the metadata needed to interpret the audio data, including
 * sample counts, sample rate, octave count, compression method, and volume.
 *
 * Note: The structure must match the IFF VHDR chunk layout exactly to allow
 * direct reading from IFF files. Field order and types are critical for
 * correct parsing.
 */

/* sCompression: Choice of compression algorithm applied to the samples. */
#define sCmpNone       0        /* not compressed */
#define sCmpFibDelta   1        /* Fibonacci-delta encoding */

struct Voice8Header {
    ULONG oneShotHiSamples;     /* # samples in the high octave 1-shot part */
    ULONG repeatHiSamples;      /* # samples in the high octave repeat part */
    ULONG samplesPerHiCycle;    /* # samples/cycle in high octave, else 0 */
    UWORD samplesPerSec;        /* data sampling rate */
    UBYTE ctOctave;             /* # of octaves of waveforms */
    UBYTE sCompression;         /* data compression technique used */
    Fixed volume;               /* playback nominal volume from 0 to Unity
                                 * (full volume). Map this value into
                                 * the output hardware's dynamic range.
                                 */
};

/*****************************************************************************/

/* AIFFCommon structure - public (AIFF format)
 *
 * This structure represents the IFF COMM (Common) chunk for AIFF format.
 * It contains fundamental parameters of the sampled sound.
 *
 * Note: The structure must match the IFF COMM chunk layout exactly to allow
 * direct reading from IFF files. Field order and types are critical for
 * correct parsing.
 */
struct AIFFCommon {
    UWORD numChannels;          /* number of audio channels */
    ULONG numSampleFrames;      /* number of sample frames */
    UWORD sampleSize;           /* number of bits per sample */
    UBYTE sampleRate[10];      /* 80-bit IEEE 754 extended floating point */
};

/*****************************************************************************/

/* MAUDHeader structure - public (MAUD format)
 *
 * This structure represents the MAUD header chunk for MAUD format.
 * It contains fundamental parameters of the sampled sound.
 */
struct MAUDHeader {
    UWORD numChannels;          /* number of audio channels */
    UWORD sampleSize;           /* number of bits per sample */
    ULONG sampleRate;           /* sample rate in Hz */
    ULONG numSampleFrames;      /* number of sample frames */
    ULONG compression;         /* compression type (0=none) */
};

/*****************************************************************************/

/* IFF Form Type IDs
 *
 * These constants identify the different IFF audio format variants.
 * ID_FORM is defined in iffparse.h and is the container for all these types.
 *
 * ID_8SVX - 8-bit Sampled Voice: Standard Amiga 8-bit audio format with
 *           optional Fibonacci-delta compression. Supports multiple octaves
 *           for musical instruments and envelopes for amplitude modulation.
 *
 * ID_16SV - 16-bit Sampled Voice: Same format as 8SVX but with 16-bit samples
 *           in the BODY chunk. Uses the same VHDR chunk structure and metadata
 *           chunks. Currently supports uncompressed data only.
 *
 * ID_AIFF - Audio Interchange File Format: Mac/Apple audio format supporting
 *           1-32 bit samples, multiple channels, and various compression types.
 *
 * ID_MAUD - Amiga audio format: Native Amiga audio format supporting various
 *           bit depths and compression methods.
 */
#define ID_8SVX    MAKE_ID('8','S','V','X')  /* 8-bit Sampled Voice */
#define ID_16SV    MAKE_ID('1','6','S','V')  /* 16-bit Sampled Voice */
#define ID_AIFF    MAKE_ID('A','I','F','F')  /* Audio Interchange File Format */
#define ID_AIFC    MAKE_ID('A','I','F','C')  /* Audio Interchange File Format (compressed) */
#define ID_MAUD    MAKE_ID('M','A','U','D')  /* Amiga audio format */

/* 8SVX CHAN chunk values */
#define CHAN_RIGHT  4L  /* Right channel (Amiga channels 1 and 2) */
#define CHAN_LEFT   2L  /* Left channel (Amiga channels 0 and 3) */
#define CHAN_STEREO 6L  /* Stereo pair (requires both left and right channels) */

/*****************************************************************************/

/* Error codes
 *
 * These constants represent error conditions that can occur during IFFSound
 * operations. Functions return 0 for success or one of these negative values
 * for errors. Use IFFSoundErrorString() to get a human-readable error message.
 *
 * IFFSOUND_OK          - Operation completed successfully
 * IFFSOUND_ERROR        - General error (check IFFSoundErrorString() for details)
 * IFFSOUND_NOMEM        - Memory allocation failed
 * IFFSOUND_BADFILE      - File I/O error or invalid IFF file structure
 * IFFSOUND_UNSUPPORTED  - Audio format or feature not supported
 * IFFSOUND_INVALID      - Invalid operation or uninitialized structure
 */
#define IFFSOUND_OK           0
#define IFFSOUND_ERROR       -1
#define IFFSOUND_NOMEM       -2
#define IFFSOUND_BADFILE     -3
#define IFFSOUND_UNSUPPORTED -4
#define IFFSOUND_INVALID     -5

/*****************************************************************************/

#endif /* IFFSOUND_H */

