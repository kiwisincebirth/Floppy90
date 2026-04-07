#ifndef __FILE_C_
#define __FILE_C_

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

#ifndef FA_READ
    #define FA_READ  0x01
    #define FA_WRITE 0x02
#endif

#define MAX_FILES 16

typedef struct {
	byte byIsOpen;
	FSIZE_t size;		    // length of File
	UINT bytesReadWritten;  // during last read write
	FRESULT lastResult;		// last error during normal IO. todo
	FIL  f;
} file;

#pragma pack(push, 1)		// Prevent compiler padding the structure
typedef struct {
	DWORD	fsize;			/* File size */
	WORD	fdate;			/* Last modified date */
	WORD	ftime;			/* Last modified time */
	BYTE	fattrib;		/* Attribute */
	TCHAR	fname[13];		/* Short file name (8.3 format) */
} FILINFO_API;
#pragma pack(pop)

void copyFILEINFO(FILINFO_API *dest, const FILINFO *src);

//-----------------------------------------------------------------------------

// File Methods
BYTE     FileExists(char* pszFileName);
file*    FileOpen(char* pszFileName, BYTE byMode);
BYTE     FileIsOpen(file *fp);
FRESULT  FileWrite(file* fp, BYTE* pby, uint32_t nSize);
FRESULT  FileRead (file* fp, BYTE* pby, uint32_t nSize);
int      FileReadLine(file* fp, char szLine[], int nMaxLen);
void     FileClose(file* fp);

// Not currently Used
//void     FileSeek(file* fp, int nOffset);
//void     FileTruncate(file* fp);
//void     FileFlush(file* fp);

// Lifecycle methods for the entire Obeject
void     FileSystemInit(void);
void     FileCloseAll(void);

#ifdef __cplusplus
}
#endif

#endif


