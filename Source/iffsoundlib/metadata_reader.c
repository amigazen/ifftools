/*
** metadata_reader.c - Metadata Chunk Reader Implementation (Internal to Library)
**
** Functions for reading IFF metadata chunks (NAME, AUTH, ANNO, ATAK, RLSE)
** All memory is owned by IFFSound and freed by FreeIFFSound()
** Pointers are valid until FreeIFFSound() is called
*/

#include "iffsound_private.h"
#include "iffsound.h"  /* For struct definitions */
#include "/debug.h"
#include <proto/exec.h>
#include <proto/iffparse.h>

/*
** ReadNAME - Read NAME chunk
** Returns: Pointer to null-terminated string in IFFSound, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
STRPTR ReadNAME(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    /* Return pointer to stored NAME data */
    return sound->metadata->name;
}

/*
** ReadCopyright - Read Copyright chunk
** Returns: Pointer to null-terminated string in IFFSound, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
STRPTR ReadCopyright(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    /* Return pointer to stored Copyright data */
    return sound->metadata->copyright;
}

/*
** ReadAuthor - Read AUTH chunk
** Returns: Pointer to null-terminated string in IFFSound, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
STRPTR ReadAuthor(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    /* Return pointer to stored AUTH data */
    return sound->metadata->author;
}

/*
** ReadAnnotation - Read ANNO chunk (first instance)
** Returns: Pointer to null-terminated string in IFFSound, or NULL if not found
** Multiple ANNO chunks can exist; this returns the first one
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
STRPTR ReadAnnotation(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    /* Return pointer to first ANNO instance */
    return sound->metadata->annotation;
}

