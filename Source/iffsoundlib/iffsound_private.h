/*
** iffsound_private.h - IFFSound Library Private Definitions
**
** Internal structures and functions used by the library implementation
** This header is only included by library source files, not by clients
*/

#ifndef IFFSOUND_PRIVATE_H
#define IFFSOUND_PRIVATE_H

#include "iffsound.h"
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <dos/dos.h>

/* IFF Chunk IDs */
#define ID_VHDR    0x56484452UL  /* 'VHDR' - Voice Header (8SVX) */
#define ID_COMM    0x434F4D4DUL  /* 'COMM' - Common (AIFF) */
#define ID_SSND    0x53534E44UL  /* 'SSND' - Sound data (AIFF) */
#define ID_BODY    0x424F4459UL  /* 'BODY' - Body data (8SVX, MAUD) */
#define ID_NAME    0x4E414D45UL  /* 'NAME' - Name text */
#define ID_ATAK    0x4154414BUL  /* 'ATAK' - Attack envelope (8SVX) */
#define ID_RLSE    0x524C5345UL  /* 'RLSE' - Release envelope (8SVX) */
#define ID_COPYRIGHT 0x28632920UL  /* '(c) ' - copyright text */
#define ID_AUTH    0x41555448UL  /* 'AUTH' - author text */
#define ID_ANNO    0x414E4E4FUL  /* 'ANNO' - annotation text */
/* ID_MAUD is defined in iffsound.h */
#define ID_FVER    0x46564552UL  /* 'FVER' - Format version (AIFC) */
#define ID_MARK    0x4D41524BUL  /* 'MARK' - Markers (AIFF) */
#define ID_INST    0x494E5354UL  /* 'INST' - Instrument (AIFF) */
#define ID_COMT    0x434F4D54UL  /* 'COMT' - Comments with timestamps (AIFF) */
#define ID_CHAN    0x4348414EUL  /* 'CHAN' - Channel assignment (8SVX) */
#define ID_PAN     0x50414E20UL  /* 'PAN ' - Panning (8SVX) */
#define ID_SEQN    0x5345514EUL  /* 'SEQN' - Sequence/loops (8SVX) */
#define ID_FADE    0x46414445UL  /* 'FADE' - Fade information (8SVX) */

/* Compression types for 8SVX */
#define sCmpNone       0
#define sCmpFibDelta   1

/* AIFF/AIFC compression/encoding format IDs */
#define AIFF_COMPRESSION_NONE    0x4E4F4E45UL  /* 'NONE' - Linear PCM (big-endian) */
#define AIFF_COMPRESSION_LPCM    0x6C70636DUL  /* 'lpcm' - Linear PCM (non-standard) */
#define AIFF_COMPRESSION_TWOS    0x74776F73UL  /* 'twos' - Two's complement PCM (big-endian) */
#define AIFF_COMPRESSION_SOWT    0x736F7774UL  /* 'sowt' - Two's complement PCM (little-endian) */
#define AIFF_COMPRESSION_ULAW    0x554C4157UL  /* 'ULAW' - G.711 mu-Law */
#define AIFF_COMPRESSION_ulaw    0x756C6177UL  /* 'ulaw' - G.711 mu-Law (lowercase) */
#define AIFF_COMPRESSION_ALAW    0x414C4157UL  /* 'ALAW' - G.711 A-Law */
#define AIFF_COMPRESSION_alaw    0x616C6177UL  /* 'alaw' - G.711 A-Law (lowercase) */
#define AIFF_COMPRESSION_FL32    0x464C3332UL  /* 'FL32' - 32-bit floating point */
#define AIFF_COMPRESSION_fl32    0x666C3332UL  /* 'fl32' - 32-bit floating point (lowercase) */
#define AIFF_COMPRESSION_ADP4    0x41445034UL  /* 'ADP4' - 4-bit ADPCM */

