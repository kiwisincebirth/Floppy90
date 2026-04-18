
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "defines.h"
#include "file.h"

#include "frehd.h"

#include <stdint.h>
#include <util.h>

#define	AM_HID_SYS (AM_HID + AM_SYS)	/* Hidden System Files */

// ORIGINAL FRE HD Bootloader Block - Z80 Code.
// Byte 0 = Confirmation Code 0xFE
// Byte 1 = CP nn - to skip the next byte being replaced
// Byte 2 = Should be replaced with byte passed in request
byte boot_data_block[] = {
	0xfe,0xfe,0xff,0x18,0x02,0x18,0x19,0x3a,
	0x40,0x38,0xe6,0x80,0xc0,0x11,0x16,0x50,
	0x06,0x09,0xcd,0x1f,0x50,0xc0,0xe9,0x46,
	0x52,0x45,0x48,0x44,0x2e,0x52,0x4f,0x4d,
	0x21,0x00,0x00,0x39,0xe5,0xdd,0xe1,0xeb,
	0x3e,0x03,0xd3,0xc4,0xcd,0xa4,0x50,0x78,
	0x3c,0x3c,0xd3,0xc3,0x0e,0xc2,0x3e,0x01,
	0xed,0x79,0xed,0xb3,0xaf,0xed,0x79,0xcd,
	0xa4,0x50,0xc0,0x0e,0xff,0xcd,0x7f,0x50,
	0x3d,0x20,0x0c,0xcd,0x70,0x50,0xcd,0x7f,
	0x50,0x77,0x23,0x10,0xf9,0x18,0xee,0x3d,
	0x28,0x0b,0xcd,0x7f,0x50,0x47,0xcd,0x7f,
	0x50,0x10,0xfb,0x18,0xe0,0xcd,0x70,0x50,
	0x3e,0x06,0xd3,0xc4,0xcd,0xa4,0x50,0xaf,
	0xc9,0xcd,0x7f,0x50,0x47,0xcd,0x7f,0x50,
	0x6f,0x05,0xcd,0x7f,0x50,0x67,0x05,0xc9,
	0x0c,0x20,0x03,0xcd,0x88,0x50,0xdb,0xc2,
	0xc9,0xaf,0xd3,0xc3,0x3e,0x04,0xd3,0xc4,
	0xcd,0xa4,0x50,0x20,0x05,0xdb,0xcf,0xe6,
	0x08,0xc0,0x3e,0x06,0xd3,0xc4,0xcd,0xa4,
	0x50,0xdd,0xf9,0x3c,0xc9,0xdb,0xcf,0x07,
	0x38,0xfb,0xdb,0xcf,0xe6,0x01,0xc9,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	// and extra line of padding
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

//-----------------------------------------------------------------------------
// SHARED REGISTERS BETWEEN Z80 and BACKEND

// STATUS CONTROL - USED TO SIGNAL OWNERSHIP
static volatile byte fhd_status = 0;

// COMMAND2 - from Z80
static volatile byte  fhd_command2 = 0;

// FILEID - holds current file ID for File IO
static volatile byte  fhd_fileid = 0;

// DATA2 - data buffer shared with Z80 and underlying process
// extra bytes for FCB storage (32) + another 32 for extra safety
static volatile byte __aligned(4) fhd_data2[256 + 64] = {0};

// SIZE2 - size of data in DATA2
static volatile byte  fhd_size2 = 0;

// ERROR2 - Actual Result of underlying FF operation
static volatile byte  fhd_error2 = FR_OK;

//-----------------------------------------------------------------------------

void FreHDInit(void)
{
}
//-----------------------------------------------------------------------------

// Command function type
typedef uint8_t (*command_func_t)(uint8_t);

//
// =================================
// STEPS
// =================================
//
// 1.  After the Command was received
// 2.  After Data was RECEIVED (from z80) providing the SIZE bytes
// 3.  After the Data was SENT  (to  Z80) consuming the SIZE bytes
// 10. Service the command by CORE 0, when BUSY Signalled
//

/*
 * Get version
 * 1. TRS writes COMMAND2 + wait
 * 2. TRS reads DATA2 (up to 6 bytes)
 */
uint8_t __not_in_flash_func(CMD_VER)(uint8_t step) {
	if (step == 1) {
		fhd_data2[0] = VERSION_MAJOR;
		fhd_data2[1] = VERSION_MINOR;
		fhd_data2[2] = 0xA5; // *((UCHAR rom *) 0x6);
		fhd_data2[3] = 0x5A; // *((UCHAR rom *) 0x7);
		fhd_data2[4] = 0; // *((UCHAR rom *) FLASH_CRC_ADDR);
		fhd_data2[5] = 0; // *((UCHAR rom *) FLASH_CRC_ADDR+1);
		fhd_size2 = 6;
		return STATUS_OKDATA;
	}
	return STATUS_OK;
}

// ====================================================================

// Stateful Open File - Legacy (single) File Object
static TFILE* state_openFile;

/*
 * Open file
 * 1. TRS writes COMMAND2
 * 2. TRS writes SIZE2 (1 + strlen(filename))
 * 3. TRS writes DATA2
 *       byte 0 : open options
 *       bytes 1..n : filename
 * 4. TRS waits
 * 5. TRS checks status for error (in ERROR2 if any)
      AND the DATA2 register contains the size of the file just opened.
 */
uint8_t __not_in_flash_func(CMD_OPEN)(uint8_t step) {

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		// close existing file
		FileClose((void *)state_openFile);

		BYTE byMode = fhd_data2[0]; // first byte is the OPEN mode
		char* pszFromZ80 = (char*)&fhd_data2[1]; // next bytes are the file name

		// Sanitised
		char cleanPath[33];
		uint8_t driveNum;

		// 1. Sanitize the string (handles /ext and :D)
		sanitizeTrs80Path(pszFromZ80, cleanPath, &driveNum, 32);

		// 2. Open it (relative to CWD)
		state_openFile = FileOpenByName(cleanPath, byMode);

		if ( state_openFile ) {

			fhd_error2 = state_openFile->lastResult;
			if ( state_openFile->byIsOpen ) {

				FSIZE_t size = FileGetSize(state_openFile);
				printf("\r\nopenFile(%s,%llu) ",cleanPath,size);

				fhd_size2 = sizeof (DWORD); // file size as 4 bytes
				memcpy((void *)fhd_data2, &size, fhd_size2);

				return STATUS_OKDATA;
			} else {
				return STATUS_ERROR; // not opened
			}
		} else {
			fhd_error2 = FR_TOO_MANY_OPEN_FILES; // Or equivalent FatFs error
			printf("\r\nOpenFile(TooManyFiles)");
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * Read file
 * 1. File must have been opened by openfile()
 * 2. TRS sends SIZE2 bytes to read (default 256)
 * 2. TRS sends COMMAND2
 * 3. TRS80 waits
 * 3. TRS checks status. If DRQ set, data was read
 *      DRQ : TRS reads DATA2 (SIZE2 bytes)
 *				SIZE2 may be smaller if last page EOF
 *     !DRQ : done
 *      ERR : error in ERROR2
 */
uint8_t __not_in_flash_func(CMD_READ)(uint8_t step) {

	// after getting Command - Signal Busy and defer processing.
	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		if ( FileIsOpen((void *)state_openFile) ) {

			fhd_error2 = FileRead((void *)state_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == FR_OK) {

				uint32_t actualRead = state_openFile->bytesReadWritten;
				fhd_size2 = (byte)actualRead; // truncated to 8bit 256->0

				if (actualRead > 0) {
					printf("-");
					return STATUS_OKDATA; // got data
				} else {
					return STATUS_OK; // got no data, don't set DRQ
				}
			} else {
				return STATUS_ERROR;
			}
		} else {
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * Write file
 * 1. file must have been opened by openfile()
 * 2. TRS sends COMMAND2
 * 3. TRS sends SIZE2 (if not, default is 256 bytes)
 * 4  TRS sends DATA2 (SIZE2) bytes to write
 * 5. TRS waits
 * 6. TRS checks status
 */
 uint8_t __not_in_flash_func(CMD_WRITE)(uint8_t step) {

 	// Step 1 noting to do at this point, wait for data in Step 2

 	// after getting N Bytes - Signal Busy and defer processing.
 	if (step == 2) return TRS_HARD_BUSY;

 	if (step == 10) {
		printf("\r\nwriteFile() ");

		if ( FileIsOpen((void *)state_openFile) ) {

			fhd_error2 = FileWrite((void *)state_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == FR_OK) {
				if ( state_openFile->bytesReadWritten < fhd_size2) {
					return STATUS_ERROR; // didnt complete write
				} else {
					printf("+");
					return STATUS_OK; // success
				}
			} else {
				return STATUS_ERROR;
			}
		} else {
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		}
 	}

 	return STATUS_OK;
 }

/*
 * Close file
 * 1. file must have been opened by openfile()
 * 2. TRS sends COMMAND2
 * 3. TRS waits
 * 3. TRS checks status for ERR.
 */
uint8_t __not_in_flash_func(CMD_CLOSE)(uint8_t step) {

	if (step == 1) {
		if ( FileIsOpen((void *)state_openFile) ) {
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {

		if (FileIsOpen((void *)state_openFile)) {

			printf("\r\ncloseFile() ");
			fhd_error2 = FileClose((void *)state_openFile);
			if (fhd_error2 != FR_OK) {
				return STATUS_ERROR;
			}
		}
		state_openFile = NULL;
	}
	return STATUS_OK;
}

// ====================================================================

// Directory Stateful Objects
static DIR   state_dir;
static byte  state_dir_open = 0;

/*
 * Open a directory
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2 = strlen(path)
 * 2. TRS sends DATA2 path
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
uint8_t __not_in_flash_func(CMD_OPENDIR)(uint8_t step) {

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {
		printf("\r\nopenDir() ");
		fhd_error2 = DirOpen(&state_dir, (void *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			state_dir_open = 0;
			return STATUS_ERROR;
		} else {
			state_dir_open = 1;
		}
	}

	return STATUS_OK;
}

/*
 * Read a directory
 * 1. opendir (see above)
 * 2. TRS issues COMMAND2 + wait
 * 3. TRS reads status. If HARD_ERR, check ERROR2
 *    If ok, check DRQ.
 *           If set: SIZE2 contains directory info size.
 *                   DATA2 contains directory info data.
 *           If clear: no more entries in this directory.
 */
uint8_t __not_in_flash_func(CMD_READDIR)(uint8_t step)
{
	static FILINFO fno;
	static FILINFO_API api_fno;

	// after getting Command - Signal Busy and defer processing.
	if (step == 1) {

		if (state_dir_open == 0) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		} else {
			// Signal Busy and defer processing.
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {
		if (state_dir_open == 0) {
			// user has not done opendir() before.
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		}
		fhd_error2 = DirRead(&state_dir, &fno);
		if (fhd_error2 != FR_OK) return STATUS_ERROR;
		copyFILEINFO(&api_fno,&fno);

		if (api_fno.fname[0] == 0) {
			// reach end of dir : do not set DRQ
			return STATUS_OK;
		}

		fhd_size2 = (unsigned char) sizeof (FILINFO_API);
		memcpy((void *)fhd_data2, &api_fno, fhd_size2);
		return STATUS_OKDATA;
	}

	return STATUS_OK;
}

// ====================================================================

/*
 * Directory
 * DIR {wildcard}{optional :dirNumber}
 * - specify a wildcard selection (required)
 * - Specify a Drive number 0-9 (0 current dir is the default) - FUTURE
 * - Doest allow path to be specified
 * 1. TRS issues SIZE2 of data2, and COMMAND2 + wait
 * 2. TRS sends DATA2
 *       byte 0 : drive # e.g. 0 thru 9 in binary (FUTURE)
 *       bytes 1..n : wildcard (null terminated string) e.g. *
 * 3. TRS waits
 * 4. TRS reads status.
 * 			If TRS_HARD_ERR set, check ERROR2 and exit
 *			If DATA is available then continue
 *			If No data then exit out
 * 5. TRS Reads SIZE2 bytes. -> goes back to 3
 */
uint8_t __not_in_flash_func(CMD_DIR)(uint8_t step)
{
	static DIR dir;
	static byte dir_open = 0;		// is the dir open
	static byte dir_findfirst = 1;  // do we need to do . find first
	static FILINFO fno;
	static FILINFO_API api_fno;
	static char file_pattern[16]; // the dir pattern buffer

	if (step == 2) {

		// BYTE byDrive = fhd_data2[0]; // todo first byte -> virtual Drive Number (lookup)

		// 1. Inline check is much faster than a function call
		if (fhd_data2[0] == '\0') {
			// 2. Write 32 bits at once: '*' (0x2A), '\0', '\0', '\0'
			// This is a single STR instruction on the Cortex-M33/RISC-V
			*(uint32_t*)file_pattern = 0x0000002A;
		} else {
			// 3. Fast inline copy. __builtin_memcpy tells the compiler
			// to use the fastest hardware-specific move (like LDM/STM)
			__builtin_memcpy(file_pattern, (void *)fhd_data2, 16);
			file_pattern[15] = '\0'; // Hard cap for safety
		}

		dir_findfirst = 1; // need to do the find first

		// Signal Busy and defer processing.
		return TRS_HARD_BUSY;

	} else if (step == 3) {

		dir_findfirst = 0; // need to do a find next

		// Signal Busy and defer processing.
		return TRS_HARD_BUSY;
	}

	if (step == 10) {

		if (dir_findfirst) {

			if (dir_open) { // if dir is open
				// todo tis should be replaced with method in File.h
				f_closedir(&dir); // ensure dir is closed
				dir_open = 0;
			}

			// start the directory scan for first time
			fhd_error2 = DirFindFirst(&dir, &fno, ".", file_pattern);
		} else {
			// read the next directory entry
			fhd_error2 = DirFindNext(&dir, &fno);
		}

		// ensure the entry is a valid item
		while ( fhd_error2==FR_OK && ( fno.fattrib&AM_HID_SYS || fno.fname[0]=='.' ) ) {
			// continue scanning the next file if the file found is hidden/system
			fhd_error2 = DirFindNext(&dir, &fno);

			// todo check for enless loop.
		}

		if (fhd_error2 == FR_OK) {
			dir_open = 1;

			if ( fno.fname[0] != 0) {

				// move the data into the buffer
				copyFILEINFO(&api_fno,&fno);
				fhd_size2 = (unsigned char) sizeof (FILINFO_API);
				memcpy((void *)fhd_data2, &api_fno, fhd_size2);

				return STATUS_OKDATA;

			} else {
				// End of directory or Error
				fhd_size2 = 0; // TELL THE Z80 THERE IS NOTHING TO READ
				if (dir_open) { // if dir is open
					// todo tis should be replaced with method in File.h
					f_closedir(&dir); // ensure dir is closed
					dir_open = 0;
				}
				return STATUS_OK; // signal no data
			}
		} else {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;

}

// ====================================================================
// ====================================================================
// ====================================================================


/*
 * Change directory
 * CD {nameOfDirectory or /} - Changes the current working directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
uint8_t __not_in_flash_func(CMD_CHANGEDIR)(uint8_t step)
{
	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		char* psz = (char*)&fhd_data2[0]; // next bytes are the file name

		printf("\r\nCD(%s) ", psz );
		fhd_error2 = DirChange((const TCHAR *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * Make directory
 * MKDIR {nameOfDirectory} - Creates a named directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
uint8_t __not_in_flash_func(CMD_MAKEEDIR)(uint8_t step)
{
	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		char* psz = (char*)&fhd_data2[0]; // next bytes are the file name

		printf("\r\nMKDIR(%s) ", psz );
		fhd_error2 = DirMake((const TCHAR *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * Delete File or directory
 * DELETE {nameOfDirectory} - delete file or directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
uint8_t __not_in_flash_func(CMD_DELETE)(uint8_t step)
{
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {
		char* psz = (char*)&fhd_data2[0];
		char cleanPath[33];
		uint8_t driveNum;
		bool hadAtLeastOneError = false;

		// Use strtok to split the string by spaces
		// Note: fhd_data2 is modified by strtok, which is fine here.
		char* token = strtok(psz, " ");

		while (token != NULL) {
			// 1. Sanitize the current token (filename)
			sanitizeTrs80Path(token, cleanPath, &driveNum, 32);

			if (cleanPath[0] != '\0') {
				printf("\r\nRM(%s) ", cleanPath);

				// 2. Attempt to delete
				BYTE res = FileDelete(cleanPath);

				if (res != FR_OK) {
					printf("FAILED: %d", res);
					fhd_error2 = res; // Store the last error
					hadAtLeastOneError = true;
				} else {
					printf("OK");
				}
			}

			// 3. Get next filename
			token = strtok(NULL, " ");
		}

		return hadAtLeastOneError ? STATUS_ERROR : STATUS_OK;
	}
	return STATUS_OK;
}

/*
 * Rename a File or directory
 * REN {sourceName} {targetName} - delete file or directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
uint8_t __not_in_flash_func(CMD_RENAME)(uint8_t step)
{
	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {
		char* pszSrc = (char*)&fhd_data2[0];
		char* pszDest = NULL;

		// 1. Find the space between <src> and <dest>
		char* pSpace = strchr(pszSrc, ' ');
		if (!pSpace) {
			fhd_error2 = TRS_ILLEGAL_FILENAME;
			return STATUS_ERROR;
		}

		// 2. Null-terminate the source string at the space
		*pSpace = '\0';

		// 3. The destination starts right after the space
		// Skip any extra spaces just in case the user typed "REN FILE1    FILE2"
		pszDest = pSpace + 1;
		while (*pszDest == ' ') pszDest++;

		// 4. Sanitize both paths
		char cleanSrc[33];
		char cleanDest[33];
		uint8_t driveSrc, driveDest;

		sanitizeTrs80Path(pszSrc, cleanSrc, &driveSrc, 32);
		sanitizeTrs80Path(pszDest, cleanDest, &driveDest, 32);

		printf("\r\nREN (%s) -> (%s)", cleanSrc, cleanDest);

		// 5. Execute Rename
		fhd_error2 = FileRename(cleanSrc, cleanDest);

		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * PWD - display working directory
 *
 * 1. TRS issues COMMAND2
 * 2. TRS waits
 * 3. TRS reads status.
 * 			If TRS_HARD_ERR set, check ERROR2 and exit
 *			If DATA is available then
 * 4. TRS Reads SIZE2 bytes. -> displays to user
 */

uint8_t __not_in_flash_func(CMD_PWD)(uint8_t step)
{
	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {
		// Get the working directory into the buffer
		fhd_error2 = CurrentDirectory((char *)fhd_data2, sizeof(fhd_data2));

		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}

//		// Calculate string length, plush 1 byte terminator
//		fhd_size2 = strlen((char *)fhd_data2);
//		// 2. Overwrite the '\0' with 0x0D (TRS-80 terminator)
//		fhd_data2[fhd_size2] = 0x0D;

		// 1. Find the current length (excluding null)
		uint16_t len = strlen((char *)fhd_data2);

		// Debug: Print the actual string
		printf("\r\nPWD(%s) %i", (char *)&fhd_data2,len+1);

		// 2. Overwrite the '\0' with 0x0D (TRS-80 terminator)
		fhd_data2[len] = 0x0D;

		// 3. Set the size to include that 0x0D
		fhd_size2 = len + 1;

		//  data in the buffer for the Z80 to fetch
		return STATUS_OKDATA;
	}
	return STATUS_OK;
}

// ====================================================================
// ====================================================================
// ====================================================================


/**
; * Pack the parameter registers at the beginning of the FCB
;   * Open Flag's (Existing New, ETC)
;   * B - LRL
;   * DE - FCB Buffer
; * Send the 32 byte FCB block to PICO, OPEN command
*/
#pragma pack(push, 1)		// Prevent compiler padding the structure
typedef struct {
	BYTE    byMode;			/* Open Mode - Defined by FF */
	BYTE    lrl;			/* Logical Record Length 256=0 */
	WORD	buffaddr;		/* address of in memory buffer */
	TCHAR	filename[32];	/* FCB containing filename */
} OPEN_REQUEST;
#pragma pack(pop)

// void __not_in_flash_func(parseRequest)(OPEN_REQUEST* req, TFILE* f) {
// 	// 1. Extract LRL
// 	f->lrl = req->lrl;
//
// 	// 2. Use the core processor
// 	sanitizeTrs80Path((char*)req->fcb, f->fname, &f->driveId, 32);
// }

/**
;  The 32 byte FCB block sent back to Z80
; NOTES:
;  (*) typically refers to a sensible default.
;  Unspecified Bits they can be left as Zero
;  MOB refers to Middle Order Byte in 24 bit values -> LSB, MOB, MSB
;      Least Significant - Middle Order - Most Significant (Bytes)
*/
#pragma pack(push, 1)		// Prevent compiler padding the structure
typedef struct {

	BYTE	byte1;				// Bit 7 = 1 if the file is open. (*)
								// Bit 7 = 0 if the file is NOT open.

	BYTE	byte2;				// Bit 7 = 1 if accessing logical records of size LRL
								// Bit 7 = 0 if accessing full sectors LRL = 0 (256)
								// Bit 6 = 1 EOF to be set only if exceed file size - random access
								// Bit 6 = 0 EOF to be set after every write - sequential access (*)
								// Bit 5 = 1 the Z80 sector buffer is not populated (*)
								// Bit 5 = 0 the Z80 sector buffer is populated
								// Bit 4 = 1 the Z80 sector buffer contents are Dirty. Changed since last read.
								// Bit 4 = 0 the Z80 sector buffer contents are Clean. unchanged (*)

	BYTE	byte3;				// 02 Reserved Flags / Future Use
	WORD	buffaddr;			// 03 16 bit Z80 main data buffer address. copied From request
	BYTE	nextLSB;			// 05 LSB of the next byte (initially 0) in file to access.
	BYTE    driveId;		    // 06 drive number 0-9, where the file was opened.
	BYTE    fileId;				// 07 unique File identifier. used to access the File
	BYTE	eofLSB;				// 08 LSB of EOF of the File. File Size.
	BYTE	lrl;				// 09 Logical Record Length, zero means 256 full sector access
	BYTE	nextMOB;			// 0A MOB of the next byte (initially 0) in file to access.
	BYTE	nextMSB;			// 0B MSB of the next byte (initially 0) in file to access.
	BYTE	eofMOB;				// 0C MOB of EOF of the File. File Size.
	BYTE	eofMSB;				// 0D MSB of EOF of the File. File Size.

	TCHAR	trailer[18];		// trailing data must be filled with $FF (*)
} FCB;
#pragma pack(pop)

/**
 * Used to populate an FCB from an open File
 * @param fp
 * @param fcb
 */
void FileToFCB(TFILE* fp, FCB* fcb) {
	if (!fp || !fcb ) return;

	// Init structure: Zero out, then set the $FF trailer
	memset(fcb, 0, sizeof(FCB));

	// byte1: Bit 7 = 1 (Open Status)
	fcb->byte1 |= (fp->byIsOpen) ? 0x80 : 0x00;

	// 3. byte2
	// Bit 7 = 1 if accessing logical records of size LRL
	// Bit 7 = 0 if accessing full sectors LRL = 0 (256)
	fcb->byte2 |= (fp->lrl == 0) ? 0x00: 0x80;
	// Bit 5 = 1 (Z80 sector buffer is not populated)
	fcb->byte2 |= 0x20;

	// other data
	fcb->buffaddr = fp->bufferAddress;
	fcb->lrl      = fp->lrl;

	// 5. System Identifiers for mapping back to RP2350 objects
	fcb->driveId  = fp->driveId;
	fcb->fileId   = fp->fileId;

	// Extract Next Pointer (24-bit)
	uint32_t nextPos = FileGetPos(fp);
	fcb->nextLSB = (BYTE)(nextPos & 0xFF);
	fcb->nextMOB = (BYTE)((nextPos >> 8) & 0xFF);
	fcb->nextMSB = (BYTE)((nextPos >> 16) & 0xFF);

	// Extract EOF/Size (24-bit)
	uint32_t fileSize = FileGetSize(fp);
	fcb->eofLSB = (BYTE)(fileSize & 0xFF);
	fcb->eofMOB = (BYTE)((fileSize >> 8) & 0xFF);
	fcb->eofMSB = (BYTE)((fileSize >> 16) & 0xFF);

	// Trailing Data
	memset(fcb->trailer, 0xFF, sizeof(fcb->trailer));
}

// ====================================================================

/**
 * Dumps a memory buffer to the serial console in a formatted hex view.
 * @param title A string title to print before the dump (e.g., "DATA2 FCB")
 * @param buf   Pointer to the data
 * @param len   Number of bytes to dump (e.g., 32)
 */
void debug_dump_buffer(const char* title, const uint8_t* buf, size_t len) {
	printf("\r\n--- %s (%zu bytes) ---\n", title, len);
	// printf("Offset  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
	// printf("------------------------------------------------------------------\n");

	for (size_t i = 0; i < len; i += 16) {
		// Print the offset
		printf("%04X    ", (unsigned int)i);

		// Print Hex values
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				printf("%02X ", buf[i + j]);
			} else {
				printf("   "); // Padding for partial rows
			}
			if (j == 7) printf(" "); // Visual split at 8 bytes
		}

		printf(" ");

		// Print ASCII values
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				uint8_t c = buf[i + j];
				// Check if character is printable, else print a dot
				printf("%c", isprint(c) ? c : '.');
			}
			if (j == 7) printf(" ");
		}
		printf("\n");
	}
	// printf("------------------------------------------------------------------\n");
}

/*
 * FILE -> Open
 * 1. TRS writes COMMAND2, SIZE2 set to 36
 * 2. TRS writes DATA2 (36 bytes)
 *       byte 0 : open options
 *       byte 1 : logical record lenght 0=256
 *       byte 2,3 : location of Buffer in TRS memory
 *       bytes 4..36 : fcb data from TRS
 * 3. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 * 5. DATA 2 contains
 *       bytes 0..31 : Actual FCB data
 * 6. FILEID contains the Id for Open File
 */
uint8_t __not_in_flash_func(CMD_FILE_OPEN)(uint8_t step) {

	if (step == 1) { // SET the number of bytes upfront.
		fhd_size2 = 32 + 4; // the fcb and 4 bytes "OPEN_REQUEST"
	}

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		// debug_dump_buffer("Open Request", (uint8_t*)fhd_data2, 32);

		// cast copy the request data into OPEN_REQUEST
		OPEN_REQUEST* openRequest = (OPEN_REQUEST*)fhd_data2;

		// Sanitised
		char cleanName[33];
		uint8_t driveNum;

		// 1. Sanitize the string (handles /ext and :D)
		sanitizeTrs80Path((char*)openRequest->filename, cleanName, &driveNum, 32);

		printf("\r\nfileOpen(%s) ",cleanName);

		// the main open command
		TFILE *file = FileOpenByName(cleanName, openRequest->byMode);

		if ( file == NULL ) {
			fhd_error2 = FR_TOO_MANY_OPEN_FILES; // Or equivalent FatFs error
			printf("Err=TooManyFiles ");
			return STATUS_ERROR;
		}

		fhd_error2 = file->lastResult;

		if (fhd_error2 == FR_OK) {

			printf("Id=%d Size=%lu ",file->fileId,FileGetSize(file));

			// copy data from request into the file object
			file->lrl = openRequest->lrl;
			file->bufferAddress = openRequest->buffaddr;
			file->driveId = driveNum; // extracted from the filename

			// sets all infor into FCB for return
			FCB fcbData;
			FileToFCB(file, &fcbData);

			// copy FCB into Data and set size to 32
			fhd_size2 = sizeof (FCB);
			memcpy((void *)fhd_data2, &fcbData, fhd_size2);

			// debug_dump_buffer("Open Response", (uint8_t*)fhd_data2, 32);

			// and set the current File Id reg
			fhd_fileid = file->fileId; // Set File ID into Register

			// Check if file is -> New / Empty (0 bytes)
			if (FileGetSize(file) == 0) {
				// Set a bit in our Status register (e.g., Bit 1)
				return STATUS_OKDATA | TRS_HARD_ZERO;
			} else {
				return STATUS_OKDATA;
			}
		} else {
			printf ("Err=%d ",fhd_error2);
			return STATUS_ERROR; // not opened
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Close
 * 1. TRS80 writes FILEID with Id for Open File
 * 1. TRS writes COMMAND2
 * 2. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 */
uint8_t __not_in_flash_func(CMD_FILE_CLOSE)(uint8_t step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		printf("\r\nfileClose(%d) ",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
			return STATUS_ERROR;
		}

		printf("Name=%s ) ",file->fname);

		fhd_error2 = FileClose(file);

		if (fhd_error2 == FR_OK) {
			fhd_fileid = 0 ;

			// Copy the name bytes directly from our TFILE storage
			memcpy((char *)fhd_data2, file->fname, 32);
			fhd_size2 = 0x20;

			// debug_dump_buffer("File Close Data", (uint8_t*)fhd_data2, 32);

			return STATUS_OK;
		} else {
			printf ("Err=%d ",fhd_error2);
			return STATUS_ERROR; // not opened
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Kill
 * 1. TRS80 writes FILEID with Id for Open File
 * 1. TRS writes COMMAND2
 * 2. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 */
uint8_t __not_in_flash_func(CMD_FILE_KILL)(uint8_t step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		printf("\r\nfileKill(%d) ",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
			return STATUS_ERROR;
		}

		printf("\r\nfileClose(%s) ",file->fname);

		FileClose(file); // just ignore any error

		printf("& Kill ");

		// the filename is still present.
		fhd_error2 = FileDelete(file->fname);

		if (fhd_error2 == FR_OK) {
			return STATUS_OK;
		} else {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Read
 * 1. TRS80 writes FILEID with Id for Open File
 * 2. TRS writes COMMAND2
 * 3. TRS waits
 * 4. TRS checks status.
 *      ERR : error in ERROR2 - including EOF Error, if no bytes
 *      DRQ : should be true otherwise
 * 5. TRS reads DATA2 (SIZE2 bytes) may be smaller if last page
 * Noting the Bytes read are defined by the LRL defined at OPEN
 */
uint8_t __not_in_flash_func(CMD_FILE_READ)(uint8_t step) {

//	printf("\r\nfR()");

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		// printf("\r\nfileRead(%d)",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileRead(%d) Err=IllegalFileId) ",fhd_fileid);
			return STATUS_ERROR;
		}

		// actually read the data into the target data array starting at
		fhd_error2 = FileRead((void *)file, (void *)&fhd_data2, file->lrl);

		if (fhd_error2 == FR_OK) {

			uint32_t actualRead = file->bytesReadWritten;

			if ( actualRead > 0 ) {

				// printf("\r\nfileRead(Bytes Read=%lu)",actualRead);
				// debug_dump_buffer("Read Data", (uint8_t*)fhd_data2, 256);

				printf("-");

				// set the data length
				fhd_size2 = actualRead; // 8bit LRL where 256->0

				return STATUS_OKDATA; // got data
			} else {
				fhd_error2 = TRS_EOF_ENCOUNTERED;
				return STATUS_ERROR;
			}
		} else {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Write
 * 1. File must have been opened by openfile()
 * 2. TRS writes COMMAND2
 * 3. TRS optionally sets SIZE2 (if not, default is LRL)
 * 4  TRS sends DATA2 (SIZE2) bytes to write
 * 5. TRS waits
 * 6. TRS checks status.
 *      ERR : error in ERROR2
 */
uint8_t __not_in_flash_func(CMD_FILE_WRITE)(uint8_t step) {

//	printf("\r\nfW()");

	if (step == 1) {
		// find the file object, using "fhd_fileid" set in IO Port
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		}

		// set the default size based on files LRL
		fhd_size2 = file->lrl;
		// printf("\r\nfileWrite(Bytes=%d)", fhd_size2);

		return STATUS_OK;
	}

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		// printf("\r\nWriteFile(%d)",fhd_fileid);

		// find the file object, using "fhd_fileid" passed in IO Port
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileWrite(%d) Err=IllegalFileId) ",fhd_fileid);
			return STATUS_ERROR;
		}

		uint16_t expectedWrite = fhd_size2 == 0 ? 256 : fhd_size2;
		// debug_dump_buffer("Write Data", (uint8_t*)fhd_data2, expectedWrite);

		// actually write the data from the Z80 DATA buffer TO the file
		fhd_error2 = FileWrite((void *)file, (void *)&fhd_data2, expectedWrite);

		if (fhd_error2 == FR_OK) {

			// printf("\r\nfileWrite(Bytes Written=%i)",file->bytesReadWritten);
			uint32_t actualWrite = file->bytesReadWritten;

			if (actualWrite == expectedWrite) {
				printf("+");
				return STATUS_OK; // Write success
			} else {
				fhd_error2 = TRS_UNKNOWN_ERROR; // todo Map to Better Error
				return STATUS_ERROR;
			}
		} else {
			printf("\r\nfileWrite(%d) Err=%d ",fhd_fileid,fhd_error2);
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/**
 * Generic Position Handler
 * Handles REW (SOF), POS (position), BACK, and EOF
 */
uint8_t __not_in_flash_func(CMD_FILE_POSITION)(uint8_t step) {

	if (step == 1) {
		if (fhd_command2==TRS_FILE_POS) {
			// POS Requires Z80 to send 2 byte Record Number
			fhd_size2 = 2;
			return STATUS_OK;
		} else {
			// all other commands are parameterless
			return TRS_HARD_BUSY;
		}
	}

	if (step == 2) {
		if (fhd_command2==TRS_FILE_POS) {
			return TRS_HARD_BUSY;
		}
	}

    if (step == 10) {

		printf("\r\nfilePOS(%i) ",fhd_fileid);

    	TFILE* file = findOpenFile(fhd_fileid);
        if (!file) {
            fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
            return STATUS_ERROR;
        }

    	FSIZE_t newPos = 0;

        switch (fhd_command2) {
            case TRS_FILE_SOF:
                newPos = 0;
                break;
            case TRS_FILE_POS: {
		            // Get the Logical Record Number from the buffer
            		uint16_t recordNum = *((uint16_t*)fhd_data2);
            		uint32_t recordSize = (file->lrl == 0) ? 256 : file->lrl;
            		newPos = (FSIZE_t)recordNum * recordSize;
				}
        		break;
            case TRS_FILE_BACK:
                // Calculate back one record, floor at 0
                if (f_tell(&file->f) >= file->lrl) {
                    newPos = f_tell(&file->f) - (file->lrl == 0 ? 256 : file->lrl);
                } else {
                    newPos = 0;
                }
                break;
            case TRS_FILE_EOF:
                newPos = f_size(&file->f);
                break;
        }

        fhd_error2 = FileSeek(file, newPos);

        if (fhd_error2 == FR_OK) {

	    	printf("Pos=%llu ",newPos);

            // Prepare the 32-byte FCB for the Z80 to pull back
            FCB fcbData;
            FileToFCB(file, &fcbData);

        	// copy a subset of FCB data into the man data buffer
        	fhd_size2 = 9; // copy 9 bytes, starting at the fifth byte
        	memcpy((void *)fhd_data2, &((uint8_t*)&fcbData)[5], fhd_size2);

            return STATUS_OKDATA;
        } else {
        	printf("Err=%d ",fhd_error2);
        	return STATUS_ERROR;
        }
    }
    return STATUS_OK;
}

/*
 * FILE -> Read FCB
 * 1. TRS80 writes FILEID with Id for Open File
 * 2. TRS writes COMMAND2
 * 3. TRS waits
 * 4. TRS checks status.
 *      ERR : error in ERROR2 - including EOF Error, if no bytes
 *      DRQ : should be true otherwise
 * 5. TRS reads DATA2 FCB, should be 16 bytes
 */
uint8_t __not_in_flash_func(CMD_FILE_FCB)(uint8_t step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileFCB(%d) Err=IllegalFileId", fhd_fileid);
			return STATUS_ERROR;
		}

		// printf("\r\nfileFCB(%s)",file->fname);

        // Prepare the 32-byte FCB for the Z80 to pull back
		FCB fcbData;
		FileToFCB(file, &fcbData);

		// copy a subset of FCB data into the man data buffer
		fhd_size2 = 9; // copy 9 bytes, starting at the fifth byte
		memcpy((void *)fhd_data2, &((uint8_t*)&fcbData)[5], fhd_size2);

		// debug_dump_buffer("FCB Response", (uint8_t*)fhd_data2, 32);

		return STATUS_OKDATA;
	}
	return STATUS_OK;
}

//
// Command Listing - NOTE We need this to be IN SRAM
//

const command_func_t __not_in_flash("frehd") command_table[MAX_COMMANDS] = {

	// Standard FreHD Commands
	[TRS_EXTRA_VERSION]		= CMD_VER,
	[TRS_EXTRA_OPENFILE]	= CMD_OPEN,
	[TRS_EXTRA_READFILE]	= CMD_READ,
	[TRS_EXTRA_WRITEFILE]	= CMD_WRITE,
	[TRS_EXTRA_CLOSEFILE]	= CMD_CLOSE,
	[TRS_EXTRA_OPENDIR]		= CMD_OPENDIR,
	[TRS_EXTRA_READDIR]		= CMD_READDIR,

	// General Purpose  ( $10 - $13 )
	[TRS_NEWER_DIR]			= CMD_DIR,

	// File Control ( $14 - $17 )
	[TRS_FILE_OPEN]			= CMD_FILE_OPEN,
	[TRS_FILE_CLOSE]		= CMD_FILE_CLOSE,
	[TRS_FILE_KILL]			= CMD_FILE_KILL,

	// Directory Command ( $18 - $1F )
	[TRS_NEWER_CHANGEDIR]	= CMD_CHANGEDIR,
	// [TRS_NEWER_COPYFILE] = CMD_COPYFILE,
	[TRS_NEWER_DELETE]		= CMD_DELETE,
	[TRS_NEWER_MAKEDIR]		= CMD_MAKEEDIR,
	[TRS_NEWER_RENAME]		= CMD_RENAME,
	[TRS_NEWER_PWD]			= CMD_PWD,

	// Fle Handling Commands ( $20 - $2F )
	[TRS_FILE_READ]			= CMD_FILE_READ,
	[TRS_FILE_WRITE]		= CMD_FILE_WRITE,
	[TRS_FILE_SOF]			= CMD_FILE_POSITION,
	[TRS_FILE_POS]			= CMD_FILE_POSITION,
	[TRS_FILE_BACK]			= CMD_FILE_POSITION,
	[TRS_FILE_EOF]			= CMD_FILE_POSITION,
	[TRS_FILE_FCB]			= CMD_FILE_FCB

};
//-----------------------------------------------------------------------------

void __not_in_flash_func(frehd_command_dispatch)(uint8_t step) {

	// reject if out of bounds
	if (__builtin_expect(fhd_command2 >= MAX_COMMANDS, 0)) {
    	// printf("\r\nUnknown Command %i",fhd_command2);
		return;
	}

    command_func_t func = command_table[fhd_command2]; // get the command
    if (func == NULL) {
    	// printf("\r\nUnknown Command %i",fhd_command2);
    	return; // if NO command, then nothing to do
    }

    // Actually EXECUTE the command
    uint8_t status_result = func(step);

	// Ensure all memory writes before status changes as
	// this is a handoff between processing threads
	__DSB(); // Data Sync Barrier

	// if the process Z80 Side signals BUSY then we set this State
	// otherwise if any other status we must ensure BUST is NOT Set
	if (status_result == TRS_HARD_BUSY) {
		fhd_status = TRS_HARD_BUSY;
	} else {
	    fhd_status = status_result & ~TRS_HARD_BUSY;
		if (fhd_status == STATUS_ERROR) {
			fhd_size2 = 0;
		}
	}

}

//-----------------------------------------------------------------------------
// ONLY USED by Z80 Core

// BOOT loader Flags used by Z80 Access
static bool  fhd_bootMode = false;
static int	 fhd_bootIndex = 0;

// Index pointer into DATA2 for Z80 R/W
static byte  fhd_data2Index = 0;

//-----------------------------------------------------------------------------
void __not_in_flash_func(frehd_port_out)(word addr, byte data)
{
	switch (addr) {
		case DATA2:
			fhd_data2[fhd_data2Index++] = data; // store the data byte into the buffer
			// fhd_data2[fhd_data2Index  ] = 0; 	// precaution, puts a null after last byte
			// printf(" %d",fhd_data2Index);
			// STEP 2 : When all (size2) bytes have been received dispatch
			if (fhd_data2Index==fhd_size2) frehd_command_dispatch(2);

			break;
		case SIZE2:
			fhd_size2 = data;
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			break;
		case COMMAND2:
			fhd_command2 = data;
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			fhd_bootMode = false; // boot mode should be disabled

			// STEP 1 : After receiving the command
			frehd_command_dispatch(1);

			break;
		case FILEID2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			fhd_fileid = data;
			break;

		case WRITEROM: // (OUT on ERROR2) Bootloader Initiate Command
			if (g_byEnableFreHDBoot) { // only if enabled
				fhd_bootMode = true; // boot loader Mode
				fhd_bootIndex = 0; // reset the data pointer
				boot_data_block[2] = data; // byte from out command.
			}
		 	break;
	}
}

//-----------------------------------------------------------------------------
byte __not_in_flash_func(frehd_port_in)(word addr)
{
    byte data = 0xFF;

	switch (addr)
	{
		case DATA2:
			data = fhd_data2[fhd_data2Index++];
			if (fhd_data2Index==fhd_size2) {   // if sent SIZE bytes out of the buffer
				frehd_command_dispatch(3); // STEP 3 : When all bytes have been Sent Back
			}
			break;
		case SIZE2:
			// reading the Size would indicate we are about to read the BYTES
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_size2; // return the size2 register
			break;
		case READROM: // (In on COMMAND2) Bootloader Data Read
			if (fhd_bootMode) {
				// if correct mode then return data
				data = boot_data_block[fhd_bootIndex++];
			}
			break;
		case ERROR2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_error2;
			break;
		case STATUS:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_status;
			break;
		case FILEID2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_fileid;
			break;
	}

	return data;
}

//-----------------------------------------------------------------------------

void __not_in_flash_func(ServiceFreHdStateMachine)(void) {

	// IF Busy Flag Set then we need process a command
	if ( fhd_status == TRS_HARD_BUSY ) {

		// STEP 10; Service the command by CORE 0
		frehd_command_dispatch(10);
	}

}