/*
** ReadAllAnnotations - Read all ANNO chunks
** Returns: Pointer to TextList structure, or NULL if not found
** The structure contains count and array pointer into IFFSound's memory
** Pointers are valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct TextList *ReadAllAnnotations(struct IFFSound *sound)
{
    static struct TextList result;
    
    if (!sound || !sound->metadata || sound->metadata->annotationCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->annotationCount;
    result.texts = sound->metadata->annotationArray;
    
    return &result;
}

/*
** ReadATAK - Read ATAK chunk (attack envelope)
** Returns: Pointer to EGPointList structure, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct EGPointList *ReadATAK(struct IFFSound *sound)
{
    static struct EGPointList result;
    
    if (!sound || !sound->metadata || sound->metadata->atakCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->atakCount;
    result.points = sound->metadata->atak;
    
    return &result;
}

/*
** ReadRLSE - Read RLSE chunk (release envelope)
** Returns: Pointer to EGPointList structure, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct EGPointList *ReadRLSE(struct IFFSound *sound)
{
    static struct EGPointList result;
    
    if (!sound || !sound->metadata || sound->metadata->rlseCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->rlseCount;
    result.points = sound->metadata->rlse;
    
    return &result;
}

/*
** ReadAllMeta - Read all metadata chunks (internal function)
** Called by ParseIFFSound() after parsing the file
** Allocates metadata structure and reads all metadata chunks
*/
VOID ReadAllMeta(struct IFFSound *sound)
{
    struct StoredProperty *sp;
    struct CollectionItem *ci;
    ULONG count;
    ULONG i;
    UBYTE *data;
    ULONG size;
    STRPTR str;
    struct EGPoint *points;
    ULONG pointCount;
    
    if (!sound || !sound->iff) {
        return;
    }
    
    /* Allocate metadata structure if not already allocated */
    if (!sound->metadata) {
        sound->metadata = (struct IFFSoundMeta *)AllocMem(sizeof(struct IFFSoundMeta), MEMF_PUBLIC | MEMF_CLEAR);
        if (!sound->metadata) {
            return; /* Out of memory */
        }
    }
    
    /* Read NAME chunk */
    sp = FindProp(sound->iff, sound->formtype, ID_NAME);
    if (sp && sp->sp_Size > 0) {
        size = sp->sp_Size;
        str = (STRPTR)AllocMem(size + 1, MEMF_PUBLIC | MEMF_CLEAR);
        if (str) {
            CopyMem(sp->sp_Data, str, size);
            str[size] = '\0'; /* Ensure null termination */
            sound->metadata->name = str;
            sound->metadata->nameSize = size + 1;
        }
    }
    
    /* Read Copyright chunk */
    sp = FindProp(sound->iff, sound->formtype, ID_COPYRIGHT);
    if (sp && sp->sp_Size > 0) {
        size = sp->sp_Size;
        str = (STRPTR)AllocMem(size + 1, MEMF_PUBLIC | MEMF_CLEAR);
        if (str) {
            CopyMem(sp->sp_Data, str, size);
            str[size] = '\0'; /* Ensure null termination */
            sound->metadata->copyright = str;
            sound->metadata->copyrightSize = size + 1;
        }
    }
    
    /* Read AUTH chunk */
    sp = FindProp(sound->iff, sound->formtype, ID_AUTH);
    if (sp && sp->sp_Size > 0) {
        size = sp->sp_Size;
        str = (STRPTR)AllocMem(size + 1, MEMF_PUBLIC | MEMF_CLEAR);
        if (str) {
            CopyMem(sp->sp_Data, str, size);
            str[size] = '\0'; /* Ensure null termination */
            sound->metadata->author = str;
            sound->metadata->authorSize = size + 1;
        }
    }
    
    /* Read ANNO chunks (collection) */
    ci = FindCollection(sound->iff, sound->formtype, ID_ANNO);
    if (ci) {
        /* Count ANNO chunks */
        count = 0;
        {
            struct CollectionItem *item;
            item = ci;
            while (item) {
                count++;
                item = item->ci_Next;
            }
        }
        
        if (count > 0) {
            STRPTR *array;
            ULONG *sizes;
            
            array = (STRPTR *)AllocMem(count * sizeof(STRPTR), MEMF_PUBLIC | MEMF_CLEAR);
            sizes = (ULONG *)AllocMem(count * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
            
            if (array && sizes) {
                i = 0;
                {
                    struct CollectionItem *item;
                    item = ci;
                    while (item && i < count) {
                        size = item->ci_Size;
                        str = (STRPTR)AllocMem(size + 1, MEMF_PUBLIC | MEMF_CLEAR);
                        if (str) {
                            CopyMem(item->ci_Data, str, size);
                            str[size] = '\0'; /* Ensure null termination */
                            array[i] = str;
                            sizes[i] = size + 1;
                        }
                        item = item->ci_Next;
                        i++;
                    }
                }
                
                sound->metadata->annotationCount = count;
                sound->metadata->annotationArray = array;
                sound->metadata->annotationSizes = sizes;
                
                /* Store first annotation for ReadAnnotation() */
                if (count > 0 && array[0]) {
                    sound->metadata->annotation = array[0];
                }
            }
        }
    }
    
    /* Read ATAK chunk (8SVX only) */
    if (sound->formtype == ID_8SVX) {
        sp = FindProp(sound->iff, sound->formtype, ID_ATAK);
        if (sp && sp->sp_Size > 0) {
            /* ATAK contains array of EGPoint structures */
            if (sp->sp_Size % sizeof(struct EGPoint) == 0) {
                pointCount = sp->sp_Size / sizeof(struct EGPoint);
                points = (struct EGPoint *)AllocMem(sp->sp_Size, MEMF_PUBLIC | MEMF_CLEAR);
                if (points) {
                    CopyMem(sp->sp_Data, points, sp->sp_Size);
                    /* Convert from big-endian if needed */
                    {
                        UBYTE *src = (UBYTE *)sp->sp_Data;
                        ULONG j;
                        for (j = 0; j < pointCount; j++) {
                            points[j].duration = (UWORD)((src[j * 6 + 0] << 8) | src[j * 6 + 1]);
                            points[j].dest = (ULONG)((src[j * 6 + 2] << 24) | (src[j * 6 + 3] << 16) | (src[j * 6 + 4] << 8) | src[j * 6 + 5]);
                        }
                    }
                    sound->metadata->atak = points;
                    sound->metadata->atakCount = pointCount;
                }
            }
        }
    }
    
    /* Read RLSE chunk (8SVX only) */
    if (sound->formtype == ID_8SVX) {
        sp = FindProp(sound->iff, sound->formtype, ID_RLSE);
        if (sp && sp->sp_Size > 0) {
            /* RLSE contains array of EGPoint structures */
            if (sp->sp_Size % sizeof(struct EGPoint) == 0) {
                pointCount = sp->sp_Size / sizeof(struct EGPoint);
                points = (struct EGPoint *)AllocMem(sp->sp_Size, MEMF_PUBLIC | MEMF_CLEAR);
                if (points) {
                    CopyMem(sp->sp_Data, points, sp->sp_Size);
                    /* Convert from big-endian if needed */
                    {
                        UBYTE *src = (UBYTE *)sp->sp_Data;
                        ULONG j;
                        for (j = 0; j < pointCount; j++) {
                            points[j].duration = (UWORD)((src[j * 6 + 0] << 8) | src[j * 6 + 1]);
                            points[j].dest = (ULONG)((src[j * 6 + 2] << 24) | (src[j * 6 + 3] << 16) | (src[j * 6 + 4] << 8) | src[j * 6 + 5]);
                        }
                    }
                    sound->metadata->rlse = points;
                    sound->metadata->rlseCount = pointCount;
                }
            }
        }
    }
    
    /* Read FVER chunk */
    sp = FindProp(sound->iff, sound->formtype, ID_FVER);
    if (sp && sp->sp_Size > 0) {
        if (sound->formtype == ID_AIFC) {
            /* For AIFC, FVER contains a 4-byte timestamp */
            if (sp->sp_Size >= 4) {
                UBYTE *src = (UBYTE *)sp->sp_Data;
                sound->metadata->fver = (ULONG)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
            }
        } else {
            /* For other formats, FVER contains an AmigaOS version string */
            size = sp->sp_Size;
            str = (STRPTR)AllocMem(size + 1, MEMF_PUBLIC | MEMF_CLEAR);
            if (str) {
                CopyMem(sp->sp_Data, str, size);
                str[size] = '\0'; /* Ensure null termination */
                sound->metadata->fverString = str;
                sound->metadata->fverStringSize = size + 1;
            }
        }
    }
    
    /* Read MARK chunks (AIFF/AIFC only) */
    if (sound->formtype == ID_AIFF || sound->formtype == ID_AIFC) {
        ci = FindCollection(sound->iff, sound->formtype, ID_MARK);
        if (ci) {
            /* MARK chunk structure: UWORD numMarkers, then array of Marker structures */
            /* Each Marker: UWORD id, ULONG position, UBYTE nameLen, char name[nameLen] */
            UBYTE *markerData = (UBYTE *)ci->ci_Data;
            UWORD numMarkers;
            ULONG pos;
            struct AIFFMarker *markers;
            
            if (ci->ci_Size >= 2) {
                ULONG *nameSizes;  /* Declare at top of block for C89 */
                
                numMarkers = (UWORD)((markerData[0] << 8) | markerData[1]);
                pos = 2;
                
                if (numMarkers > 0 && numMarkers < 65535) {
                    markers = (struct AIFFMarker *)AllocMem(numMarkers * sizeof(struct AIFFMarker), MEMF_PUBLIC | MEMF_CLEAR);
                    nameSizes = (ULONG *)AllocMem(numMarkers * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
                    if (markers && nameSizes) {
                        for (i = 0; i < numMarkers && pos < ci->ci_Size; i++) {
                            if (pos + 6 <= ci->ci_Size) {
                                markers[i].id = (UWORD)((markerData[pos] << 8) | markerData[pos + 1]);
                                markers[i].position = (ULONG)((markerData[pos + 2] << 24) | (markerData[pos + 3] << 16) |
                                                              (markerData[pos + 4] << 8) | markerData[pos + 5]);
                                pos += 6;
                                
                                /* Read Pascal string (nameLen byte followed by name) */
                                if (pos < ci->ci_Size) {
                                    UBYTE nameLen = markerData[pos++];
                                    if (nameLen > 0 && pos + nameLen <= ci->ci_Size) {
                                        nameSizes[i] = nameLen + 1;
                                        markers[i].name = (STRPTR)AllocMem(nameSizes[i], MEMF_PUBLIC | MEMF_CLEAR);
                                        if (markers[i].name) {
                                            CopyMem(&markerData[pos], markers[i].name, nameLen);
                                            markers[i].name[nameLen] = '\0';
                                        }
                                        pos += nameLen;
                                    } else {
                                        nameSizes[i] = 0;
                                    }
                                } else {
                                    nameSizes[i] = 0;
                                }
                            }
                        }
                        sound->metadata->markerCount = numMarkers;
                        sound->metadata->markers = markers;
                        sound->metadata->markerNameSizes = nameSizes;
                    } else {
                        if (markers) FreeMem(markers, numMarkers * sizeof(struct AIFFMarker));
                        if (nameSizes) FreeMem(nameSizes, numMarkers * sizeof(ULONG));
                    }
                }
            }
        }
    }
    
    /* Read INST chunk (AIFF/AIFC only) */
    if (sound->formtype == ID_AIFF || sound->formtype == ID_AIFC) {
        sp = FindProp(sound->iff, sound->formtype, ID_INST);
        if (sp && sp->sp_Size >= 20) {
            /* INST chunk is 20 bytes: 6 bytes instrument params, 2 bytes gain, 6 bytes sustain loop, 6 bytes release loop */
            struct AIFFInstrument *inst;
            UBYTE *src = (UBYTE *)sp->sp_Data;
            
            inst = (struct AIFFInstrument *)AllocMem(sizeof(struct AIFFInstrument), MEMF_PUBLIC | MEMF_CLEAR);
            if (inst) {
                inst->baseNote = src[0];
                inst->detune = src[1];
                inst->lowNote = src[2];
                inst->highNote = src[3];
                inst->lowVelocity = src[4];
                inst->highVelocity = src[5];
                inst->gain = (WORD)((src[6] << 8) | src[7]);
                inst->sustainLoop.playMode = (WORD)((src[8] << 8) | src[9]);
                inst->sustainLoop.beginLoop = (UWORD)((src[10] << 8) | src[11]);
                inst->sustainLoop.endLoop = (UWORD)((src[12] << 8) | src[13]);
                inst->releaseLoop.playMode = (WORD)((src[14] << 8) | src[15]);
                inst->releaseLoop.beginLoop = (UWORD)((src[16] << 8) | src[17]);
                inst->releaseLoop.endLoop = (UWORD)((src[18] << 8) | src[19]);
                
                sound->metadata->instrument = inst;
            }
        }
    }
    
    /* Read COMT chunks (AIFF/AIFC only) */
    if (sound->formtype == ID_AIFF || sound->formtype == ID_AIFC) {
        ci = FindCollection(sound->iff, sound->formtype, ID_COMT);
        if (ci) {
            /* COMT chunk structure: UWORD numComments, then array of Comment structures */
            /* Each Comment: ULONG timeStamp, UWORD marker, UWORD count, char text[count] */
            UBYTE *commentData = (UBYTE *)ci->ci_Data;
            UWORD numComments;
            ULONG pos;
            struct AIFFComment *comments;
            
            if (ci->ci_Size >= 2) {
                ULONG *textSizes;  /* Declare at top of block for C89 */
                UWORD textCount;   /* Declare at top of block for C89 */
                
                numComments = (UWORD)((commentData[0] << 8) | commentData[1]);
                pos = 2;
                
                if (numComments > 0 && numComments < 65535) {
                    comments = (struct AIFFComment *)AllocMem(numComments * sizeof(struct AIFFComment), MEMF_PUBLIC | MEMF_CLEAR);
                    textSizes = (ULONG *)AllocMem(numComments * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
                    if (comments && textSizes) {
                        for (i = 0; i < numComments && pos < ci->ci_Size; i++) {
                            if (pos + 8 <= ci->ci_Size) {
                                comments[i].timeStamp = (ULONG)((commentData[pos] << 24) | (commentData[pos + 1] << 16) |
                                                                 (commentData[pos + 2] << 8) | commentData[pos + 3]);
                                comments[i].marker = (UWORD)((commentData[pos + 4] << 8) | commentData[pos + 5]);
                                textCount = (UWORD)((commentData[pos + 6] << 8) | commentData[pos + 7]);
                                pos += 8;
                                
                                /* Read text string */
                                if (textCount > 0 && pos + textCount <= ci->ci_Size) {
                                    textSizes[i] = textCount + 1;
                                    comments[i].text = (STRPTR)AllocMem(textSizes[i], MEMF_PUBLIC | MEMF_CLEAR);
                                    if (comments[i].text) {
                                        CopyMem(&commentData[pos], comments[i].text, textCount);
                                        comments[i].text[textCount] = '\0';
                                    }
                                    pos += textCount;
                                } else {
                                    textSizes[i] = 0;
                                }
                            }
                        }
                        sound->metadata->commentCount = numComments;
                        sound->metadata->comments = comments;
                        sound->metadata->commentTextSizes = textSizes;
                    } else {
                        if (comments) FreeMem(comments, numComments * sizeof(struct AIFFComment));
                        if (textSizes) FreeMem(textSizes, numComments * sizeof(ULONG));
                    }
                }
            }
        }
    }
    
    /* Read CHAN chunk (8SVX only) */
    if (sound->formtype == ID_8SVX || sound->formtype == ID_16SV) {
        sp = FindProp(sound->iff, sound->formtype, ID_CHAN);
        if (sp && sp->sp_Size >= 4) {
            /* CHAN contains a 4-byte LONG value */
            UBYTE *src = (UBYTE *)sp->sp_Data;
            sound->metadata->chan = (ULONG)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
        }
    }
    
    /* Read PAN chunk (8SVX only) */
    if (sound->formtype == ID_8SVX || sound->formtype == ID_16SV) {
        sp = FindProp(sound->iff, sound->formtype, ID_PAN);
        if (sp && sp->sp_Size >= 4) {
            /* PAN contains a 4-byte Fixed value */
            UBYTE *src = (UBYTE *)sp->sp_Data;
            sound->metadata->pan = (Fixed)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
        }
    }
    
    /* Read SEQN chunk (8SVX only) */
    if (sound->formtype == ID_8SVX || sound->formtype == ID_16SV) {
        sp = FindProp(sound->iff, sound->formtype, ID_SEQN);
        if (sp && sp->sp_Size >= 8 && (sp->sp_Size % 8 == 0)) {
            /* SEQN contains pairs of ULONG values (start, end) */
            ULONG loopCount;
            struct LoopPair *loops;
            UBYTE *src;
            ULONG j;
            
            loopCount = sp->sp_Size / 8;  /* Each loop is 8 bytes (2 ULONGs) */
            loops = (struct LoopPair *)AllocMem(loopCount * sizeof(struct LoopPair), MEMF_PUBLIC | MEMF_CLEAR);
            if (loops) {
                src = (UBYTE *)sp->sp_Data;
                for (j = 0; j < loopCount; j++) {
                    /* Read start offset */
                    loops[j].start = (ULONG)((src[j * 8 + 0] << 24) | (src[j * 8 + 1] << 16) |
                                             (src[j * 8 + 2] << 8) | src[j * 8 + 3]);
                    /* Read end offset */
                    loops[j].end = (ULONG)((src[j * 8 + 4] << 24) | (src[j * 8 + 5] << 16) |
                                           (src[j * 8 + 6] << 8) | src[j * 8 + 7]);
                }
                sound->metadata->seqnCount = loopCount;
                sound->metadata->seqnLoops = loops;  /* Store pointer to LoopPair array */
            }
        }
    }
    
    /* Read FADE chunk (8SVX only) */
    if (sound->formtype == ID_8SVX || sound->formtype == ID_16SV) {
        sp = FindProp(sound->iff, sound->formtype, ID_FADE);
        if (sp && sp->sp_Size >= 4) {
            /* FADE contains a 4-byte ULONG value (loop number) */
            UBYTE *src = (UBYTE *)sp->sp_Data;
            sound->metadata->fade = (ULONG)((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
        }
    }
}

/*
** ReadFVER - Read FVER chunk (AIFC format version timestamp)
** Returns: Format version timestamp, or 0 if not found
** FVER timestamp is only present in AIFC files
** For other formats, use ReadFVERString() to get the version string
*/
ULONG ReadFVER(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return 0;
    }
    
    return sound->metadata->fver;
}

/*
** ReadFVERString - Read FVER chunk (AmigaOS version string)
** Returns: Pointer to version string (e.g., "$VER: name ver.rev"), or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
** FVER version string is present in non-AIFC formats (8SVX, 16SV, AIFF, MAUD)
** For AIFC, use ReadFVER() to get the timestamp
*/
STRPTR ReadFVERString(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    return sound->metadata->fverString;
}

/*
** ReadAllMarkers - Read all MARK chunks
** Returns: Pointer to AIFFMarkerList structure, or NULL if not found
** The structure contains count and array pointer into IFFSound's memory
** Pointers are valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct AIFFMarkerList *ReadAllMarkers(struct IFFSound *sound)
{
    static struct AIFFMarkerList result;
    
    if (!sound || !sound->metadata || sound->metadata->markerCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->markerCount;
    result.markers = sound->metadata->markers;
    
    return &result;
}

/*
** ReadINST - Read INST chunk (instrument data)
** Returns: Pointer to AIFFInstrument structure, or NULL if not found
** Pointer is valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct AIFFInstrument *ReadINST(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return NULL;
    }
    
    return sound->metadata->instrument;
}

/*
** ReadAllComments - Read all COMT chunks
** Returns: Pointer to AIFFCommentList structure, or NULL if not found
** The structure contains count and array pointer into IFFSound's memory
** Pointers are valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct AIFFCommentList *ReadAllComments(struct IFFSound *sound)
{
    static struct AIFFCommentList result;
    
    if (!sound || !sound->metadata || sound->metadata->commentCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->commentCount;
    result.comments = sound->metadata->comments;
    
    return &result;
}

/*
** ReadCHAN - Read CHAN chunk (8SVX channel assignment)
** Returns: Channel assignment (RIGHT=4, LEFT=2, STEREO=6), or 0 if not found
** CHAN is only present in 8SVX files
*/
ULONG ReadCHAN(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return 0;
    }
    
    return sound->metadata->chan;
}

/*
** ReadPAN - Read PAN chunk (8SVX panning)
** Returns: Panning position (0 to Unity), or 0 if not found
** PAN is only present in 8SVX files
*/
Fixed ReadPAN(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return 0;
    }
    
    return sound->metadata->pan;
}

/*
** ReadSEQN - Read SEQN chunk (8SVX sequence/loops)
** Returns: Pointer to LoopList structure, or NULL if not found
** The structure contains count and array pointer into IFFSound's memory
** Pointers are valid until FreeIFFSound() is called
** Library owns the memory - caller must NOT free
*/
struct LoopList *ReadSEQN(struct IFFSound *sound)
{
    static struct LoopList result;
    
    if (!sound || !sound->metadata || sound->metadata->seqnCount == 0) {
        return NULL;
    }
    
    result.count = sound->metadata->seqnCount;
    result.loops = sound->metadata->seqnLoops;
    
    return &result;
}

/*
** ReadFADE - Read FADE chunk (8SVX fade information)
** Returns: Loop number to start fading, or 0 if not found
** FADE is only present in 8SVX files
*/
ULONG ReadFADE(struct IFFSound *sound)
{
    if (!sound || !sound->metadata) {
        return 0;
    }
    
    return sound->metadata->fade;
}

