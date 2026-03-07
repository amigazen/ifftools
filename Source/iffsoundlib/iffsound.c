/*
** iffsound.c - IFFSound Library Implementation
**
** Amiga-style function library for loading and decoding IFF audio files
*/

#include "iffsound_private.h"
#include "/debug.h"
#include <stdio.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/utility.h>
#include <math.h>

/* Library base is defined in main.c */
extern struct Library *IFFParseBase;

/* Forward declarations for internal functions */
LONG ReadVHDR(struct IFFSound *sound);
LONG ReadCOMM(struct IFFSound *sound);
LONG ReadMAUD(struct IFFSound *sound);
LONG ReadBODY(struct IFFSound *sound);

/*
** AllocIFFSound - Allocate a new IFFSound object
** Returns: Pointer to new object or NULL on failure
** Follows iffparse.library pattern: AllocIFF
*/
struct IFFSound *AllocIFFSound(VOID)
{
    struct IFFSound *sound;
    
    /* Allocate memory for sound structure - use public memory (not chip RAM) */
    sound = (struct IFFSound *)AllocMem(sizeof(struct IFFSound), MEMF_PUBLIC | MEMF_CLEAR);
    if (!sound) {
        return NULL;
    }
    
    /* Initialize structure */
    sound->vhdr = NULL;
    sound->comm = NULL;
    sound->maud = NULL;
    sound->formtype = 0;
    sound->sampleData = NULL;
    sound->sampleDataSize = 0;
    sound->isCompressed = FALSE;
    sound->iff = NULL;
    sound->lastError = IFFSOUND_OK;
    sound->errorString[0] = '\0';
    sound->isLoaded = FALSE;
    sound->isDecoded = FALSE;
    sound->bodyChunkSize = 0;
    sound->bodyChunkPosition = 0;
    sound->metadata = NULL;
    
    return sound;
}

/*
** FreeIFFSoundMeta - Free metadata structure and all its contents (internal helper)
*/
static VOID FreeIFFSoundMeta(struct IFFSoundMeta *meta)
{
    ULONG i;
    
    if (!meta) {
        return;
    }
    
    /* Free standard metadata */
    if (meta->name) {
        FreeMem(meta->name, meta->nameSize);
    }
    if (meta->copyright) {
        FreeMem(meta->copyright, meta->copyrightSize);
    }
    if (meta->author) {
        FreeMem(meta->author, meta->authorSize);
    }
    if (meta->annotationArray) {
        for (i = 0; i < meta->annotationCount; i++) {
            if (meta->annotationArray[i] && meta->annotationSizes) {
                FreeMem(meta->annotationArray[i], meta->annotationSizes[i]);
            }
        }
        FreeMem(meta->annotationArray, meta->annotationCount * sizeof(STRPTR));
    }
    if (meta->annotationSizes) {
        FreeMem(meta->annotationSizes, meta->annotationCount * sizeof(ULONG));
    }
    if (meta->atak) {
        FreeMem(meta->atak, meta->atakCount * sizeof(struct EGPoint));
    }
    if (meta->rlse) {
        FreeMem(meta->rlse, meta->rlseCount * sizeof(struct EGPoint));
    }
    
    /* Free markers array */
    if (meta->markers) {
        for (i = 0; i < meta->markerCount; i++) {
            if (meta->markers[i].name && meta->markerNameSizes) {
                FreeMem(meta->markers[i].name, meta->markerNameSizes[i]);
            }
        }
        FreeMem(meta->markers, meta->markerCount * sizeof(struct AIFFMarker));
    }
    if (meta->markerNameSizes) {
        FreeMem(meta->markerNameSizes, meta->markerCount * sizeof(ULONG));
    }
    
    /* Free instrument */
    if (meta->instrument) {
        FreeMem(meta->instrument, sizeof(struct AIFFInstrument));
    }
    
    /* Free comments array */
    if (meta->comments) {
        for (i = 0; i < meta->commentCount; i++) {
            if (meta->comments[i].text && meta->commentTextSizes) {
                FreeMem(meta->comments[i].text, meta->commentTextSizes[i]);
            }
        }
        FreeMem(meta->comments, meta->commentCount * sizeof(struct AIFFComment));
    }
    if (meta->commentTextSizes) {
        FreeMem(meta->commentTextSizes, meta->commentCount * sizeof(ULONG));
    }
    
    /* Free SEQN loops array */
    if (meta->seqnLoops) {
        FreeMem(meta->seqnLoops, meta->seqnCount * sizeof(struct LoopPair));
    }
    
    /* Free FVER version string */
    if (meta->fverString) {
        FreeMem(meta->fverString, meta->fverStringSize);
    }
    
    /* Free the metadata structure itself */
    FreeMem(meta, sizeof(struct IFFSoundMeta));
}

