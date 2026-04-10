#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
//#include "hardware/gpio.h"
#include "hardware/structs/systick.h"

#include "defines.h"
#include "z80_bus.h"
#include "file.h"
//#include "frehd.h"

// Force 4-byte alignment for faster 32-bit internal bus access
// and ensure they are placed in SRAM (not Flash).

// ==================================================
// FDC BOOTBLOCK

// The bootblock served by FDC at initial L2 Startup
static uint8_t __aligned(4) fdc_bootblock[256];

// Poiter into BootBlock
static uint8_t fdc_bootblockIdx = 0;

// ==================================================
// UPPER 32KB RAM

// Upper 32k memory in 0x8000 to 0xFFFF
static uint8_t __aligned(4) by_highmemory[0x8000];

// ==================================================
// PAGED VIRTUAL MEMORY

// the first byte offset 0 in the springboard of each element
static uint8_t __aligned(4) trampoline_first[256];
// the three bytes in offset's 1,2,3 of all springboard elements
static uint8_t __aligned(4) trampoline_code[4] = { 0x00, 0x00, 0x34, 0xC9 };

// Paged RAM (128KB) in region 0x3400 - 0x05FF
// 256 pages * 512 bytes = 0x20000
static uint8_t __aligned(4) by_pagedmemory[0x20000];

// Use volatile if currentPage is modified by both
// the Z80 bus loop and a background USB/UI task.
static volatile uint8_t currentPage = 0;

// current page stack with 16 levels
#define STACK_SIZE 16
#define STACK_MASK (STACK_SIZE - 1)
static uint8_t page_ring[STACK_SIZE];
static uint8_t ring_ptr = 4;

// 512 Bytes ram for 0x3600 to 0x37FF
// Note: addresses 0x3400-0x35FF will effectively "shadow" this RAM
static uint8_t __aligned(4) by_extramemory[0x200];

void MemInit(void) {

	// set up default of call Jump instructions
	memset(trampoline_first, 0xCD, sizeof(trampoline_first));

	// and initialise the paged and non-paged memory itself
	memset(by_pagedmemory, 0x00, sizeof(by_pagedmemory));
	memset(by_extramemory, 0x00, sizeof(by_extramemory));
	currentPage = 0;

	// setup the stack
	memset(page_ring, 0x00, sizeof(page_ring));
	ring_ptr = 4;
}

void LoadFDCBootBlock(void) {

	FIL fsrc;      /* File objects */
	FRESULT fr;    /* FatFs function common result code */
	UINT br;       /* File read/write count */

	fr = f_open(&fsrc, "/SYS/BOOT.SYS", FA_READ);
	if ( fr == FR_OK) {

		/* Read a chunk of data from the source file */
		fr = f_read(&fsrc, fdc_bootblock, sizeof fdc_bootblock, &br);

		/* Close open files */
		f_close(&fsrc);
	}
}

// Service 32KB high RAM in address range $8000 to $FFFF
void __not_in_flash_func(ServiceHighMemoryOperation)(word addr)
{
    byte* pby = &by_highmemory[addr - 0x8000];

    if (!get_gpio(RD_PIN)) { // todo not ideal, should be passed in the request
        FinishReadOperation(*pby);

    } else {
		*pby = FinishWriteOperation();
    }
}