/* IFFSoundMeta structure - metadata storage, allocated on demand */
struct IFFSoundMeta {
    /* Standard metadata storage - library owns all memory */
    STRPTR name;                        /* NAME chunk */
    ULONG nameSize;                    /* Size of name string (including null) */
    STRPTR copyright;                   /* Copyright chunk */
    ULONG copyrightSize;               /* Size of copyright string (including null) */
    STRPTR author;                      /* AUTH chunk */
    ULONG authorSize;                   /* Size of author string (including null) */
    STRPTR annotation;                  /* ANNO chunk (first instance) */
    ULONG annotationCount;             /* Number of ANNO chunks */
    STRPTR *annotationArray;            /* Array of all ANNO strings */
    ULONG *annotationSizes;            /* Array of sizes for each ANNO string */
    struct EGPoint *atak;                /* ATAK chunk (attack envelope) */
    ULONG atakCount;                    /* Number of attack envelope points */
    struct EGPoint *rlse;               /* RLSE chunk (release envelope) */
    ULONG rlseCount;                    /* Number of release envelope points */
    
    /* AIFF/AIFC specific metadata */
    ULONG fver;                         /* FVER chunk (format version timestamp for AIFC) */
    STRPTR fverString;                  /* FVER chunk (version string for other formats) */
    ULONG fverStringSize;               /* Size of FVER version string */
    ULONG markerCount;                  /* Number of MARK chunks */
    struct AIFFMarker *markers;         /* Array of markers */
    ULONG *markerNameSizes;             /* Array of sizes for each marker name */
    struct AIFFInstrument *instrument;  /* INST chunk (instrument data) */
    ULONG commentCount;                 /* Number of COMT chunks */
    struct AIFFComment *comments;       /* Array of comments with timestamps */
    ULONG *commentTextSizes;            /* Array of sizes for each comment text */
    
    /* 8SVX extended metadata */
    ULONG chan;                         /* CHAN chunk (channel assignment: RIGHT=4, LEFT=2, STEREO=6) */
    Fixed pan;                          /* PAN chunk (panning position: 0 to Unity) */
    ULONG seqnCount;                    /* Number of SEQN loop pairs */
    struct LoopPair *seqnLoops;         /* Array of loop pairs (start, end) */
    ULONG fade;                         /* FADE chunk (loop number to start fading) */
};

/* Complete IFFSound structure - private implementation */
struct IFFSound {
    /* Public members */
    struct Voice8Header *vhdr;         /* 8SVX voice header */
    struct AIFFCommon *comm;           /* AIFF common chunk */
    struct MAUDHeader *maud;           /* MAUD header */
    ULONG formtype;                    /* IFF FORM type (ID_8SVX, ID_16SV, ID_AIFF, ID_AIFC, ID_MAUD) */
    
    /* Decoded audio data */
    UBYTE *sampleData;
    ULONG sampleDataSize;
    
    /* Format analysis */
    BOOL isCompressed;
    ULONG aiffCompression;              /* AIFC compression format ID (NONE, ULAW, ALAW, FL32, etc.) */
    BOOL isLittleEndian;                /* TRUE for sowt (little-endian PCM) */
    
    /* Private members - internal to library */
    struct IFFHandle *iff;
    BPTR filehandle;
    LONG lastError;
    char errorString[256];
    
    /* Internal state */
    BOOL isLoaded;
    BOOL isDecoded;
    ULONG bodyChunkSize;
    ULONG bodyChunkPosition;
    
    /* Metadata storage - allocated on demand */
    struct IFFSoundMeta *metadata;      /* Metadata structure, NULL if no metadata */
};

/* Internal function prototypes - declared in sound_decoder.c */
LONG Decode8SVX(struct IFFSound *sound);
LONG Decode16SV(struct IFFSound *sound);
LONG DecodeAIFF(struct IFFSound *sound);
LONG DecodeMAUD(struct IFFSound *sound);
LONG AnalyzeFormat(struct IFFSound *sound);
VOID SetIFFSoundError(struct IFFSound *sound, LONG error, const char *message);
VOID ReadAllMeta(struct IFFSound *sound);

/* AIFF decompression functions - declared in aiff_decompressor.c */
LONG DecodeULAW(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize);
LONG DecodeALAW(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize);
LONG DecodeFL32(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize);
LONG DecodePCM(struct IFFSound *sound, UBYTE *input, ULONG inputSize, UBYTE *output, ULONG outputSize, ULONG bitDepth, ULONG channels, BOOL littleEndian);
double IEEE754ReadExtended(UBYTE *bytes);

#endif /* IFFSOUND_PRIVATE_H */

