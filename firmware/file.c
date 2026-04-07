#include "defines.h"
#include "file.h"

#include <ctype.h>

#include "system.h"
#include "string.h"
#include <util.h>

//-----------------------------------------------------------------------------

static file    g_fFiles[MAX_FILES];

// todo not sure why these are static ?
static DIR     dj;  /* Directory object */
static FILINFO fno; /* File information */

file* findUnusedFile()
{
	if (sd_byCardInialized == 0) {
		return NULL;
	}

	// find the first unused file buffer
	int i = 0;

	while ((i < MAX_FILES) && (g_fFiles[i].byIsOpen)) {
		++i;
	}

	if (i >= MAX_FILES) return NULL;

	return &g_fFiles[i];
}

//-----------------------------------------------------------------------------
file* FileOpen(char* pszFileName, BYTE byMode)
{
	file *file = findUnusedFile();

	file->lastResult = f_open(&file->f, pszFileName, byMode);

	if (file->lastResult == FR_OK) {
		file->byIsOpen = TRUE;
		file->size = f_size( &file->f );
	} else {
		file->byIsOpen = FALSE;
		file->size = 0;
	}

	return file;
}

//-----------------------------------------------------------------------------
void FileClose(file* fp)
{
	if ((fp == NULL) || (fp->byIsOpen == FALSE)) return;

	fp->lastResult = f_close(&fp->f); // noting the result is ignored
	fp->byIsOpen = FALSE;
}

//-----------------------------------------------------------------------------
__inline BYTE FileIsOpen(file *fp)
{
	return fp != NULL ? fp->byIsOpen : FALSE;
}

//-----------------------------------------------------------------------------
FRESULT FileRead(file* fp, BYTE* pby, uint32_t nSize)
{
	if ((fp == NULL) || (fp->byIsOpen == 0)) return FR_INVALID_OBJECT;
	if (nSize==0) nSize = 0x100; // 256 bytes if 0 is passed

	fp->lastResult = f_read(&fp->f, pby, nSize, &fp->bytesReadWritten);

	return fp->lastResult;
}

//-----------------------------------------------------------------------------
FRESULT FileWrite(file* fp, BYTE* pby, uint32_t nSize)
{
	if ((fp == NULL) || (fp->byIsOpen == 0)) return FR_INVALID_OBJECT;
	if (nSize==0) nSize = 0x100; // 256 bytes if 0 is passed

	return fp->lastResult = f_write(&fp->f, pby, nSize, &fp->bytesReadWritten);
}

//-----------------------------------------------------------------------------
// void FileSeek(file* fp, int nOffset)
// {
// 	if (fp == NULL) return;
// 	fp->lastResult = f_lseek(&fp->f, nOffset);
// }

//-----------------------------------------------------------------------------
// void FileFlush(file* fp)
// {
// 	if (fp == NULL) return;
// 	fp->lastResult = f_sync(&fp->f);
// }

//-----------------------------------------------------------------------------
// void FileTruncate(file* fp)
// {
// 	if (fp == NULL) return;
// 	fp->lastResult = f_truncate(&fp->f);
// }

//-----------------------------------------------------------------------------
void FileCloseAll(void)
{
	for (int i = 0; i < MAX_FILES; ++i) {
		FileClose(&g_fFiles[i]);
	}
}

//-----------------------------------------------------------------------------
void FileSystemInit(void)
{
	for (int i = 0; i < MAX_FILES; ++i) {
		g_fFiles[i].fileId = i + 42; // unique identifier
		g_fFiles[i].byIsOpen = false;
	}
}

//-----------------------------------------------------------------------------
BYTE isFileEOF(file* fp)
{
	if (fp == NULL) return TRUE;
	return f_eof(&fp->f);
}

//-----------------------------------------------------------------------------
int FileReadLine(file* fp, char szLine[], int nMaxLen)
{
	char* psz;
	int   nLen = 0;

	if ((fp == NULL) || (isFileEOF(fp))) return -1;
	if (nMaxLen == 0) return -1;

	--nMaxLen;
	szLine[0] = 0;

	f_gets(szLine, nMaxLen, &fp->f);						/* Get a string from the file */

	// remove CR
	psz = strchr(szLine, '\r');

	if (psz != NULL) {
		*psz = 0;
	}

	// remove LF
	psz = strchr(szLine, '\n');

	if (psz != NULL) {
		*psz = 0;
	}

	nLen = (int)strlen(szLine);

	return nLen;
}

////////////////////////////////////////////////////////////////////////////////////
// BYTE FileExists(char* pszFileName)
// {
// 	memset(&dj, 0, sizeof(dj));
// 	memset(&fno, 0, sizeof(fno));
//
// 	FRESULT fr = f_findfirst(&dj, &fno, "0:", "*");
//
// 	if (FR_OK != fr) return FALSE;
//
// 	while ((fr == FR_OK) && (fno.fname[0] != 0)) /* Repeat until a file is found */
// 	{
// 		if ((fno.fattrib & AM_DIR) || (fno.fattrib & AM_SYS)) {
// 			// pcAttrib = pcDirectory;
// 		} else if (stricmp(fno.fname, pszFileName) == 0) {
// 			return TRUE;
// 		}
//
// 		fr = f_findnext(&dj, &fno); /* Search for next item */
// 	}
//
// 	return FALSE;
// }


void copyFILEINFO(FILINFO_API *dest, const FILINFO *src) {
	// 1. Critical Guard: If FatFs returns a null pointer, abort safely
	if (!src || !dest) return;

	dest->fsize   = src->fsize;
	dest->fdate   = src->fdate;
	dest->ftime   = src->ftime;
	dest->fattrib = src->fattrib;

	// 2. Logic: Prioritize the 8.3 name (altname) if it exists.
	// If not, use the main fname (which must be 8.3 if altname is empty).
	const char* s_ptr = (src->altname[0] != '\0') ? src->altname : src->fname;

	// 3. Safety: Use strncpy to ensure we never write more than 12 chars + null
	// This prevents the RP2350 from crashing by overwriting its own stack/heap.
	strncpy(dest->fname, s_ptr, 12);
	dest->fname[12] = '\0'; // Manually force the 13th byte to be NULL
}

// void copyFILEINFO(FILINFO_API *dest, const FILINFO *src) {
// 	if (!src || !dest) return;
//
// 	dest->fsize = src->fsize;
// 	dest->fdate = src->fdate;
// 	dest->ftime = src->ftime;
// 	dest->fattrib = src->fattrib;
//
// 	if (src->altname[0]) {
//         // use altname , or fallback to fname
// 		strncpy(dest->fname, src->altname, sizeof(dest->fname) - 1);
//     } else {
// 		strncpy(dest->fname, src->fname, sizeof(dest->fname) - 1);
//     }
// 	dest->fname[sizeof(dest->fname) - 1] = '\0';
//
// }

/**
 * Processes a TRS-80 style path into a FAT-compatible path.
 * Handles: Uppercasing, / to . conversion for extensions,
 * :D drive extraction, and legacy terminators.
 * * @param src      The raw string from Z80 (may not be null terminated)
 * @param dest     Buffer to store sanitized path (min 33 bytes)
 * @param driveId  Pointer to update with extracted drive number (0-9)
 * @param maxLen   Maximum bytes to read from src (usually 32)
 */
void __not_in_flash_func(sanitizeTrs80Path)(const char* src, char* dest, uint8_t* driveId, int maxLen) {

	int sIdx = 0;
    int dIdx = 0;
    int lastSlashIdx = -1;
    bool hasDot = false;

    memset(dest, 0, 33);
    if (driveId) *driveId = 0;

    while (sIdx < maxLen) {
        unsigned char c = src[sIdx++];

        if (c == 0x0D || c == 0x03 || c == 0x00) break;

        if (c == ':') {
            if (sIdx < maxLen && isdigit((unsigned char)src[sIdx])) {
                if (driveId) *driveId = src[sIdx] - '0';
            }
            break;
        }

        if (c == '.') {
            // Rule: Check for password truncation.
            // If we already have a dot OR we have a legacy slash-extension...
            if (hasDot || (lastSlashIdx != -1 && (dIdx - 1 - lastSlashIdx) <= 3)) {
                // If it's not a relative path (../), check length of what follows.
                if (dIdx > 0 && dest[dIdx-1] != '.') {
                    int lookAhead = 0;
                    while (sIdx + lookAhead < maxLen && isalnum((unsigned char)src[sIdx + lookAhead])) {
                        lookAhead++;
                    }
                    // If it's a long "password" (4+ chars), terminate now.
                    if (lookAhead >= 4) break;
                }
            }
        }

        if (isalnum(c) || c == '/' || c == '.') {
            c = toupper(c);
            if (c == '/') {
                lastSlashIdx = dIdx;
                hasDot = false;
            }
            if (c == '.') hasDot = true;

            if (dIdx < 32) {
                dest[dIdx++] = (char)c;
            }
        }
    }
    dest[dIdx] = '\0';

    // Final Legacy Conversion Fallback
    if (lastSlashIdx != -1) {
        bool dotInFinalSegment = false;
        for (int i = lastSlashIdx + 1; i < dIdx; i++) {
            if (dest[i] == '.') dotInFinalSegment = true;
        }

        if (!dotInFinalSegment) {
            int charsAfterSlash = (dIdx - 1) - lastSlashIdx;
            if (charsAfterSlash > 0 && charsAfterSlash <= 3) {
                dest[lastSlashIdx] = '.';
            }
        }
    }
}

