
#ifndef _H_DEFINES_
#define _H_DEFINES_

#define VERSION_MAJOR 0
#define VERSION_MINOR 4
#define VERSION_STRING "0.4.0 (beta)"

#pragma GCC optimize ("O3")

#include <ff.h>
#include "file.h"

typedef unsigned long      	uint32_t;
typedef unsigned long long 	uint64_t;

typedef unsigned char		byte;
typedef unsigned short     	word;

#define SizeOfArray(x) (sizeof(x) / sizeof(x[0]))

#define INT_PIN       0
#define WAIT_PIN      1
#define CLK_SCLK      2
#define CMD_MOSI      3

#define DAT0_MISO     4
#define DAT3_CS       5
#define LED_PIN       6

#define ADDRL_OE_PIN  7
#define DATAB_OE_PIN  8
#define ADDRH_OE_PIN  9

#define IN_PIN        10
#define RD_PIN        11
#define WR_PIN        12
#define OUT_PIN       13
#define MREQ_PIN      14

#define D0_PIN        15
#define D1_PIN        16
#define D2_PIN        17
#define D3_PIN        18

#define D4_PIN        19
#define D5_PIN        20
#define D6_PIN        21
#define D7_PIN        22

#define DIR_PIN       26
#define SYSRES_PIN    27
#define CD_PIN        28

// Bit  Net      Input    Hex
//             (1=input)
// 31    -         0
// 30    -         0
// 29    -         0
// 28    CD        1       1     0
//
// 27    SYSRES    1
// 26    DIR       0
// 25    -         0
// 24    -         0       8     0
//
// 23    -         0
// 22    D7        1
// 21    D6        1
// 20    D5        1       7     7
//
// 19    D4        1
// 18    D3        1
// 17    D2        1
// 16    D1        1       F     F
//
// 15    D0        1
// 14    RAS       1
// 13    OUT       1
// 12    WR        1       F     8
//
// 11    RD        1
// 10    IN        1
//  9    ADDRH     0
//  8    DATAB     0       C     0
//
//  7    ADDRL     0
//  6    LED       0
//  5    DAT3_CS   0
//  4    DAT0_MISO 0       0     0
//
//  3    CMD_MOSI  0
//  2    CLK_SCLK  0
//  1    WAIT      0
//  0    INT       0       0     0

#define GPIO_IN_MASK  0x187FFC00
#define DATA_BUS_MASK 0x007F8000

// #define A_TO_B_DIR 1
// #define B_TO_A_DIR 0

///////////////////////////////////////////////////////////////////////////////////////////////////

// Low Power Timer Overflow Rates in Hz
#define TIMER_OVERFLOW_RATE 2000

#define LED_ON_TIME_MS      3000

///////////////////////////////////////////////////////////////////////////////////////////////////
// generic defines/equates

#ifndef TRUE
	#define FALSE	0
	#define TRUE	1
#endif

#ifndef true
	#define false	0
	#define true	1
#endif

#ifndef MAX_PATH
  #define MAX_PATH 64
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// CLI Command Line

// columns to display for DIR command
#define DIR_COLUMNS 3

///////////////////////////////////////////////////////////////////////////////////////////////////
// enumerations

enum {
  opRead = 0,
  opWrite,
  opNop
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Error Message Handling

// Success No Error TRS-OK and FR_OK (FatFs) are the Same
#define TRS_OK FR_OK
// TRS-80 Error Codes - Error ORed with 0x40
#define TRS_DEVICE_NOT_AVAIL ( 8 | 0x40)
#define TRS_WRITE_PROTECTED  (15 | 0x40)
#define TRS_ILL_FILENUMBER   (15 | 0x40)
#define TRS_ILLEGAL_FILENAME (19 | 0x40)
#define TRS_FILE_NOT_FOUND   (24 | 0x40)
#define TRS_EOF_ENCOUNTERED  (28 | 0x40)
// 29 Record number out of range
// À request to read a record within a random access file (see the @POSN vector) provided
#define TRS_ILLEGAL_DRIVENUM (32 | 0x40)
//
// Intentional GAP
// NEW Errors (previously undefined work down from 63)
#define TRS_ACCESS_DENIED	 (58 | 0x40)
#define TRS_FILE_EXISTS		 (59 | 0x40)
#define TRS_PATH_NOT_FOUND   (60 | 0x40)
#define TRS_ILLEGAL_DOS      (61 | 0x40)
#define TRS_INTERNAL_DOS     (62 | 0x40)
#define TRS_UNKNOWN_ERROR    (63 | 0x40)

// Error Translation
static BYTE resultTranslation[] = {
	FR_OK,					/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	TRS_FILE_NOT_FOUND, 	/* (4) Could not find the file */
	TRS_PATH_NOT_FOUND,		/* (5) Could not find the path */
	TRS_ILLEGAL_FILENAME,	/* (6) The path name format is invalid */
	TRS_ACCESS_DENIED,		/* (7) Access denied or directory full */
	TRS_FILE_EXISTS,		/* (8) Name Collision File already exists */
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

static BYTE __inline trans(FRESULT result) {
	return result < 20 ? resultTranslation[result] : result;
}

//-----------------------------------------------------------------------------
// counter for real time clock (RTC).

// configuration Items
extern volatile byte     g_byEnableUpperMem;
extern volatile byte     g_byEnableFDCBoot;
extern volatile byte     g_byEnableFreHDBoot;
extern volatile byte     g_byEnableWaitStates;

// Interrupts
extern volatile byte	g_byRaiseInterrupt;
extern volatile byte	g_byRtcIntrActive;

// Defined by SD Core
extern volatile byte  sd_byCardInialized;

// Defined By System and Z0 Bus
extern volatile byte     g_byResetActive;

///////////////////////////////////////////////////////////////////////////////////////////////////

void InitGlobalVars(void);
void ReleaseWait(void);

byte* TD230_GetPtr(void);
int   TD230_Size(void);

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif
