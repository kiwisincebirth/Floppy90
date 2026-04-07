#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "tusb.h"
#include "stdio.h"
#include "defines.h"
#include "system.h"
#include "file.h"
#include "sd_core.h"
#include "tests.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

void ServiceFdcLog(void);

static uint64_t g_nCdcPrevTime;
static uint32_t g_nCdcConnectDuration;
static bool     g_bCdcConnected;
static bool     g_bCdcPromptSent;

static char     g_szCommandLine[64];
static int      g_nCommandLineIndex;

#define BUF_SIZE 1024
byte g_bFdcResponse[BUF_SIZE];

// version
static char*   g_pszVersion = {(char*)VERSION_STRING};

static char szHelpText[] = {
                        "\n"
                        "help       - returns this message\n"
                        "reboot     - Reboot normally\n"
                        "flash      - Reboot into BOOTSEL mode\n"
                        "status     - returns the current FDC status\n"
                        "dir filter - returns a directory listing of the root folder of the SD-Card\n"
                        "             optionally include a filter.  For example dir .ini\n"
                    };

void InitCli(void)
{
    g_bCdcPromptSent = false;
    g_bCdcConnected  = false;
    g_nCdcPrevTime   = time_us_64();

	memset(&g_bFdcResponse, 0, sizeof(g_bFdcResponse));
}

void hex_dump(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;

	printf("\n");

    for (size_t i = 0; i < size; i += 16) {
        // Print offset
        printf("%08zx  ", i);

        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size)
                printf("%02X ", bytes[i + j]);
            else
                printf("   ");
        }

        printf(" ");

        // Print ASCII representation
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = bytes[i + j];
            printf("%c", isprint(c) ? c : '.');
        }

        printf("\n");
    }
}

//-----------------------------------------------------------------------------
void FdcProcessStatusRequest(byte print)
{
	char szBuf[64];
	char szLineEnd[4];
	int  i;

	if (print) {
		strcpy_s(szLineEnd, sizeof(szLineEnd)-1, (char*)"\r\n");
	} else {
		strcpy_s(szLineEnd, sizeof(szLineEnd)-1, (char*)"\r");
	}

	strcpy_s((char*)(g_bFdcResponse), sizeof(g_bFdcResponse)-1, (char*)"Pico FDC Version ");
	strcat_s((char*)(g_bFdcResponse), sizeof(g_bFdcResponse)-1, g_pszVersion);
	strcat_s((char*)(g_bFdcResponse), sizeof(g_bFdcResponse)-1, szLineEnd);
	strcat_s((char*)(g_bFdcResponse), sizeof(g_bFdcResponse)-1, szLineEnd);

	sprintf(szBuf, "Enable FBC Boot  = %d", g_byEnableFDCBoot);
	strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);
	strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);

	sprintf(szBuf, "Enable FreHD Boot= %d", g_byEnableFreHDBoot);
	strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);
	strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);

    sprintf(szBuf, "Enable High RAM  = %d", g_byEnableUpperMem);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);

    sprintf(szBuf, "EnableWaitStates = %d", g_byEnableWaitStates);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);

    sprintf(szBuf, "Card Inserted = %d", get_cd() );
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szLineEnd);

    sprintf(szBuf, "Card Mounted  = %d", sd_byCardInialized );
    strcat_s((char*)(g_bFdcResponse),  sizeof(g_bFdcResponse)-1, szBuf);

    puts((char*)g_bFdcResponse);
}