/*
** FreeIFFSound - Free an IFFSound object
** Frees all allocated memory and closes any open files
** Follows iffparse.library pattern: FreeIFF
*/
VOID FreeIFFSound(struct IFFSound *sound)
{
    if (!sound) {
        return;
    }
    
    /* Close IFF handle if open */
    if (sound->iff) {
        CloseIFFSound(sound);
    }
    
    /* Note: File handle management is the caller's responsibility, following
     * iffparse.library pattern. The caller must close the file handle with Close()
     * after calling CloseIFFSound(). */
    
    /* Free voice header */
    if (sound->vhdr) {
        FreeMem(sound->vhdr, sizeof(struct Voice8Header));
        sound->vhdr = NULL;
    }
    
    /* Free AIFF common chunk */
    if (sound->comm) {
        FreeMem(sound->comm, sizeof(struct AIFFCommon));
        sound->comm = NULL;
    }
    
    /* Free MAUD header */
    if (sound->maud) {
        FreeMem(sound->maud, sizeof(struct MAUDHeader));
        sound->maud = NULL;
    }
    
    /* Free sample data */
    if (sound->sampleData) {
        FreeMem(sound->sampleData, sound->sampleDataSize);
        sound->sampleData = NULL;
        sound->sampleDataSize = 0;
    }
    
    /* Free metadata structure if allocated */
    if (sound->metadata) {
        FreeIFFSoundMeta(sound->metadata);
        sound->metadata = NULL;
    }
    
    /* Free sound structure */
    FreeMem(sound, sizeof(struct IFFSound));
}

/*
** SetIFFSoundError - Set error code and message (internal)
*/
VOID SetIFFSoundError(struct IFFSound *sound, LONG error, const char *message)
{
    if (!sound) {
        return;
    }
    
    sound->lastError = error;
    if (message) {
        Strncpy(sound->errorString, message, sizeof(sound->errorString) - 1);
        sound->errorString[sizeof(sound->errorString) - 1] = '\0';
    } else {
        sound->errorString[0] = '\0';
    }
}

/*
** InitIFFSoundasDOS - Initialize IFFSound as DOS stream
** Follows iffparse.library pattern: InitIFFasDOS
** 
** Initializes the IFFSound to operate on DOS streams.
** The iff_Stream field must be set by the caller after
** calling Open() to get a BPTR file handle.
** 
** Example usage:
**   sound = AllocIFFSound();
**   filehandle = Open("file.iff", MODE_OLDFILE);
**   InitIFFSoundasDOS(sound);
**   sound->iff->iff_Stream = (ULONG)filehandle;
**   OpenIFFSound(sound, IFFF_READ);
*/
VOID InitIFFSoundasDOS(struct IFFSound *sound)
{
    struct IFFHandle *iff;
    
    if (!sound) {
        return;
    }
    
    /* Allocate IFF handle */
    iff = AllocIFF();
    if (!iff) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Cannot allocate IFF handle");
        return;
    }
    
    sound->iff = iff;
    
    /* Initialize IFF as DOS stream */
    /* Note: iff_Stream must be set by caller after calling Open() */
    InitIFFasDOS(iff);
}

