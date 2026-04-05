
#ifndef _SYSTEM_H
#define _SYSTEM_H

#ifndef MAX_PATH
	#define MAX_PATH 64
#endif

void reset_system(void);
void SysInit(void);

uint32_t CountDown(uint32_t nCount, uint32_t nAdjust);
uint32_t CountUp(uint32_t nCount, uint32_t nAdjust);

////////////////////////////////////////////////////////////////////////////////////

char* SkipBlanks(char* psz);
char* SkipToBlank(char* psz);
char* GetWord(char* psz, char* dest, int max_len);

char* CopyLabelName(char* pszSrc, char* pszDst, int nMaxLen);
void  CopyString(char* pszSrc, char* pszDst, int nMaxLen);
void  StrToUpper(char* psz);
char* stristr(char* psz, char* pszFind);

void  AddTrailingBackslash(char szFilePath[], int nMaxLen);

int  stricmp(char* psz1, char* psz2);
void strcpy_s(char* pszDst, int nDstSize, char* pszSrc);
void strcat_s(char* pszDst, int nDstSize, char* pszSrc);
#define sprintf_s snprintf

void UpdateCounters(void);
void LoadIniFile(char* pszFileName);

#endif
