#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "pico/multicore.h"

#include "defines.h"
#include "sd_core.h"

#include <sd_spi.h>

#include "ff.h"
#include "hw_config.h"
#include "sd_card.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////

#define SD_KHZ_MIN   1000		// SPI clock the card is initialised and mounted at (SD_SPI_KHZ build option)
#define SD_KHZ_MAX  25000		// highest SPI clock an SD card is allowed to use

volatile BYTE  sd_byCardInialized = false;
volatile DWORD g_dwSdCardPresenceCount;
volatile DWORD g_dwSdCardMaxPresenceCount;
volatile DWORD g_dwSdSpiKhz = 5000;		// SD KHZ from HARDWARE.cfg

static BYTE    sd_byCurrentCdState;
static BYTE    sd_byCardRemoved;
static BYTE    sd_byPreviousCdState;
static WORD    sd_wCardInitTries;

//-----------------------------------------------------------------------------
// returns: 1 => card inserted; 0 => card removed;
unsigned char get_cd(void)
{
	return 1; // TODO Something wrong Here
	// inverting since the pin is Active LOW
	// return (gpio_get(CD_PIN) == 0) ? 1 : 0;
}

//-----------------------------------------------------------------------------
BYTE IsSdCardInserted(void)
{
	return sd_byCurrentCdState != 0;
}

////////////////////////////////////////////////////////////////////////////////////
// The card is always initialised and mounted at the boot speed (SD_SPI_KHZ, 5 MHz by default),
// so system.cfg can be read on any card. Once system.cfg has been read, SDKHZ raises (or lowers)
// the clock for normal operation. A card that is inserted later is handled the same way.
void SdApplyConfiguredClock(void)
{
	sd_card_t* pSD = sd_get_by_num(0);

	if (pSD == NULL) return;

	if (g_dwSdSpiKhz < SD_KHZ_MIN) g_dwSdSpiKhz = SD_KHZ_MIN;
	if (g_dwSdSpiKhz > SD_KHZ_MAX) g_dwSdSpiKhz = SD_KHZ_MAX;

	pSD->spi->baud_rate = g_dwSdSpiKhz * 1000;

	sd_spi_go_high_frequency(pSD);
}

////////////////////////////////////////////////////////////////////////////////////
void MountSdCard(void)
{
	if ( get_cd() ) { // if card inserted

		sd_byCardInialized = FALSE;

		if (sd_wCardInitTries < 5) {
			++sd_wCardInitTries;

			// initialise the card at the boot speed
			sd_card_t* pSD = sd_get_by_num(0);
			if (pSD != NULL) pSD->spi->baud_rate = SD_KHZ_MIN * 1000;

			if (!sd_init_driver()) return;

			// char*  pszDrive = (char*)"0:";
			FATFS* pfs = &sd_get_by_num(0)->fatfs;

			if (!pfs) return;

			FRESULT fr = f_mount(pfs, "0:", 1);

			if (fr == FR_OK) {
				sd_byCardInialized = TRUE;
				SdApplyConfiguredClock();
			}
		}

		sd_byCardRemoved = FALSE;
	}
	else
	{
		sd_byCardRemoved   = TRUE;
		sd_byCardInialized = FALSE;
		sd_wCardInitTries  = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////
void TestSdCardInsertion(void)
{
	sd_byCurrentCdState = get_cd();		// 0 => card removed; 1 => card inserted;

	if (sd_byCurrentCdState != sd_byPreviousCdState)
	{
		if (sd_byCurrentCdState != 0)	// card has been inserted
		{
			if (g_dwSdCardPresenceCount < g_dwSdCardMaxPresenceCount) // wait for Sd-Card insertion to debounce
			{
				return;
			}

			sd_byCardRemoved  = TRUE;
			sd_wCardInitTries = 0;
			MountSdCard();
		}
		else // card was removed
		{
		}

		if (sd_byCurrentCdState == 0)		// card is not present
		{
			sd_byCardInialized = FALSE;
		    // multicore_reset_core1();
			// reset_system();
		}
	}

	sd_byPreviousCdState = sd_byCurrentCdState;
}

////////////////////////////////////////////////////////////////////////////////////
void SDHC_Init(void)
{
	sd_byCardRemoved           = FALSE;
	sd_byCardInialized         = FALSE;
	sd_byCurrentCdState        = get_cd();				// 0 => card removed; 1 => card inserted;
	sd_byPreviousCdState       = sd_byCurrentCdState;
	sd_wCardInitTries          = 0;
	g_dwSdCardMaxPresenceCount = 10000;
	g_dwSdCardPresenceCount    = 0;
}