// Extra Paged Memory in Range 0x3000 - 0x37DF
void __not_in_flash_func(ServiceExtraMemory)(word addr)
{
	volatile byte* pby = NULL;
	byte bIsRead = ! get_gpio(RD_PIN); // write or read BUS operation // todo not ideal, should be passed in the request
	addr = (addr - 0x3000) & 0x07FF; // truncate address to range $0000 - $07FF

	if (addr<0x400) { // springboard memory 0x000 - 0x3FF

		byte offset = addr & 0x03; // offset in the springboard
		uint8_t accessPage = (addr >> 2) & 0xFF; // page in springboard

		if (offset==0) { // fist offset byte

			if (bIsRead) {
				// Read of Offset 0 If a CALL, push ht current Page
				if (trampoline_first[accessPage] == 0xCD) {
					page_ring[ring_ptr] = currentPage;
					ring_ptr = (ring_ptr + 1) & STACK_MASK;
				}

				// Set PAGE code byte to read From
				pby = &trampoline_first[accessPage];
			} else {
				// WRITING/LOADING: Validate the opcode
				byte val = FinishWriteOperation();
				// If it's not a Jump, we treat it as a Call (the safer default)
				trampoline_first[accessPage] = (val == 0xC3) ? 0xC3 : 0xCD;
			}

			// Set the Current page
			currentPage = accessPage;
			printf("\r\nmemPage(%d)",currentPage);

		} else if (bIsRead) { // READS of offset 123, WRITES ignored

			if (offset == 3) {
				// On RET (Offset 3): Move Back and Restore current page
				ring_ptr = (ring_ptr - 1) & STACK_MASK;
				currentPage = page_ring[ring_ptr];
				printf("\r\nmemPage(%d)",currentPage);
			}

			pby = &trampoline_code[offset]; // the offset code byte to return
		} // else WRITES are ignored Completely

	} else if ( addr>=0x600) { // general purpose 0x600 - 0x7FF(actually 0x7DF)
		pby = &by_extramemory[addr - 0x600]; //

	} else { // paged memory 0x400 0x5FF
		pby = &by_pagedmemory[(addr - 0x400) + (currentPage * 0x200)];
	}

	if (pby) {
		if (bIsRead) {
			FinishReadOperation(*pby);
		} else {
			*pby = FinishWriteOperation();
		}
	}
}

// Service Memory Control Ports in Range 0x37E0 - 0x3FFF
void __not_in_flash_func(ServiceExtraPorts)(word addr)
{
	static byte byDataAvailable = 0x02; // 1771 FDC Status - data available
	static byte byInterruptAvailable = 0x80; // Interuptt Timer Available
	static byte byZero = 0x00;
	byte bIsRead = ! get_gpio(RD_PIN); // write or read BUS operation // todo not ideal, should be passed in the request
	volatile byte* pby = NULL; // byte to write or read.

	switch (addr) {

		case 0x37E0:
			if (bIsRead) {

				if (g_byRtcIntrActive) {
					pby = &byInterruptAvailable;
					g_byRtcIntrActive = false;
					clr_gpio(INT_PIN);
				} else {
					pby = &byZero;
				}
			}
			break;

		case 0x37EC: // FDC Command Status Register
			if (g_byEnableFDCBoot) {
				if (bIsRead) {
					// return an 0x01 constant.
					pby = &byDataAvailable;
				} else {
					// writing CMD -> reset the pointer to boot block
					// printf("\r\nfdcBoot()");
					fdc_bootblockIdx = 0;
				}
			}
			break;
		case 0x37EF: // FDC Data Register
			if (g_byEnableFDCBoot) {
				if (bIsRead) {
					// printf(".");
					// read byte from the boot block
					pby = &fdc_bootblock[fdc_bootblockIdx++];
				}
			}
			break;
		case 0x37F0: // Virtual MEMORY PAGE Register
			pby = &currentPage; // Page control register (Read/Write)
			break;
		case 0x37F1:
			// todo This needs to be implemented, the below is not correct.
			// if (bIsRead) {
			// 	// POP operation: Restore previous page and return it to Z80
			// 	ring_ptr = (ring_ptr - 1) & STACK_MASK;
			// 	currentPage = page_ring[ring_ptr];
			// } else {
			// 	// PUSH operation: Save current page and switch to the new one
			// 	page_ring[ring_ptr] = currentPage;
			// 	ring_ptr = (ring_ptr + 1) & STACK_MASK;
			// }
			// pby = &currentPage; // RW Page control register at $37F0
			break;
	}

	// if assigned then we need to read write the byte
	if (pby) {
		if (bIsRead) {
			FinishReadOperation(*pby);
		} else {
			*pby = FinishWriteOperation();
		}
	}
}

// $37E1 - Drive Select - IGNORE - is only every written with $01
// $37EC - Disk Command/Status
//				* return $01
//					* return >0 and <$FF for initial detection
//					* return bit0 = 0 (not busy)
//					* return bit1 = 1 (data available)
//				* any write to this should reset pointer
//					* $03 - is written to issue Seek Track 0
//					* $8C - is written to issue read command
// $37ED - Disk Track - IGNORE - never used
// $37EE - Disk Sector - IGNORE - written once ($0) to select sector
// $37EF - Disk Data - where data is read from => $4200
//				* any read should return the boot block data
// $37F0 - Paged RAM - Page Select
//				* full read write access to page register