/*
** OpenIFFSound - Prepare IFFSound to read or write a new IFF stream
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: OpenIFF
** 
** The IFFSound must have been initialized with InitIFFSoundasDOS()
** and iff_Stream must be set to a valid BPTR file handle.
** 
** rwMode: IFFF_READ or IFFF_WRITE
*/
LONG OpenIFFSound(struct IFFSound *sound, LONG rwMode)
{
    LONG error;
    
    if (!sound) {
        return RETURN_FAIL;
    }
    
    /* Ensure IFF handle is allocated and initialized */
    if (!sound->iff) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "IFFSound not initialized - call InitIFFSoundasDOS() first");
        return RETURN_FAIL;
    }
    
    /* Ensure stream is set */
    if (!sound->iff->iff_Stream) {
        SetIFFSoundError(sound, IFFSOUND_INVALID, "IFF stream not set - set iff_Stream to file handle after Open()");
        return RETURN_FAIL;
    }
    
    /* Open IFF for reading or writing */
    error = OpenIFF(sound->iff, rwMode);
    if (error) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Cannot open IFF stream");
        return RETURN_FAIL;
    }
    
    sound->isLoaded = TRUE;
    return RETURN_OK;
}

/*
** CloseIFFSound - Close IFFSound and free IFF handle
** Follows iffparse.library pattern: CloseIFF
*/
VOID CloseIFFSound(struct IFFSound *sound)
{
    if (!sound) {
        return;
    }
    
    /* Close IFF handle if open */
    if (sound->iff) {
        CloseIFF(sound->iff);
        FreeIFF(sound->iff);
        sound->iff = NULL;
    }
}