void ListFiles(char* pszFilter)
{
	DIR     dj;			// Directory object
	FILINFO fno;		// File information
	FILINFO_API api_fno;
    FRESULT fr;  		// Return value
    int nCol = 0;

    memset(&dj, 0, sizeof(dj));
    memset(&fno, 0, sizeof(fno));

    fr = f_findfirst(&dj, &fno, "0:", "*");

	while ((fr == FR_OK) && (fno.fname[0] != 0))
	{
		if ((fno.fattrib & AM_DIR) || (fno.fattrib & AM_SYS) || (fno.fname[0] == '.'))
		{
			// pcAttrib = pcDirectory;
		}
		else
		{
//			copyFILEINFO(&api_fno,&fno);
//			hex_dump(&api_fno, sizeof(api_fno));
//			printf("\n");

			if ((pszFilter[0] == 0) || (stristr(fno.fname, pszFilter) != NULL))
			{
                ++nCol;

    			if (nCol < DIR_COLUMNS)
				{
                    printf("%30s %9d", fno.fname, fno.fsize);
                }
                else
                {
                    printf("%30s %9d\r\n", fno.fname, fno.fsize);
                    nCol = 0;
                }
            }
		}

		if (fno.fname[0] != 0)
		{
			fr = f_findnext(&dj, &fno); /* Search for next item */
		}
	}
}

// ======================================

void ProcessCommand(char* psz)
{
    char szCmd[16] = {""};
    char szParm1[16] = {""};

    psz = GetWord(psz, szCmd, sizeof(szCmd)-2);
    psz = GetWord(psz, szParm1, sizeof(szParm1)-2);

    if (stricmp(szCmd, "HELP") ==  0) {
        puts(szHelpText);
    } else if (stricmp(szCmd, "DIR") == 0) {
        ListFiles(szParm1);
    } else if (stricmp(szCmd, "STATUS") == 0) {
        FdcProcessStatusRequest(true);
    } else if (stricmp(szCmd, "REBOOT") == 0) {
        watchdog_reboot(0,0,0);
    } else if (stricmp(szCmd, "FLASH") == 0) {
		rom_reset_usb_boot(0,0);
    } else if (stricmp(szCmd, "t") == 0) {
        RunUnitTests();
    } else {
        puts("Unknown command");
        puts(szHelpText);
    }

}

void ServiceCli(void)
{
    uint64_t nTimeNow;
    char*    prompt = {"\nCMD>"};
    int      c;

    if (!tud_cdc_connected())
    {
        g_bCdcConnected = false;
        return;
    }

    if (g_bCdcConnected == false)
    {
        g_bCdcConnected = true;
       	g_nCdcPrevTime  = time_us_64();
        g_nCdcConnectDuration = 0;
        g_szCommandLine[0] = 0;
        g_nCommandLineIndex = 0;
        return;
    }

    if (g_bCdcPromptSent == false)
    {
        nTimeNow = time_us_64();
        g_nCdcConnectDuration += (g_nCdcPrevTime - nTimeNow);
        g_nCdcPrevTime = nTimeNow;

        if (g_nCdcConnectDuration < 2000000)
        {
            return;
        }

        printf("\nCMD> ");
        g_bCdcPromptSent = true;
    }

    c = getchar_timeout_us(0);

    if (c == PICO_ERROR_TIMEOUT) // no new characters
    {
        return;
    }

    if (c == '\r')
    {
        puts("");
        ProcessCommand(g_szCommandLine);
        printf("\nCMD> ");
        g_nCommandLineIndex = 0;
        g_szCommandLine[0] = 0;
    }
    else if (c == '\b') // backspace
    {
        if (g_nCommandLineIndex > 0)
        {
            --g_nCommandLineIndex;
            g_szCommandLine[g_nCommandLineIndex] = 0;
            printf("\b \b");
        }
    }
    else if (c < 32)
    {
        while (c != PICO_ERROR_TIMEOUT)
        {
            c = getchar_timeout_us(100);
        }
    }
    else if (g_nCommandLineIndex < sizeof(g_szCommandLine)-2)
    {
        if ((c != '\n') && (c != '\r'))
        {
            printf("%c", c);
            g_szCommandLine[g_nCommandLineIndex] = c;
            ++g_nCommandLineIndex;
            g_szCommandLine[g_nCommandLineIndex] = 0;
        }
    }
}
