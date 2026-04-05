
// TRS model : passed by the patched rom
//#define		ROM_MODEL_1	1
//#define		ROM_MODEL_3	3

// FreHD Specific PORT Interface defines
; 									// HDD C0 - C1 - Winchester Hard Disk
#define		DATA2	  	0xC2		// FreHd
#define		SIZE2     	0xC3		// FreHd
#define		COMMAND2  	0xC4		// FreHd write
#define		READROM		0xC4		// FreHd read - get confirmation and get BOOT image
#define		ERROR2    	0xC5		// FreHd read,
#define		WRITEROM	0xC5		// FreHd write - command to get a BOOT Image
;
// NEW REGISTERS
// #define		DATA3	  	0xC6		// read / write
// #define		COMMAND3  	0xC7		// write
;
; 									// HDD C8-CE - Winchester Hard Disk
#define		STATUS    	0xCF		// HDD CF - Frehd - Piggy Backed on the Hard Drive Status
;

// 0xCF - Register 7 for WD1010 Winchester Disk Controller Chip.
//		  Read = Status Register:
//
//			Bit 0: Error Exists (just an OR of Bits 1-7)
//			Bit 1: Command in Progress
//			Bit 2: Reserved (so forced to 0)
//			Bit 3: Data Request
//			Bit 4: Seek Complete
//			Bit 5: Write Fault
//			Bit 6: Drive Ready
//			Bit 7: Busy

// TRS model : passed by the patched rom
#define ROM_MODEL_1	  1
#define ROM_MODEL_3	  3
#define ROM_MODEL_4	  4
#define ROM_MODEL_4A  4		// model 4 azerty
#define ROM_MODEL_4P  5

// Fre HD Commands
#define TRS_EXTRA_VERSION		0x00
#define TRS_EXTRA_GETTIME		0x01
#define TRS_EXTRA_SETTIME		0x02
#define TRS_EXTRA_OPENFILE		0x03
#define TRS_EXTRA_READFILE		0x04
#define TRS_EXTRA_WRITEFILE		0x05
#define TRS_EXTRA_CLOSEFILE		0x06
#define TRS_EXTRA_BOOTLOADER	0x07
#define TRS_EXTRA_OPENDIR		0x08
#define TRS_EXTRA_READDIR		0x09
// #define TRS_EXTRA_MOUNTDRIVE	0x0A
// #define TRS_EXTRA_SEEKFILE		0x0B
// #define TRS_EXTRA_INFODRIVE		0x0C
// #define TRS_EXTRA_IMAGE			0x0D
// #define TRS_EXTRA_READ_HEADER	0x0E

// NEWER Commands (Post FreHD)
// If implemented fills gap in original FreHD
#define TRS_NEWER_CLOSEDIR		0x10
// dir - is the only one that has any real output.
#define TRS_NEWER_DIR			0x11
//
// cd - error, or no output
#define TRS_NEWER_CHANGEDIR		0x19
// copy - error, or no output
#define TRS_NEWER_COPYFILE		0x1A
// delete - error, or no output
#define TRS_NEWER_DELETE		0x1B
// mkdir - error or noo
#define TRS_NEWER_MAKEDIR		0x1C
// rename - error, or no output
#define TRS_NEWER_RENAME		0x1D
// move file - error, or no output
#define TRS_NEWER_MOVE		    0x1E
//
#define MAX_COMMANDS            0x1E

// Fre HD Status Codes
#define TRS_HARD_BUSY     		0x80
#define TRS_HARD_READY    		0x40
#define TRS_HARD_WRERR    		0x20
#define TRS_HARD_SEEKDONE 		0x10
#define TRS_HARD_DRQ	  		0x08
#define TRS_HARD_ECC	  		0x04
#define TRS_HARD_CIP	  		0x02
#define TRS_HARD_ERR	  		0x01

void FreHDInit(void);

void frehd_port_out(word addr, byte data);
byte frehd_port_in(word addr);
