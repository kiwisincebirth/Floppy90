#include "defines.h"
#include "file.h"

#include <ctype.h>

#include "string.h"
#include <util.h>

//-----------------------------------------------------------------------------

static TFILE    g_fFiles[MAX_FILES];

//------------------------------------------------------------------------------

void FileSystemInit(void)
{
	for (int i = 0; i < MAX_FILES; ++i) {
		g_fFiles[i].fileId = i + 42; // unique non-zero identifier
		g_fFiles[i].byIsOpen = false;
	}
}

//------------------------------------------------------------------------------

// In Pico use a translate function to convert from 0-20 to 128-192
// where the 128-192 maps to TRSDOS 0-63
// These translations map expected Codes to trsdos counterparts
// we provide a mapping where we can, or leave unchanged where unsure.
// NOTE : ZERO FR_OK should remain as ZERO !!!

// TRS Unmapped Errors
// DIRSPCFUL$	EQU	26	; Directory Space Full
// DSCSPCFUL$	EQU	27	; Disk Space Full

// 	DB	$1A	; (7) Access denied or directory full
// 	DB	$19	; (8) File Path Exists : Name Collission

BYTE resultTranslation[] = {
	FR_OK,					/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	TRS_FILE_NOT_FOUND, 	/* (4) Could not find the file */
	TRS_PATH_NOT_FOUND,		/* (5) Could not find the path */
	TRS_ILLEGAL_FILENAME,	/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied or directory full */
	FR_EXIST,				/* (8) Name Collision File already exists */
	TRS_ILL_FILENUMBER,		/* (9) The file/directory object is invalid */
	TRS_WRITE_PROTECTED,	/* (10) The physical drive is write protected */
	TRS_ILLEGAL_DRIVENUM,	/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
};

BYTE __inline trans(FRESULT result) {
	return result <= 19 ? resultTranslation[result] : result;
}

//------------------------------------------------------------------------------

TFILE* findUnusedFile()
{
	if (sd_byCardInialized == 0) return NULL;

	for (int i = 0; i < MAX_FILES; i++) {
		TFILE *fInfo = &g_fFiles[i];

		if ( ! fInfo->byIsOpen ) {
			fInfo->fname[0] = '\0'; // Instant string "clear"
			fInfo->lastResult = FR_OK;
			fInfo->bytesReadWritten = 0;
			fInfo->lrl = 0;
			fInfo->driveId = 0;
			return fInfo;
		}
	}
	return NULL;
}

/**
 * Find a file we previously opened.
 */
TFILE* findOpenFile( BYTE fileId )
{
	if (sd_byCardInialized == 0) return NULL;

	// Ensure the ID is within the valid range before doing math
	if (fileId < 42 || fileId >= (42 + MAX_FILES)) {
		return NULL;
	}

	TFILE *fInfo = &g_fFiles[ fileId - 42 ];
	if ( fInfo->fileId == fileId && fInfo->byIsOpen ) {
		return fInfo;
	}
	return NULL;
}

// TFILE* findOpenFile( BYTE fileId )
// {
// 	if (sd_byCardInialized == 0) return NULL;
//
// 	for (int i = 0; i < MAX_FILES; i++) {
// 		TFILE *fInfo = &g_fFiles[i];
//
// 		if ( fInfo->fileId == fileId && fInfo->byIsOpen ) {
// 			return fInfo;
// 		}
// 	}
// 	return NULL;
// }

//-----------------------------------------------------------------------------

TFILE* FileOpenByName(char* pszFileName, BYTE byMode)
{
	TFILE *fInfo = findUnusedFile();
	if (!fInfo) return NULL; // Always check for exhaustion!

	// THE ACTUAL OPEN
	fInfo->lastResult = trans( f_open(&fInfo->f, pszFileName, byMode) );

	if (fInfo->lastResult == FR_OK) {

		// basic properties
		fInfo->byIsOpen = TRUE;

		// copy the File name
		// Using strncpy ensures we stop at the Null OR the limit.
		// Since file->fname is 33 bytes (32 + \0), we copy up to 32.
		strncpy(fInfo->fname, pszFileName, sizeof(fInfo->fname) - 1);
		fInfo->fname[sizeof(fInfo->fname) - 1] = '\0'; // Manual safety termination
	}

	return fInfo;
}