/*
** ParseIFFSound - Parse IFF structure and read chunks
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: ParseIFF
*/
LONG ParseIFFSound(struct IFFSound *sound)
{
    LONG error;
    struct ContextNode *cn;
    ULONG formType;
    
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Sound not opened");
        }
        return RETURN_FAIL;
    }
    
    /* First, parse one step to get FORM type */
    error = ParseIFF(sound->iff, IFFPARSE_STEP);
    if (error != 0) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to parse FORM chunk");
        return RETURN_FAIL;
    }
    
    cn = CurrentChunk(sound->iff);
    if (!cn || cn->cn_ID != ID_FORM) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Not a valid IFF FORM file");
        return RETURN_FAIL;
    }
    
    formType = cn->cn_Type;
    sound->formtype = formType;
    
    DEBUG_PRINTF1("DEBUG: ParseIFFSound - FORM type = 0x%08lx\n", formType);
    
    /* Set up property chunks based on form type */
    if (formType == ID_8SVX || formType == ID_16SV) {
        /* 8SVX and 16SV use VHDR and BODY chunks */
        if ((error = PropChunk(sound->iff, formType, ID_VHDR)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set PropChunk for VHDR");
            return RETURN_FAIL;
        }
        PropChunk(sound->iff, formType, ID_NAME);
        PropChunk(sound->iff, formType, ID_COPYRIGHT);
        PropChunk(sound->iff, formType, ID_AUTH);
        CollectionChunk(sound->iff, formType, ID_ANNO);
        PropChunk(sound->iff, formType, ID_ATAK);
        PropChunk(sound->iff, formType, ID_RLSE);
        PropChunk(sound->iff, formType, ID_CHAN);  /* Channel assignment */
        PropChunk(sound->iff, formType, ID_PAN);   /* Panning */
        PropChunk(sound->iff, formType, ID_SEQN);  /* Sequence/loops */
        PropChunk(sound->iff, formType, ID_FADE);  /* Fade information */
        PropChunk(sound->iff, formType, ID_FVER);  /* Version string (general IFF) */
        if ((error = StopChunk(sound->iff, formType, ID_BODY)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set StopChunk for BODY");
            return RETURN_FAIL;
        }
    } else if (formType == ID_AIFF || formType == ID_AIFC) {
        /* AIFF uses COMM and SSND chunks */
        if ((error = PropChunk(sound->iff, formType, ID_COMM)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set PropChunk for COMM");
            return RETURN_FAIL;
        }
        PropChunk(sound->iff, formType, ID_FVER);  /* Format version (AIFC) or version string (AIFF) */
        PropChunk(sound->iff, formType, ID_NAME);
        PropChunk(sound->iff, formType, ID_COPYRIGHT);
        PropChunk(sound->iff, formType, ID_AUTH);
        CollectionChunk(sound->iff, formType, ID_ANNO);
        CollectionChunk(sound->iff, formType, ID_MARK);  /* Markers */
        PropChunk(sound->iff, formType, ID_INST);  /* Instrument */
        CollectionChunk(sound->iff, formType, ID_COMT);  /* Comments with timestamps */
        if ((error = StopChunk(sound->iff, formType, ID_SSND)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set StopChunk for SSND");
            return RETURN_FAIL;
        }
    } else if (formType == ID_MAUD) {
        /* MAUD uses MAUD header and BODY chunks */
        if ((error = PropChunk(sound->iff, formType, ID_MAUD)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set PropChunk for MAUD");
            return RETURN_FAIL;
        }
        PropChunk(sound->iff, formType, ID_NAME);
        PropChunk(sound->iff, formType, ID_COPYRIGHT);
        PropChunk(sound->iff, formType, ID_AUTH);
        CollectionChunk(sound->iff, formType, ID_ANNO);
        if ((error = StopChunk(sound->iff, formType, ID_BODY)) != 0) {
            SetIFFSoundError(sound, IFFSOUND_ERROR, "Failed to set StopChunk for BODY");
            return RETURN_FAIL;
        }
    } else {
        SetIFFSoundError(sound, IFFSOUND_UNSUPPORTED, "Unsupported IFF FORM type");
        return RETURN_FAIL;
    }
    
    /* Parse the file until we hit the data chunk */
    error = ParseIFF(sound->iff, IFFPARSE_SCAN);
    if (error != 0 && error != IFFERR_EOC) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Failed to parse IFF file");
        return RETURN_FAIL;
    }
    
    /* Extract stored property chunks based on format */
    if (formType == ID_8SVX || formType == ID_16SV) {
        if (ReadVHDR(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
        ReadAllMeta(sound);
        if (ReadBODY(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
    } else if (formType == ID_AIFF || formType == ID_AIFC) {
        if (ReadCOMM(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
        ReadAllMeta(sound);
        if (ReadBODY(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
    } else if (formType == ID_MAUD) {
        if (ReadMAUD(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
        ReadAllMeta(sound);
        if (ReadBODY(sound) != RETURN_OK) {
            return RETURN_FAIL; /* Error already set */
        }
    }
    
    sound->isLoaded = TRUE;
    return RETURN_OK;
}

/*
** Getter functions - return values from sound structure
** Following iffparse.library pattern: Get* functions
*/
struct IFFHandle *GetIFFHandle(struct IFFSound *sound)
{
    if (!sound) {
        return NULL;
    }
    return sound->iff;
}

ULONG GetSampleRate(struct IFFSound *sound)
{
    double sampleRateFloat;
    
    if (!sound) {
        return 0;
    }
    if (sound->vhdr) {
        return sound->vhdr->samplesPerSec;
    }
    if (sound->comm) {
        /* Extract sample rate from 80-bit IEEE 754 extended float */
        sampleRateFloat = IEEE754ReadExtended(sound->comm->sampleRate);
        if (sampleRateFloat < 0.0) {
            sampleRateFloat = 0.0;
        }
        return (ULONG)sampleRateFloat;
    }
    if (sound->maud) {
        return sound->maud->sampleRate;
    }
    return 0;
}

ULONG GetChannels(struct IFFSound *sound)
{
    if (!sound) {
        return 0;
    }
    if (sound->comm) {
        return sound->comm->numChannels;
    }
    if (sound->maud) {
        return sound->maud->numChannels;
    }
    /* 8SVX is always mono */
    if (sound->vhdr) {
        return 1;
    }
    return 0;
}

ULONG GetBitDepth(struct IFFSound *sound)
{
    if (!sound) {
        return 0;
    }
    if (sound->comm) {
        return sound->comm->sampleSize;
    }
    if (sound->maud) {
        return sound->maud->sampleSize;
    }
    /* 8SVX is 8-bit, 16SV is 16-bit */
    if (sound->vhdr) {
        if (sound->formtype == ID_16SV) {
            return 16;
        }
        return 8;
    }
    return 0;
}

ULONG GetSampleCount(struct IFFSound *sound)
{
    if (!sound) {
        return 0;
    }
    if (sound->comm) {
        return sound->comm->numSampleFrames;
    }
    if (sound->maud) {
        return sound->maud->numSampleFrames;
    }
    if (sound->vhdr) {
        return sound->vhdr->oneShotHiSamples + sound->vhdr->repeatHiSamples;
    }
    return 0;
}

ULONG GetFormType(struct IFFSound *sound)
{
    if (!sound) {
        return 0;
    }
    return sound->formtype;
}

struct Voice8Header *GetVHDR(struct IFFSound *sound)
{
    if (!sound) {
        return NULL;
    }
    return sound->vhdr;
}

struct AIFFCommon *GetCOMM(struct IFFSound *sound)
{
    if (!sound) {
        return NULL;
    }
    return sound->comm;
}

struct MAUDHeader *GetMAUD(struct IFFSound *sound)
{
    if (!sound) {
        return NULL;
    }
    return sound->maud;
}

UBYTE *GetSampleData(struct IFFSound *sound)
{
    if (!sound) {
        return NULL;
    }
    return sound->sampleData;
}

ULONG GetSampleDataSize(struct IFFSound *sound)
{
    if (!sound) {
        return 0;
    }
    return sound->sampleDataSize;
}

BOOL IsCompressed(struct IFFSound *sound)
{
    if (!sound) {
        return FALSE;
    }
    return sound->isCompressed;
}

/*
** GetSoundInfo - Get all core audio properties in a single structure
** Returns: Pointer to IFFSoundInfo structure, or NULL if sound is invalid
** The structure is allocated statically and remains valid until the next
** call to GetSoundInfo() or until the IFFSound is freed.
*/
struct IFFSoundInfo *GetSoundInfo(struct IFFSound *sound)
{
    static struct IFFSoundInfo info;
    
    if (!sound) {
        return NULL;
    }
    
    /* Populate structure with all core properties */
    info.sampleRate = GetSampleRate(sound);
    info.channels = GetChannels(sound);
    info.bitDepth = GetBitDepth(sound);
    info.sampleCount = GetSampleCount(sound);
    info.formType = GetFormType(sound);
    info.compressedSize = sound->bodyChunkSize;      /* Compressed data size (BODY chunk) */
    info.decodedSize = GetSampleDataSize(sound);     /* Decoded sample data size */
    info.duration = GetDuration(sound);              /* Duration in milliseconds */
    info.bitRate = GetBitRate(sound);                /* Bit rate in bps */
    info.byteRate = GetByteRate(sound);              /* Byte rate in bytes/sec */
    info.isCompressed = IsCompressed(sound);
    info.isLoaded = sound->isLoaded;
    info.isDecoded = sound->isDecoded;
    
    return &info;
}

/*
** GetDuration - Get duration in milliseconds
** Returns: Duration in milliseconds, or 0 if not loaded or sample rate is 0
*/
ULONG GetDuration(struct IFFSound *sound)
{
    ULONG sampleRate;
    ULONG sampleCount;
    
    if (!sound) {
        return 0;
    }
    
    sampleRate = GetSampleRate(sound);
    sampleCount = GetSampleCount(sound);
    
    if (sampleRate == 0) {
        return 0;
    }
    
    /* Calculate duration: (sampleCount / sampleRate) * 1000 */
    return (sampleCount * 1000UL) / sampleRate;
}

/*
** GetDurationSeconds - Get duration in seconds (as double)
** Returns: Duration in seconds, or 0.0 if not loaded or sample rate is 0
*/
double GetDurationSeconds(struct IFFSound *sound)
{
    ULONG sampleRate;
    ULONG sampleCount;
    
    if (!sound) {
        return 0.0;
    }
    
    sampleRate = GetSampleRate(sound);
    sampleCount = GetSampleCount(sound);
    
    if (sampleRate == 0) {
        return 0.0;
    }
    
    /* Calculate duration: sampleCount / sampleRate */
    return (double)sampleCount / (double)sampleRate;
}

/*
** GetBitRate - Get bit rate in bits per second
** Returns: Bit rate in bps, or 0 if not loaded
*/
ULONG GetBitRate(struct IFFSound *sound)
{
    ULONG sampleRate;
    ULONG bitDepth;
    ULONG channels;
    
    if (!sound) {
        return 0;
    }
    
    sampleRate = GetSampleRate(sound);
    bitDepth = GetBitDepth(sound);
    channels = GetChannels(sound);
    
    if (sampleRate == 0 || bitDepth == 0 || channels == 0) {
        return 0;
    }
    
    /* Bit rate = sample rate * bit depth * channels */
    return sampleRate * bitDepth * channels;
}

/*
** GetByteRate - Get byte rate in bytes per second
** Returns: Byte rate in bytes/sec, or 0 if not loaded
*/
ULONG GetByteRate(struct IFFSound *sound)
{
    ULONG sampleRate;
    ULONG bitDepth;
    ULONG channels;
    
    if (!sound) {
        return 0;
    }
    
    sampleRate = GetSampleRate(sound);
    bitDepth = GetBitDepth(sound);
    channels = GetChannels(sound);
    
    if (sampleRate == 0 || bitDepth == 0 || channels == 0) {
        return 0;
    }
    
    /* Byte rate = sample rate * (bit depth / 8) * channels */
    return (sampleRate * bitDepth * channels) / 8UL;
}

/*
** GetLength - Get length in sample frames
** Returns: Number of sample frames, or 0 if not loaded
** This is an alias for GetSampleCount() with a more descriptive name
*/
ULONG GetLength(struct IFFSound *sound)
{
    return GetSampleCount(sound);
}

/*
** ReadVHDR - Read VHDR chunk (8SVX)
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: FindProp
*/
LONG ReadVHDR(struct IFFSound *sound)
{
    struct StoredProperty *sp;
    struct Voice8Header *vhdr;
    
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or IFF handle");
        }
        return RETURN_FAIL;
    }
    
    /* Find stored VHDR property */
    sp = FindProp(sound->iff, sound->formtype, ID_VHDR);
    if (!sp) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "VHDR chunk not found");
        return RETURN_FAIL;
    }
    
    DEBUG_PRINTF1("DEBUG: ReadVHDR - Found VHDR property, size=%ld\n", sp->sp_Size);
    
    /* Check size - VHDR should be 20 bytes */
    if (sp->sp_Size < 20) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "VHDR chunk too small");
        return RETURN_FAIL;
    }
    
    /* Allocate VHDR structure - use public memory (not chip RAM) */
    vhdr = (struct Voice8Header *)AllocMem(sizeof(struct Voice8Header), MEMF_PUBLIC | MEMF_CLEAR);
    if (!vhdr) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate Voice8Header");
        return RETURN_FAIL;
    }
    
    /* Read fields individually from byte array to avoid structure alignment issues */
    /* IFF data is big-endian, Amiga is big-endian, so we can read directly */
    {
        UBYTE *src = (UBYTE *)sp->sp_Data;
        
        /* Read ULONG fields (big-endian, 4 bytes each) */
        vhdr->oneShotHiSamples = (ULONG)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
        vhdr->repeatHiSamples = (ULONG)((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
        vhdr->samplesPerHiCycle = (ULONG)((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
        
        /* Read UWORD field (big-endian, 2 bytes) */
        vhdr->samplesPerSec = (UWORD)((src[12] << 8) | src[13]);
        
        /* Read UBYTE fields (1 byte each) */
        vhdr->ctOctave = src[14];
        vhdr->sCompression = src[15];
        
        /* Read Fixed field (big-endian, 4 bytes) */
        vhdr->volume = (Fixed)((src[16] << 24) | (src[17] << 16) | (src[18] << 8) | src[19]);
    }
    
    sound->vhdr = vhdr;
    sound->isCompressed = (vhdr->sCompression != sCmpNone);
    
    return RETURN_OK;
}

/*
** ReadCOMM - Read COMM chunk (AIFF)
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: FindProp
*/
LONG ReadCOMM(struct IFFSound *sound)
{
    struct StoredProperty *sp;
    struct AIFFCommon *comm;
    
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or IFF handle");
        }
        return RETURN_FAIL;
    }
    
    /* Find stored COMM property */
    sp = FindProp(sound->iff, sound->formtype, ID_COMM);
    if (!sp) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "COMM chunk not found");
        return RETURN_FAIL;
    }
    
    DEBUG_PRINTF1("DEBUG: ReadCOMM - Found COMM property, size=%ld\n", sp->sp_Size);
    
    /* Check size - COMM should be at least 18 bytes */
    if (sp->sp_Size < 18) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "COMM chunk too small");
        return RETURN_FAIL;
    }
    
    /* Allocate COMM structure - use public memory (not chip RAM) */
    comm = (struct AIFFCommon *)AllocMem(sizeof(struct AIFFCommon), MEMF_PUBLIC | MEMF_CLEAR);
    if (!comm) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate AIFFCommon");
        return RETURN_FAIL;
    }
    
    /* Read fields individually from byte array */
    {
        UBYTE *src = (UBYTE *)sp->sp_Data;
        ULONG pos;
        
        /* Read UWORD field (big-endian, 2 bytes) */
        comm->numChannels = (UWORD)((src[0] << 8) | src[1]);
        
        /* Read ULONG field (big-endian, 4 bytes) */
        comm->numSampleFrames = (ULONG)((src[2] << 24) | (src[3] << 16) | (src[4] << 8) | src[5]);
        
        /* Read UWORD field (big-endian, 2 bytes) */
        comm->sampleSize = (UWORD)((src[6] << 8) | src[7]);
        
        /* Read 80-bit IEEE 754 extended float (10 bytes) */
        pos = 8;
        {
            ULONG i;
            for (i = 0; i < 10 && (pos + i) < sp->sp_Size; i++) {
                comm->sampleRate[i] = src[pos + i];
            }
        }
        
        /* For AIFC, read compression format after the 80-bit float */
        if (sound->formtype == ID_AIFC) {
            pos = 18; /* After standard COMM fields */
            if (pos + 4 <= sp->sp_Size) {
                /* Read compression format ID (4 bytes) */
                sound->aiffCompression = (ULONG)((src[pos] << 24) | (src[pos + 1] << 16) |
                                                  (src[pos + 2] << 8) | src[pos + 3]);
                
                /* Check for little-endian PCM */
                if (sound->aiffCompression == AIFF_COMPRESSION_SOWT) {
                    sound->isLittleEndian = TRUE;
                    sound->aiffCompression = AIFF_COMPRESSION_NONE; /* Treat as PCM */
                } else {
                    sound->isLittleEndian = FALSE;
                }
                
                /* Mark as compressed if not NONE/lpcm/twos */
                if (sound->aiffCompression != AIFF_COMPRESSION_NONE &&
                    sound->aiffCompression != AIFF_COMPRESSION_LPCM &&
                    sound->aiffCompression != AIFF_COMPRESSION_TWOS) {
                    sound->isCompressed = TRUE;
                }
            }
        } else {
            /* AIFF always uses uncompressed PCM */
            sound->aiffCompression = AIFF_COMPRESSION_NONE;
            sound->isLittleEndian = FALSE;
        }
    }
    
    sound->comm = comm;
    
    return RETURN_OK;
}

/*
** ReadMAUD - Read MAUD header chunk
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: FindProp
*/
LONG ReadMAUD(struct IFFSound *sound)
{
    struct StoredProperty *sp;
    struct MAUDHeader *maud;
    
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or IFF handle");
        }
        return RETURN_FAIL;
    }
    
    /* Find stored MAUD property */
    sp = FindProp(sound->iff, sound->formtype, ID_MAUD);
    if (!sp) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "MAUD chunk not found");
        return RETURN_FAIL;
    }
    
    DEBUG_PRINTF1("DEBUG: ReadMAUD - Found MAUD property, size=%ld\n", sp->sp_Size);
    
    /* Check size - MAUD should be at least 16 bytes */
    if (sp->sp_Size < 16) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "MAUD chunk too small");
        return RETURN_FAIL;
    }
    
    /* Allocate MAUD structure - use public memory (not chip RAM) */
    maud = (struct MAUDHeader *)AllocMem(sizeof(struct MAUDHeader), MEMF_PUBLIC | MEMF_CLEAR);
    if (!maud) {
        SetIFFSoundError(sound, IFFSOUND_NOMEM, "Failed to allocate MAUDHeader");
        return RETURN_FAIL;
    }
    
    /* Read fields individually from byte array */
    {
        UBYTE *src = (UBYTE *)sp->sp_Data;
        
        /* Read UWORD field (big-endian, 2 bytes) */
        maud->numChannels = (UWORD)((src[0] << 8) | src[1]);
        maud->sampleSize = (UWORD)((src[2] << 8) | src[3]);
        
        /* Read ULONG fields (big-endian, 4 bytes each) */
        maud->sampleRate = (ULONG)((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
        maud->numSampleFrames = (ULONG)((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
        maud->compression = (ULONG)((src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15]);
    }
    
    sound->maud = maud;
    sound->isCompressed = (maud->compression != 0);
    
    return RETURN_OK;
}

/*
** ReadBODY - Read BODY chunk header information
** Returns: RETURN_OK on success, RETURN_FAIL on error
** Follows iffparse.library pattern: CurrentChunk
*/
LONG ReadBODY(struct IFFSound *sound)
{
    struct ContextNode *cn;
    
    if (!sound || !sound->iff) {
        if (sound) {
            SetIFFSoundError(sound, IFFSOUND_INVALID, "Invalid sound or IFF handle");
        }
        return RETURN_FAIL;
    }
    
    /* Get current chunk (should be BODY or SSND) */
    cn = CurrentChunk(sound->iff);
    if (!cn) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "No current chunk");
        return RETURN_FAIL;
    }
    
    /* Check if it's BODY or SSND chunk */
    if (cn->cn_ID != ID_BODY && cn->cn_ID != ID_SSND) {
        SetIFFSoundError(sound, IFFSOUND_BADFILE, "Expected BODY or SSND chunk");
        return RETURN_FAIL;
    }
    
    sound->bodyChunkSize = cn->cn_Size;
    sound->bodyChunkPosition = 0; /* We're positioned at start of chunk */
    
    DEBUG_PRINTF2("DEBUG: ReadBODY - Found %s chunk, size=%ld\n",
                  (cn->cn_ID == ID_BODY) ? "BODY" : "SSND",
                  sound->bodyChunkSize);
    
    return RETURN_OK;
}

/*
** IFFSoundError - Get last error code
** Returns: Error code (IFFSOUND_* constant) or 0 for success
*/
LONG IFFSoundError(struct IFFSound *sound)
{
    if (!sound) {
        return IFFSOUND_INVALID;
    }
    return sound->lastError;
}

/*
** IFFSoundErrorString - Get last error message
** Returns: Pointer to error string (valid until next operation or FreeIFFSound)
*/
const char *IFFSoundErrorString(struct IFFSound *sound)
{
    if (!sound) {
        return "Invalid IFFSound pointer";
    }
    return sound->errorString;
}

