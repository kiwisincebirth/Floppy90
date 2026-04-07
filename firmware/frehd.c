
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

// #include "sd_core.h"
#include "defines.h"
#include "file.h"
// #include "system.h"
// #include "string.h"

#include "frehd.h"

#include <stdint.h>
#include <util.h>

#define STATUS_ERROR (TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR)
#define STATUS_OK (TRS_HARD_READY | TRS_HARD_SEEKDONE )
#define STATUS_OKDATA (TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_DRQ)

#define	AM_HID_SYS AM_HID + AM_SYS	/* Hidden System Files */

// FRE HD Bootloader Block - Z80 Code.
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

// DATA2 - data buffer shared with Z80 and underlying process
static volatile byte __aligned(4) fhd_data2[256 + 4] = {0};

// SIZE2 - size of data in DATA2
static volatile byte  fhd_size2 = 0;

// ERROR2 - Actual Result of underlying FF operation
static volatile byte  fhd_error2 = FR_OK;

//-----------------------------------------------------------------------------

// FreHD Open File
static volatile file* fhd_openFile;

// DIRECTORY
static DIR  state_dir;
static byte  state_dir_open = 0;

//-----------------------------------------------------------------------------
void FreHDInit(void)
{
}

// --- Command table ---
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

//			// code to check the filename
//        	int   nIndex = 0;
//        	int   nSize =  strlen(psz);
//        	while ((*psz != 0) && (*psz != ',') && (nIndex < nSize))
//        	{
//        		if (*psz == '/') { // replace / with .
//        			*psz = '.';
//        		}
//        		if (*psz == ':') { // strip off drive specification
//        			*psz = 0;
//        		}
//        		++psz;
//        		++nIndex;
//        	}

		// close existing file
		FileClose((void *)fhd_openFile);

		BYTE byMode = fhd_data2[0]; // first byte is the OPEN mode
		char* psz = (char*)&fhd_data2[1]; // next bytes are the file name

		// the main open command
		fhd_openFile = FileOpen(psz, byMode);

		if (fhd_openFile != NULL) {
			fhd_error2 = fhd_openFile->lastResult;
			if ( fhd_openFile->byIsOpen ) {

				printf("\r\nopenFile(%s,%llu) ",psz,fhd_openFile->size);

				fhd_size2 = sizeof (DWORD); // file size as 4 bytes
				memcpy((void *)fhd_data2, (void *)&fhd_openFile->size, fhd_size2);

				return STATUS_OKDATA;
			}
		}
		fhd_size2 = 0;
		return STATUS_ERROR; // not opened
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
	if (step == 1) {

		if ( ! FileIsOpen((void *)fhd_openFile) ) {
			fhd_error2 = FR_NO_FILE;
			return STATUS_ERROR;
		} else {
			// Signal Busy and defer processing.
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {

		if ( FileIsOpen((void *)fhd_openFile) ) {

			fhd_error2 = FileRead((void *)fhd_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == FR_OK) {

				uint32_t actualRead = fhd_openFile->bytesReadWritten;
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
			fhd_error2 = FR_NO_FILE;
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

 	if (step == 1) {
 		if ( ! FileIsOpen((void *)fhd_openFile) ) {
 			fhd_error2 = FR_NO_FILE;
 			return STATUS_ERROR;
 		}
 	}

 	// after getting N Bytes - Signal Busy and defer processing.
 	if (step == 2) return TRS_HARD_BUSY;

 	if (step == 10) {
		printf("\r\nwriteFile() ");

		if ( FileIsOpen((void *)fhd_openFile) ) {

			fhd_error2 = FileWrite((void *)fhd_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == FR_OK) {
				if ( fhd_openFile->bytesReadWritten < fhd_size2) {
					return STATUS_ERROR; // didnt complete write
				} else {
					printf("+");
					return STATUS_OK; // success
				}
			} else {
				return STATUS_ERROR;
			}
		} else {
			fhd_error2 = FR_NO_FILE;
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
		if ( FileIsOpen((void *)fhd_openFile) ) {
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {

		if (FileIsOpen((void *)fhd_openFile)) {

			printf("\r\ncloseFile() ");
			FileClose((void *)fhd_openFile);
			fhd_error2 = fhd_openFile->lastResult;
			if (fhd_error2 != FR_OK) {
				return STATUS_ERROR;
			}
		}
		fhd_openFile = NULL;
	}
	return STATUS_OK;
}


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
		fhd_error2 = f_opendir(&state_dir, (void *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			fhd_size2 = 0;
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
			fhd_error2 = FR_NO_FILE;
			return STATUS_ERROR;
		} else {
			// Signal Busy and defer processing.
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {
		if (state_dir_open == 0) {
			// user has not done opendir() before.
			fhd_error2 = FR_NO_PATH;
			return STATUS_ERROR;
		}
		fhd_error2 = f_readdir(&state_dir, &fno);
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

	// todo rewrite this
	// todo rewrite this
	// todo rewrite this

	static DIR dir;
	static byte dir_open = 0;
	static FILINFO fno;
	static FILINFO_API api_fno;
	static char file_pattern[16]; // the dir pattern buffer

	if (step == 1) {
		if (dir_open) { // if dir is open
			f_closedir(&dir); // ensure dir is closed
			dir_open = 0;
		}
		return STATUS_OK;
	}

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

		// if (fhd_data2[0] != '\0') {
		// 	strncpy(file_pattern, (char*)fhd_data2, sizeof(file_pattern) - 1);
		// 	file_pattern[sizeof(file_pattern) - 1] = '\0';
		// } else {
		// 	file_pattern[0] = '*';
		// 	file_pattern[1] = '\0';
		// }

		printf("\r\nDIR(%s) ", file_pattern);

		// start the directory scan for first time
		fhd_error2 = f_findfirst(&dir, &fno, ".", file_pattern);
		// fhd_error2 = f_findfirst(&dir, &fno, "", psz); // todo pattern should be provided

	} else if (step == 3) {
		// read the next directory entry
		fhd_error2 = f_findnext(&dir, &fno);
	}

	// ensure the entry is a valid item
	while ( fhd_error2==FR_OK && ( fno.fattrib&AM_HID_SYS || fno.fname[0]=='.' ) ) {
		// continue scanning the next file if the file found is hidden/system
		fhd_error2 = f_findnext(&dir, &fno);
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
				f_closedir(&dir); // ensure dir is closed
				dir_open = 0;
			}
			return STATUS_OK; // signal no data
		}
	} else {
		return STATUS_ERROR;
	}
}

/*
 *
 * Change directory
 * CD {nameOfDirectory or /} - Sets an implicitly named directory as :0
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
		fhd_error2 = f_chdir((const TCHAR *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
*MKDIR {nameOfDirectory}
*COPY file/ext:N file/ext:N
*RENAME file/ext:N file/ext:N
*DEL file/ext:N
*/

// Command Listing - NOTE We need this to be IN SRAM
const command_func_t __not_in_flash("frehd") command_table[MAX_COMMANDS] = {
	[TRS_EXTRA_VERSION]   = CMD_VER,
    [TRS_EXTRA_OPENFILE]  = CMD_OPEN,
    [TRS_EXTRA_READFILE]  = CMD_READ,
    [TRS_EXTRA_WRITEFILE] = CMD_WRITE,
    [TRS_EXTRA_CLOSEFILE] = CMD_CLOSE,
    [TRS_EXTRA_OPENDIR]   = CMD_OPENDIR,
    [TRS_EXTRA_READDIR]   = CMD_READDIR,

    // Newer Commands not supported on FreHD
    // [TRS_NEWER_CLOSEDIR]  = // $10
    [TRS_NEWER_DIR]		  = CMD_DIR,      // $11
    [TRS_NEWER_CHANGEDIR] = CMD_CHANGEDIR // $19
    // [TRS_NEWER_MAKEDIR] =  // $1
    // [TRS_NEWER_RENAME] =  // $1
    // [TRS_NEWER_DELETE] =  // $1
};

//-----------------------------------------------------------------------------

// todo the functional dispatch should be delegate to a state machine on the other thread.
// todo and the status should be set as BUSY, until the main thread then completes
// todo where the main thread can then update the main thread.
// todo in some respects this may not add any value since Z0 code will just
// todo poll the busy flag until it is complete.

void __not_in_flash_func(frehd_command_dispatch)(uint8_t step) {

	// reject if out of bounds
	if (__builtin_expect(fhd_command2 >= MAX_COMMANDS, 0)) return;

    command_func_t func = command_table[fhd_command2]; // get the command
    if (func == NULL) return; // if NO command, then nothing to do

    // Actually EXECUTE the command
    uint8_t status_result = func(step);

	// Ensure all memory writes before status
	__DSB(); // Data Sync Barrier

	// if the process Z80 Side signals BUSY then we set this State
	// otherwise if any other status we must ensure BUST is NOT Set
	if (status_result == TRS_HARD_BUSY) {
		fhd_status = TRS_HARD_BUSY;
	} else {
	    fhd_status = status_result & ~TRS_HARD_BUSY;
    }
}

//-----------------------------------------------------------------------------
// ONLY USED by Z80 Core

// BOOT loader Flags used by Z80 Access
static bool  fhd_bootMode = false;
static int	 fhd_bootIndex = 0;

// Index pointer into DATA2 for Z80 R/W
static byte fhd_data2Index = 0;

//-----------------------------------------------------------------------------
void __not_in_flash_func(frehd_port_out)(word addr, byte data)
{
	switch (addr)
	{
		case DATA2:
			fhd_data2[fhd_data2Index++] = data; // store the data byte into the buffer
			fhd_data2[fhd_data2Index  ] = 0; 	// precaution, puts a null after last byte

			// STEP 2 : When all (size2) bytes have been received dispatch
			if (fhd_data2Index==fhd_size2) frehd_command_dispatch(2);

			break;
		case SIZE2:
			fhd_size2 = data;

			// since Z80 provide size, assume z80 will now write to the DATA buffer
			fhd_data2Index = 0; // so set the pointer back to beginning of data buffer

			break;
		case COMMAND2:
			fhd_command2 = data;

 			// Issuing a command is a primary action and data buffer may come after this
			fhd_data2Index = 0; // so set the pointer back to beginning of data buffer
			fhd_bootMode = false; // boot mode should be disabled

			frehd_command_dispatch(1);	// STEP 1 : After receiving the command

			break;

			// todo could make this configurable
		// case WRITEROM: // (read ERROR2) Bootloader Initiate Command
		// 	fhd_bootMode = true; // boot loader Mode
		// 	fhd_bootIndex = 0; // reset the data pointer
		// 	boot_data_block[2] = data; // byte from out command.
		// 	break;
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
			fhd_data2Index = 0; // so set the pointer back to beginning of data buffer
			data = fhd_size2; // return the size2 register
			break;
		case READROM: // (write COMMAND2) Bootloader Data Read
			if (fhd_bootMode) {
				// if correct mode then return data
				data = boot_data_block[fhd_bootIndex++];
			}
			break;
		case ERROR2:
			data = fhd_error2;
			break;
		case STATUS:
			data = fhd_status;
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