//-----------------------------------------------------------------------------
FRESULT FileClose(TFILE* fp)
{
	// 1. Safety Gate
	if (fp == NULL || fp->byIsOpen == FALSE) return FR_OK;

	// 2. Physical Close
	// Even if this returns an error (e.g., SD card removed),
	// we still want to reclaim the memory slot.
	fp->lastResult = trans(f_close(&fp->f) );

	// 3. State Reset - effectively returns it for reuse
	fp->byIsOpen = FALSE;
	// Note: We keep the fname in memory; it's useful for
	// "Last File Accessed" or "Reopen" features.

	return fp->lastResult;
}

//-----------------------------------------------------------------------------
__inline BYTE FileIsOpen(TFILE *fp)
{
	return fp != NULL ? fp->byIsOpen : FALSE;
}

//-----------------------------------------------------------------------------
FRESULT FileRead(TFILE* fp, BYTE* pby, uint32_t nSize)
{
	if (!fp || !fp->byIsOpen) return TRS_ILL_FILENUMBER;

	// Default to TRS-80 sector size if 0
	uint32_t toRead = (nSize == 0) ? 256 : nSize;

	fp->lastResult = trans(f_read(&fp->f, pby, toRead, &fp->bytesReadWritten));

	// todo Optional: If you want to mimic TRS-80 EOF behavior,
	// todo you could check if bytesReadWritten < toRead here.

	return fp->lastResult;
}

FRESULT FileWrite(TFILE* fp, BYTE* pby, uint32_t nSize)
{
	if (!fp || !fp->byIsOpen) return TRS_ILL_FILENUMBER;

	uint32_t toWrite = (nSize == 0) ? 256 : nSize;

	fp->lastResult = trans(f_write(&fp->f, pby, toWrite, &fp->bytesReadWritten));

	// IMPORTANT: In FatFs, if bytesReadWritten < toWrite,
	// it usually means the SD card is physically full.
	if (fp->lastResult == FR_OK && fp->bytesReadWritten < toWrite) {
		fp->lastResult = FR_DENIED;
		// todo denied or Full. todo confirm error?
	}

	return fp->lastResult;
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
// todo investigate this and ensure called at system RESET
void FileCloseAll(void)
{
	for (int i = 0; i < MAX_FILES; ++i) {
		FileClose(&g_fFiles[i]);
	}
}

//-----------------------------------------------------------------------------
BYTE isFileEOF(TFILE* fp)
{
	if (fp == NULL) return TRUE;
	return f_eof(&fp->f);
}

uint32_t FileGetSize(TFILE* fp) {
	if (!fp || !fp->byIsOpen) return 0;
	return (uint32_t)f_size(&fp->f);
}

uint32_t FileGetPos(TFILE* fp) {
	if (!fp || !fp->byIsOpen) return 0;
	return (uint32_t)f_tell(&fp->f);
}

//-----------------------------------------------------------------------------
int FileReadLine(TFILE* fp, char szLine[], int nMaxLen)
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

// static DIR     dj;  /* Directory object */
// static FILINFO fno; /* File information */

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

////////////////////////////////////////////////////////////////////////////////////

__inline BYTE DirOpen( DIR* dp, const TCHAR* path	) {
	return trans( f_opendir(dp, path) );
}

__inline BYTE DirRead( DIR* dp, FILINFO* fno	) {
	return trans( f_readdir(dp, fno) );
}

__inline BYTE DirFindFirst( DIR* dp,	FILINFO* fno, const TCHAR* path, const TCHAR* pattern ) {
	return trans( f_findfirst(dp, fno, path, pattern) );
}

__inline BYTE DirFindNext ( DIR* dp,	FILINFO* fno ) {
	return trans( f_findnext(dp, fno) );
}
__inline BYTE DirChange ( const TCHAR* path ) {
	return trans( f_chdir( path ) );
}

__inline BYTE DirMake ( const TCHAR* path ) {
	return trans( f_mkdir( path ) );
}

__inline BYTE FileDelete ( const TCHAR* file ) {
	return trans( f_unlink( file ) );
}

__inline BYTE FileRename ( const TCHAR* old, const TCHAR* new ) {
	return trans( f_rename( old, new ) );
}

__inline BYTE CurrentDirectory (TCHAR* buff,	UINT len) {
	return trans( f_getcwd( buff, len ) );
}


////////////////////////////////////////////////////////////////////////////////////

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

