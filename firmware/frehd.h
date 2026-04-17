
#ifndef FREHD_H
#define FREHD_H

// ===================================

// TRS model : passed by the patched rom
//#define		ROM_MODEL_1	1
//#define		ROM_MODEL_3	3

// FreHD Specific PORT Interface defines
 									// HDD C0 - C1 - Winchester Hard Disk
#define		DATA2	  	0xC2		// FreHd
#define		SIZE2     	0xC3		// FreHd
#define		COMMAND2  	0xC4		// FreHd write
#define		READROM		0xC4		// FreHd read - get confirmation and get BOOT image
#define		ERROR2    	0xC5		// FreHd read,
#define		WRITEROM	0xC5		// FreHd write - command to get a BOOT Image
#define		FILEID2	  	0xC6		// Identifies The File ID for File operations
#define		NOTUSED  	0xC7		// Not Used
; 									// HDD C8-CE - Winchester Hard Disk
#define		STATUS    	0xCF		// HDD CF - Frehd - Piggy Backed on the Hard Drive Status

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

// useful return constants
#define STATUS_ERROR (TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR)
#define STATUS_OK (TRS_HARD_READY | TRS_HARD_SEEKDONE )
#define STATUS_OKDATA (TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_DRQ)

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
//
// NEWER Commands (Post FreHD)
//
// General Functions		($10 - $13)
#define TRS_NEWER_DIR			0x11
//
// File Commands API		($14 - $17)
#define TRS_FILE_OPEN		    0x14
#define TRS_FILE_CLOSE		    0x15
#define TRS_FILE_KILL		    0x16
//
// File Management Command	($18 - $1F)
#define TRS_NEWER_CHANGEDIR		0x19
#define TRS_NEWER_COPYFILE		0x1A
#define TRS_NEWER_DELETE		0x1B
#define TRS_NEWER_MAKEDIR		0x1C
#define TRS_NEWER_RENAME		0x1D
#define TRS_NEWER_PWD		    0x1E
//
// File Commands API
#define TRS_FILE_READ		    0x20
#define TRS_FILE_WRITE		    0x21
#define TRS_FILE_SOF		    0x22
#define TRS_FILE_POS		    0x23
#define TRS_FILE_BACK		    0x24
#define TRS_FILE_EOF		    0x25
#define TRS_FILE_FCB			0x26
//
#define MAX_COMMANDS            0x27

//			Bit 7: Drive Busy
//			Bit 6: Drive Ready
//			Bit 5: // Write Fault
//			Bit 4: // Seek Complete
//			Bit 3: Signal Data Ready
//			Bit 2: Signal Empty File, Zero Length
//			Bit 1: // Command in Progress
//			Bit 0: Error Exists

// Fre HD Status Codes
#define TRS_HARD_BUSY     		0x80
#define TRS_HARD_READY    		0x40
#define TRS_HARD_WRERR    		0x20
#define TRS_HARD_SEEKDONE 		0x10
#define TRS_HARD_DRQ	  		0x08
#define TRS_HARD_ZERO	  		0x04
#define TRS_HARD_CIP	  		0x02
#define TRS_HARD_ERR	  		0x01

void FreHDInit(void);

// Z80 Port Functions
void __not_in_flash_func(frehd_port_out)(word addr, byte data);
byte __not_in_flash_func(frehd_port_in)(word addr);

// background servicing.
void __not_in_flash_func(ServiceFreHdStateMachine)(void);

// ===================================

#endif // FREHD_H
