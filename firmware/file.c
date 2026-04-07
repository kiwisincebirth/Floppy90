#include "defines.h"
#include "file.h"
#include "system.h"
#include "string.h"

//-----------------------------------------------------------------------------

static file    g_fFiles[MAX_FILES];

// todo not sure why these are static ?
static DIR     dj;  /* Directory object */
static FILINFO fno; /* File information */

//-----------------------------------------------------------------------------
file* FileOpen(char* pszFileName, BYTE byMode)
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

	g_fFiles[i].lastResult = f_open(&g_fFiles[i].f, pszFileName, byMode);

	if (g_fFiles[i].lastResult == FR_OK) {
		g_fFiles[i].byIsOpen = TRUE;
		g_fFiles[i].size = f_size(&g_fFiles[i].f);
	} else {
		g_fFiles[i].byIsOpen = FALSE;
		g_fFiles[i].size = 0;
	}

	return &g_fFiles[i];
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
void FileSeek(file* fp, int nOffset)
{
	if (fp == NULL) return;
	fp->lastResult = f_lseek(&fp->f, nOffset);
}

//-----------------------------------------------------------------------------
void FileFlush(file* fp)
{
	if (fp == NULL) return;
	fp->lastResult = f_sync(&fp->f);
}

//-----------------------------------------------------------------------------
void FileTruncate(file* fp)
{
	if (fp == NULL) return;
	fp->lastResult = f_truncate(&fp->f);
}

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
		g_fFiles[i].byIsOpen = false;
	}
}

//-----------------------------------------------------------------------------
BYTE IsEOF(file* fp)
{
	if (fp == NULL) return TRUE;
	return f_eof(&fp->f);
}

//-----------------------------------------------------------------------------
int FileReadLine(file* fp, char szLine[], int nMaxLen)
{
	char* psz;
	int   nLen = 0;

	if ((fp == NULL) || (IsEOF(fp))) return -1;
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
BYTE FileExists(char* pszFileName)
{
	memset(&dj, 0, sizeof(dj));
	memset(&fno, 0, sizeof(fno));

	FRESULT fr = f_findfirst(&dj, &fno, "0:", "*");

	if (FR_OK != fr) return FALSE;

	while ((fr == FR_OK) && (fno.fname[0] != 0)) /* Repeat until a file is found */
	{
		if ((fno.fattrib & AM_DIR) || (fno.fattrib & AM_SYS)) {
			// pcAttrib = pcDirectory;
		} else if (stricmp(fno.fname, pszFileName) == 0) {
			return TRUE;
		}

		fr = f_findnext(&dj, &fno); /* Search for next item */
	}

	return FALSE;
}


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
