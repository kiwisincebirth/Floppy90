#ifndef __FILE_C_
#define __FILE_C_
#include <stddef.h>

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
		BYTE	lrl;			// Logical Record Length (1-255) 0=256
		WORD	bufferAddress;	// Z80 Buffer address, kept for reference.
		UINT    bytesReadWritten;  // during last read/write
		BYTE	lastResult;		// last error during normal IO.
		BYTE    driveId;		// drive number 0-9, default is 0
		TCHAR	fname[128];		// Fully Parsed Path/Filename.ext
		BYTE	originalFCB[32]; // Contains the unmodified User supplied FCB
		FIL		f;
	} TFILE;

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
	void sanitizeTrs80Path(const char* src, char* dest, BYTE* driveId, int maxLen);

	// Tests
	void run_sanitizer_tests();

	//-----------------------------------------------------------------------------

	// File Methods - Obtain a File Object
	TFILE*	 findOpenFile( BYTE fileId );
	TFILE*   FileOpenByName(const char* pszFileName, BYTE byMode);
	BYTE     FileOpenCount(void);

	// Misc Metods
	// BYTE     FileExists(char* pszFileName);

	// File Methods - OPEN Files
	BYTE     FileIsOpen(const TFILE *fp);
	FRESULT  FileWrite(TFILE* fp, const BYTE* pby, uint32_t nSize);
	FRESULT  FileRead (TFILE* fp, BYTE* pby, uint32_t nSize);
	int      FileReadLine(TFILE* fp, char szLine[], int nMaxLen);
	FRESULT  FileSeek(TFILE* fp, uint64_t nOffset);
	FRESULT  FileClose(TFILE* fp);

	uint32_t FileGetSize(TFILE* fp);
	uint32_t FileGetPos(TFILE* fp);


	// Not currently Used
	//void     FileTruncate(file* fp);
	//void     FileFlush(file* fp);

	//-----------------------------------------------------------------------------

	// Lifecycle methods for the entire Object
	void     FileSystemInit(void);
	void     FileCloseAll(void);

	//-----------------------------------------------------------------------------

	BYTE DirOpen( DIR* dp, const TCHAR* path	);
	BYTE DirRead( DIR* dp, FILINFO* fno );
	BYTE DirFindFirst( DIR* dp,	FILINFO* fno, const TCHAR* path,	const TCHAR* pattern );
	BYTE DirFindNext ( DIR* dp,	FILINFO* fno );
	BYTE DirChange ( const TCHAR* path );
	BYTE DirMake ( const TCHAR* path );

	BYTE FileDelete ( const TCHAR* file );
	BYTE FileRename ( const TCHAR* old, const TCHAR* new );

	BYTE get_normalized_cwd(char *outCwd, size_t outCwdSize, FRESULT *frOut);
	BYTE CurrentDirectory (TCHAR* buff,	UINT len);

	//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif


