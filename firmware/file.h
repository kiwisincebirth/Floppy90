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
	FSIZE_t size;		// length of File
	FRESULT result;		// last error during normal IO. todo
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

file*    FileOpen(char* pszFileName, BYTE byMode);
void     FileClose(file* fp);
BYTE     FileIsOpen(file *fp);
uint32_t FileRead(file* fp, BYTE* pby, uint32_t nSize);
uint32_t FileWrite(file* fp, BYTE* pby, uint32_t nSize);
void     FileSeek(file* fp, int nOffset);
void     FileTruncate(file* fp);
void     FileFlush(file* fp);
void     FileCloseAll(void);
void     FileSystemInit(void);
int      FileReadLine(file* fp, char szLine[], int nMaxLen);
BYTE     FileExists(char* pszFileName);

#ifdef __cplusplus
}
#endif

#endif


