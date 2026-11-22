#ifndef __SDHC_C_
#define __SDHC_C_

#include "ff.h"

/* type definitions ==========================================*/

/* global variable declarations ==========================================*/

extern volatile BYTE  sd_byCardInialized;
extern volatile DWORD g_dwSdCardPresenceCount;
extern volatile DWORD g_dwSdCardMaxPresenceCount;
extern volatile DWORD g_dwSdSpiKhz;		// SDKHZ from system.cfg; 0 = stay at the boot speed

/* function prototypes ==========================================*/

unsigned char get_cd(void);

BYTE IsSdCardInserted(void);
void SdApplyConfiguredClock(void);
void MountSdCard(void);
void TestSdCardInsertion(void);
void SDHC_Init(void);

void InitTraceCapture(void);

#endif


