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

// See also the definition of FF_FS_LOCK (16)
// which governs number of open files.
#define MAX_FILES 16

typedef struct {
	BYTE    fileId;			// unique identifier (array Index)
	BYTE    byIsOpen;		// is it open / in use.
	FSIZE_t size;		    // length of File at open time
	BYTE	lrl;			// Logical Record Length (1-255) 0=256
	UINT    bytesReadWritten;  // during last read/write
	FRESULT lastResult;		// last error during normal IO.
	BYTE    driveId;		// drive number 0-9, default is 0
	TCHAR	fname[33];		// FAT 8.3 Path/Filename.ext
	FIL		f;
} file;

#pragma pack(push, 1)
	// Prevent compiler padding the structure
	// used in DIR API Responses
typedef struct {
	DWORD	fsize;			/* File size */
	WORD	fdate;			/* Last modified date */
	WORD	ftime;			/* Last modified time */
	BYTE	fattrib;		/* Attribute */
	TCHAR	fname[13];		/* Short file name (8.3 format) */
} FILINFO_API;
#pragma pack(pop)

//-----------------------------------------------------------------------------

// type conversion functions
void copyFILEINFO(FILINFO_API *dest, const FILINFO *src);

// Utility functions
void sanitizeTrs80Path(const char* src, char* dest, uint8_t* driveId, int maxLen);

	// todo remove this soon
	void run_sanitizer_tests();

//-----------------------------------------------------------------------------

// File Methods - Obtain a File Object
file*    findUnusedFile();
file*    FileOpen(file* file, BYTE byMode);
file*    FileOpenByName(char* pszFileName, BYTE byMode);

// Misc Metods
// BYTE     FileExists(char* pszFileName);

// File Methods - OPEN Files
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


