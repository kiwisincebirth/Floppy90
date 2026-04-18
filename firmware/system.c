#include "defines.h"
#include "system.h"
#include "fdc.h"
#include "hdc.h"
#include "file.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"
#include "hardware/resets.h"
#include "hardware/watchdog.h"

#include "sd_core.h"
#include "util.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////

// Configuration Items
volatile byte g_byEnableUpperMem;
volatile byte g_byEnableFDCBoot;
volatile byte g_byEnableFreHDBoot;
volatile byte g_byEnableWaitStates;

// interrupt generation
volatile byte g_byRaiseInterrupt;
volatile byte g_byRtcIntrActive;

// in reset mode
volatile byte g_byResetActive;

volatile uint32_t g_dwLedCount;

//-----------------------------------------------------------------------------
// counter for real time clock (RTC)

static uint32_t g_dwResetTime;

static byte  g_byMonitorReset;
static uint32_t g_dwResetCount;

static uint64_t g_nTimeNow;
static uint64_t g_nPrevTime;

static uint32_t g_nRtcIntrCount;

///////////////////////////////////////////////////////////////////////////////////////////////////
void __not_in_flash_func(reset_system)(void)
{
    watchdog_enable(10, 0);
    watchdog_reboot(0, SRAM_END, 0);
	while(1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void InitGlobalVars(void)
{
	// primary configuration items
	g_byEnableUpperMem   = false;
	g_byEnableFDCBoot    = true;
	g_byEnableFreHDBoot  = false;
	g_byEnableWaitStates = true;

	g_dwResetTime        = 1000;	// 1ms

	g_nTimeNow           = time_us_64();
	g_nPrevTime          = g_nTimeNow;

	g_nRtcIntrCount      = 0;
	g_byMonitorReset     = FALSE;
	g_dwResetCount       = 0;

	g_byRtcIntrActive    = false;
	g_byResetActive      = true;
	g_byRaiseInterrupt   = false;

	g_dwLedCount         = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t CountDown(uint32_t nCount, uint32_t nAdjust)
{
	if (nCount > nAdjust)
	{
		nCount -= nAdjust;
	}
	else
	{
		nCount = 0;
	}

	return nCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t CountUp(uint32_t nCount, uint32_t nAdjust)
{
	if (nCount < (0xFFFFFFFF - nAdjust))
	{
		nCount += nAdjust;
	}

	return nCount;
}

////////////////////////////////////////////////////////////////////////////////////
char* SkipBlanks(char* psz)
{
	if (psz == NULL)
	{
		return NULL;
	}

	while ((*psz == ' ') && (*psz != 0))
	{
		++psz;
	}

	return psz;
}

////////////////////////////////////////////////////////////////////////////////////
char* SkipToBlank(char* psz)
{
	if (psz == NULL)
	{
		return NULL;
	}

	while ((*psz != ' ') && (*psz != 0))
	{
		++psz;
	}

	return psz;
}

////////////////////////////////////////////////////////////////////////////////////
char* GetWord(char* psz, char* dest, int max_len)
{
    int len = 0;

    psz = SkipBlanks(psz);

    while ((*psz != ' ') && (*psz != 0) && (len < max_len))
    {
        *dest = *psz;
        ++psz;
        ++dest;
        ++len;
    }

	*dest = 0;

    return psz;
}

////////////////////////////////////////////////////////////////////////////////////
char* CopyLabelName(char* pszSrc, char* pszDst, int nMaxLen)
{
	int i = 0;

	while ((i < nMaxLen) && (*pszSrc != '=') && (*pszSrc != 0))
	{
		*pszDst = toupper(*pszSrc);
		++pszDst;
		++pszSrc;
		++i;
	}

	*pszDst = 0;

	if (*pszSrc == '=')
	{
		++pszSrc;
	}

	return pszSrc;
}

////////////////////////////////////////////////////////////////////////////////////
void CopyString(char* pszSrc, char* pszDst, int nMaxLen)
{
	int i;

	for (i = 0; i < nMaxLen; ++i)
	{
		if (*pszSrc != 0)
		{
			*pszDst = *pszSrc;
			++pszSrc;
		}
		else
		{
			*pszDst = 0;
		}

		++pszDst;
  	}
}

////////////////////////////////////////////////////////////////////////////////////
void StrToUpper(char* psz)
{
	while (*psz != 0)
	{
		*psz = toupper(*psz);
		++psz;
	}
}

////////////////////////////////////////////////////////////////////////////////////
char* stristr(char* psz, char* pszFind)
{
	char* psz1;
	char* psz2;

	while (*psz != 0)
	{
		if (tolower(*psz) == tolower(*pszFind))
		{
			psz1 = psz + 1;
			psz2 = pszFind + 1;

			while ((*psz1 != 0) && (*psz2 != 0) && (tolower(*psz1) == tolower(*psz2)))
			{
				++psz1;
				++psz2;
			}

			if (*psz2 == 0)
			{
				return psz;
			}
		}

		++psz;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// returns   0 if psz1 == psz2
//			    -1 if psz1 < psz2
//           1 if psz1 > psz2
int stricmp(char* psz1, char* psz2)
{
	while ((tolower(*psz1) == tolower(*psz2)) && (*psz1 != 0) && (*psz2 != 0))
	{
		++psz1;
		++psz2;
	}

	if ((*psz1 == 0) && (*psz2 == 0))
	{
		return 0;
	}
	else if (*psz1 == 0)
	{
		return -1;
	}
	else if (*psz2 == 0)
	{
		return 1;
	}
	else if (tolower(*psz1) < tolower(*psz2))
	{
		return -1;
	}
	else if (tolower(*psz1) > tolower(*psz2))
	{
		return 1;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void strcat_s(char* pszDst, int nDstSize, char* pszSrc)
{
	int nLen = strlen(pszDst);

	pszDst += nLen;

	while ((nLen < nDstSize) && (*pszSrc != 0))
	{
		*pszDst = *pszSrc;
		++pszDst;
		++pszSrc;
		++nLen;
	}

	*pszDst = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void strcpy_s(char* pszDst, int nDstSize, char* pszSrc)
{
	int nLen = 0;

	while ((nLen < nDstSize) && (*pszSrc != 0))
	{
		*pszDst = *pszSrc;
		++pszDst;
		++pszSrc;
		++nLen;
	}

	*pszDst = 0;
}

///////////////////////////////////////////////////////////////////////////////
void UpdateCounters(void)
{
	uint64_t nDiff;

	g_nTimeNow  = time_us_64();
	nDiff       = g_nTimeNow - g_nPrevTime;
	g_nPrevTime = g_nTimeNow;

	g_nRtcIntrCount += nDiff;

	if (g_nRtcIntrCount > 25000) // 25mS => 40Hz RTC interrupt
	{
		g_nRtcIntrCount  -= 25000;
		g_byRtcIntrActive = true;
		g_byRaiseInterrupt = true;
	}

	if (get_cd())		// 0 => card removed; 1 => card inserted;
	{
		if (g_dwSdCardPresenceCount < g_dwSdCardMaxPresenceCount)
		{
			g_dwSdCardPresenceCount = CountUp(g_dwSdCardPresenceCount, nDiff);
		}
	}
	else
	{
		g_dwSdCardPresenceCount = 0;
	}

	if (g_byResetActive)
	{
		g_dwResetCount = CountUp(g_dwResetCount, nDiff);

		if ((g_dwResetCount >= 1000) && g_byMonitorReset) // 1ms
		{
			g_byMonitorReset = FALSE;
			FileCloseAll(); // close all open files
			//DirChange("0:/"); // change to root directory
			// todo printf("reset detected");
			multicore_reset_core1();
			reset_system();
		}
	}
	else
	{
		g_dwResetCount   = 0;
		g_byMonitorReset = TRUE;
	}

	if (g_dwLedCount > 0)
	{
		g_dwLedCount = CountDown(g_dwLedCount, nDiff);
	 	gpio_put(LED_PIN, 0);
	}
	else
	{
	 	gpio_put(LED_PIN, 1);
	}
}

////////////////////////////////////////////////////////////////////////////////////
void SysProcessConfigEntry(char szLabel[], char* psz) {
	/*
	EnablePicoRAM=1
	=1
	EnableFDCBoot=1
	FDCBOOT=/SYS/BOOT.SYS
	EnableInterrupts=1
	*/

	if (strcmp(szLabel, "EnableFDCBoot") == 0) {
		g_byEnableFDCBoot = atoi(psz);
	} else if (strcmp(szLabel, "EnableFreHDBoot") == 0) {
		g_byEnableFreHDBoot = atoi(psz);
	} else if (strcmp(szLabel, "EnableHighRAM") == 0) {
		g_byEnableUpperMem = atoi(psz);
	} else if (strcmp(szLabel, "EnableWaitStates") == 0) {
		g_byEnableWaitStates = atoi(psz);
	}
}

///////////////////////////////////////////////////////////////////////////////
void SysInit(void)
{
	char  szLine[64];
	char  szLabel[32];
	char* psz;

	// read the default ini file to load on init
	TFILE* f = FileOpenByName("/SYS/hardware.cfg", FA_READ);

	if (f == NULL) return;

	int nLen = FileReadLine(f, szLine, sizeof(szLine)-2);

	while (nLen >= 0)
	{
		psz = SkipBlanks(szLine);

		if ((*psz != 0) && (*psz != ';') && (*psz != '#')) // blank line or a comment line
		{
			StrToUpper(psz);
			psz = CopyLabelName(psz, szLabel, sizeof(szLabel)-2);
			SysProcessConfigEntry(szLabel, psz);
		}

		nLen = FileReadLine(f, szLine, 126);
	}

	FileClose(f);
}
