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
		BYTE	lrl;			// Logical Record Length (1-255) 0=256
		WORD	bufferAddress;	// Z80 Buffer address, kept for reference.
		UINT    bytesReadWritten;  // during last read/write
		BYTE	lastResult;		// last error during normal IO.
		BYTE    driveId;		// drive number 0-9, default is 0
		TCHAR	fname[33];		// FAT 8.3 Path/Filename.ext
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
	void sanitizeTrs80Path(const char* src, char* dest, uint8_t* driveId, int maxLen);

	// Tests
	void run_sanitizer_tests();

	//-----------------------------------------------------------------------------

	// File Methods - Obtain a File Object
	TFILE*	 findOpenFile( BYTE fileId );
	TFILE*   FileOpenByName(const char* pszFileName, BYTE byMode);
	uint8_t  FileOpenCount(void);

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

	BYTE CurrentDirectory (TCHAR* buff,	UINT len);

	//-----------------------------------------------------------------------------

	// TRS-80 Error Codes - Error ORed with 0x40
	#define TRS_DEVICE_NOT_AVAIL ( 8 | 0x40)
	#define TRS_WRITE_PROTECTED  (15 | 0x40)
	#define TRS_ILL_FILENUMBER   (15 | 0x40)
	#define TRS_ILLEGAL_FILENAME (19 | 0x40)
	#define TRS_FILE_NOT_FOUND   (24 | 0x40)
	#define TRS_EOF_ENCOUNTERED  (28 | 0x40)
	#define TRS_ILLEGAL_DRIVENUM (32 | 0x40)

	#define TRS_PATH_NOT_FOUND   (60 | 0x40)
	#define TRS_ILLEGAL_DOS      (61 | 0x40)
	#define TRS_INTERNAL_DOS     (62 | 0x40)
	#define TRS_UNKNOWN_ERROR    (63 | 0x40)

	// 29 Record number out of range
	// À request to read a record within a random access file (see the @POSN vector) provided



#ifdef __cplusplus
}
#endif

#endif


