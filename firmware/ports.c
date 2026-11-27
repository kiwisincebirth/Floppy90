
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "defines.h"
#include "file.h"

#include "ports.h"

#include <stdint.h>
#include <util.h>

#include "file_cmnds.h"

#define	AM_HID_SYS (AM_HID + AM_SYS)	/* Hidden System Files */

// ORIGINAL FRE HD Bootloader Block - Z80 Code.
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

// FILEID - holds current file ID for File IO
static volatile byte  fhd_fileid = 0;

#define FHD_DATA2_SIZE (256u + 64u)

// DATA2 - data buffer shared with Z80 and underlying process
// extra bytes for FCB storage (32) + another 32 for extra safety
static volatile byte __aligned(4) fhd_data2[FHD_DATA2_SIZE] = {0};

// SIZE2 - size of data in DATA2
static volatile byte  fhd_size2 = 0;

// ERROR2 - Actual Result of underlying FF operation
static volatile byte  fhd_error2 = TRS_OK;

//-----------------------------------------------------------------------------

void PortsInit(void)
{
}
//-----------------------------------------------------------------------------

// Command function type
typedef byte (*command_func_t)(byte);

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
byte __not_in_flash_func(CMD_VER)(byte step) {
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

// ====================================================================

// Stateful Open File - Legacy (single) File Object
static TFILE* state_openFile;

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
byte __not_in_flash_func(CMD_OPEN)(byte step) {

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		// close existing file
		FileClose((void *)state_openFile);

		BYTE byMode = fhd_data2[0]; // first byte is the OPEN mode
		char *pszFromZ80 = (char *)&fhd_data2[1];; // next bytes are the file name

		// Sanitised
		Trs80PathResult res;

		// 1. Sanitize the string (handles /ext and :D)
		// sanitizeTrs80Path(pszFromZ80, cleanPath, &driveNum, 32);
		trs80_resolve_path(pszFromZ80, 32, &res);

		// 2. Open it (relative to CWD)
		state_openFile = FileOpenByName(res.primary, byMode);

		if ( state_openFile ) {
			fhd_error2 = state_openFile->lastResult;

			bool allowSecondary = res.hasSecondary &&
				((byMode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) == 0);

			if ( ! state_openFile->byIsOpen && allowSecondary ) {
				state_openFile = FileOpenByName(res.secondary, byMode);
			}

			if ( state_openFile ) {
				fhd_error2 = state_openFile->lastResult;

				if ( state_openFile->byIsOpen ) {

					FSIZE_t size = FileGetSize(state_openFile);
					printf("\r\nopenFile(%s,%llu) ",res.primary,size);

					fhd_size2 = sizeof (DWORD); // file size as 4 bytes
					memcpy((void *)fhd_data2, &size, fhd_size2);

					return STATUS_OKDATA;
				} else {
					return STATUS_ERROR; // not opened
				}
			} else {
				return STATUS_ERROR; // not opened
			}
		}
		fhd_error2 = TRS_TOO_MANY_FILES; // Or equivalent FatFs error
		return STATUS_ERROR;
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
byte __not_in_flash_func(CMD_READ)(byte step) {

	// after getting Command - Signal Busy and defer processing.
	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		if ( FileIsOpen((void *)state_openFile) ) {

			fhd_error2 = FileRead((void *)state_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == TRS_OK) {

				uint32_t actualRead = state_openFile->bytesReadWritten;
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
			fhd_error2 = TRS_ILL_FILENUMBER;
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
 byte __not_in_flash_func(CMD_WRITE)(byte step) {

 	// Step 1 noting to do at this point, wait for data in Step 2

 	// after getting N Bytes - Signal Busy and defer processing.
 	if (step == 2) return TRS_HARD_BUSY;

 	if (step == 10) {
		printf("\r\nwriteFile() ");

		if ( FileIsOpen((void *)state_openFile) ) {

			fhd_error2 = FileWrite((void *)state_openFile, (void *)fhd_data2, fhd_size2);

			if (fhd_error2 == TRS_OK) {
				if ( state_openFile->bytesReadWritten < fhd_size2) {
					return STATUS_ERROR; // didnt complete write
				} else {
					printf("+");
					return STATUS_OK; // success
				}
			} else {
				return STATUS_ERROR;
			}
		} else {
			fhd_error2 = TRS_ILL_FILENUMBER;
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
byte __not_in_flash_func(CMD_CLOSE)(byte step) {

	if (step == 1) {
		if ( FileIsOpen((void *)state_openFile) ) {
			return TRS_HARD_BUSY;
		}
	}

	if (step == 10) {

		if (FileIsOpen((void *)state_openFile)) {

			printf("\r\ncloseFile() ");
			fhd_error2 = FileClose((void *)state_openFile);
			if (fhd_error2 != TRS_OK) {
				return STATUS_ERROR;
			}
		}
		state_openFile = NULL;
	}
	return STATUS_OK;
}

// ====================================================================

/*
 * CMD_DIR
 *
 * Command Syntax:
 *   DIR
 *   DIR <PATTERN>
 *   DIR :<D>
 *   DIR <PATTERN>:<D>
 *   DIR /<PATH>[/<PATTERN>]
 *   DIR ./<PATH>[/<PATTERN>]
 *   DIR ../<PATH>[/<PATTERN>]
 *
 *   - <PATTERN> : Optional filename or wildcard filter (default "*").
 *   - :<D>      : Optional postfixed logical drive number (0..9); drives 1..9
 *                 use ASSIGN mappings and cannot be combined with explicit
 *                 path prefixes (/, ./, ../).
 *   - /..., ./..., ../... : Explicit root, current-directory, or parent-
 *                 directory paths (resolved relative to CWD when using ..).
 *
 * Bridge Protocol / Steps:
 *   - step == 2 : Reads argument string from fhd_data2, parses target directory
 *                 and pattern via parse_dir_target(), flags parse errors,
 *                 marks next scan as find-first, returns TRS_HARD_BUSY.
 *   - step == 3 : Marks subsequent scans as find-next, returns TRS_HARD_BUSY.
 *   - step == 10: Returns STATUS_ERROR (TRS_DEVICE_NOT_AVAIL) if parse failed;
 *                 otherwise opens/advances directory scan skipping hidden/system
 *                 and '.' entries, copies FILINFO_API to fhd_data2 (STATUS_OKDATA),
 *                 or closes directory and returns STATUS_OK when finished.
 *
 * Arguments:
 *   step - Bridge state-machine step index (2, 3, 10, etc.).
 *
 * Returns:
 *   TRS_HARD_BUSY, STATUS_OKDATA, STATUS_OK, or STATUS_ERROR.
 *
 * USAGE:
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
byte __not_in_flash_func(CMD_DIR)(byte step)
{
    static DIR dir;
    static byte dir_open = 0;
    static byte dir_findfirst = 1;
    static byte need_parse = 1;
    static FILINFO fno;
    static FILINFO_API api_fno;
    static char target_dir[TRS80_PATH_MAX] = ".";
    static char file_pattern[32] = "*";

    if (step == 2) {
        need_parse = 1;
        dir_findfirst = 1;
        return TRS_HARD_BUSY;

    } else if (step == 3) {
        dir_findfirst = 0;
        return TRS_HARD_BUSY;
    }

	if (step == 10) {

		if (dir_findfirst && need_parse) {
			printf("\r\nDIR(%s) ", (char *)&fhd_data2[0]);

			char raw_arg[TRS80_PATH_MAX];
			memcpy(raw_arg, (const void *)fhd_data2, sizeof(raw_arg) - 1);
			raw_arg[sizeof(raw_arg) - 1] = '\0';

			fhd_error2 = parse_dir_target(
				raw_arg,
				target_dir,
				sizeof(target_dir),
				file_pattern,
				sizeof(file_pattern));

			if (fhd_error2 != TRS_OK) {
				if (dir_open) {
					f_closedir(&dir);
					dir_open = 0;
				}
				return STATUS_ERROR;
			}
			need_parse = 0;
		}

        if (dir_findfirst) {
            if (dir_open) {
                f_closedir(&dir);
                dir_open = 0;
            }

            fhd_error2 = DirFindFirst(&dir, &fno, target_dir, file_pattern);
            dir_findfirst = 0;
        } else {
            fhd_error2 = DirFindNext(&dir, &fno);
        }

        while (fhd_error2 == TRS_OK && fno.fname[0] != 0 &&
               ((fno.fattrib & AM_HID_SYS) || fno.fname[0] == '.')) {
            fhd_error2 = DirFindNext(&dir, &fno);
        }

        if (fhd_error2 == TRS_OK) {
            dir_open = 1;

            if (fno.fname[0] != 0) {
                copyFILEINFO(&api_fno, &fno);
                fhd_size2 = (unsigned char)sizeof(FILINFO_API);
                memcpy((void *)fhd_data2, &api_fno, fhd_size2);

                return STATUS_OKDATA;
            } else {
                fhd_size2 = 0;
                if (dir_open) {
                    f_closedir(&dir);
                    dir_open = 0;
                }
                return STATUS_OK;
            }
        } else {
            if (dir_open) {
                f_closedir(&dir);
                dir_open = 0;
            }
            return STATUS_ERROR;
        }
    }

    return STATUS_OK;
}

// ====================================================================
// ====================================================================
// ====================================================================

/*
 * format_assignments_text
 *
 * Intent:
 *   Formats active virtual drive directory mappings (:1..:9) into a display
 *   buffer for the ASSIGN command using '\n' line separators and a trailing
 *   0x03 (ETX) terminator so the DOS Display API prints all lines without
 *   stopping early on 0x0D (CR).
 *
 * Output Format:
 *   Each non-empty mapping in drives 1..9 is written as "<D>:<PATH>\n",
 *   followed by a single 0x03 byte and a null terminator when space permits.
 *   Returns 0x03 alone (plus '\0') if no drives are currently mapped.
 *
 * Arguments:
 *   buf     - Destination buffer receiving the formatted text response.
 *   bufSize - Byte capacity of buf.
 *
 * Returns:
 *   Number of bytes written into buf (excluding the trailing '\0'), or 0 if
 *   bufSize is 0.
 */
static size_t format_assignments_text(char *buf, size_t bufSize)
{
	size_t pos = 0;
	if (bufSize == 0) {
		return 0;
	}
	buf[0] = '\0';

	for (uint8_t d = 1; d <= 9; d++) {
		const char *path = trs80_get_mapped_dir(d);
		if (path != NULL && path[0] != '\0') {
			int written = snprintf(
				buf + pos,
				bufSize - pos,
				"%u:%s\n",
				(unsigned int)d,
				path);
			if (written < 0 || (size_t)written >= bufSize - pos) {
				pos = (written < 0) ? pos : bufSize - 1;
				break;
			}
			pos += (size_t)written;
		}
	}

	if (pos + 1 < bufSize) {
		buf[pos++] = 0x03;
		buf[pos] = '\0';
	}
	return pos;
}

/*
 * normalize_assign_path
 *
 * Intent:
 *   Normalizes a raw target directory string passed to ASSIGN <D>:<PATH> into
 *   a canonical uppercase slash-delimited path starting with '/' so path joins
 *   and listings behave consistently.
 *
 * Normalization Rules:
 *   - Skips leading spaces/tabs; if empty or terminated immediately, out
 *     remains an empty string (used by ASSIGN <D>: to clear a mapping).
 *   - Prepends '/' if omitted, converts '\\' to '/', and uppercases characters.
 *   - Stops at CR ('\r'), LF ('\n'), or 0x03 (ETX).
 *   - Strips trailing spaces, tabs, and '/' characters (preserving "/" alone).
 *
 * Arguments:
 *   raw     - Raw path string following "<D>:" from the ASSIGN command input.
 *   out     - Destination buffer receiving the normalized path.
 *   outSize - Byte capacity of out.
 */
static void normalize_assign_path(const char *raw, char *out, size_t outSize)
{
    size_t outIdx = 0;
    if (outSize == 0) {
        return;
    }
    out[0] = '\0';

    while (*raw == ' ' || *raw == '\t') {
        raw++;
    }

    if (*raw == '\0') {
        return;
    }

    /* Ensure leading '/' so joined paths become "/DOCS/FILE.TXT" */
    if (*raw != '/') {
        out[outIdx++] = '/';
        out[outIdx] = '\0';
    }

    while (*raw != '\0' && outIdx + 1 < outSize) {
        unsigned char c = (unsigned char)*raw++;
        if (c == '\r' || c == '\n' || c == 0x03) {
            break;
        }
        if (c == '\\') {
            c = '/';
        }
        out[outIdx++] = (char)toupper(c);
    }

    /* Trim trailing whitespace or extra slashes (keep root "/" if alone) */
    while (outIdx > 1 && (out[outIdx - 1] == ' ' || out[outIdx - 1] == '\t' || out[outIdx - 1] == '/')) {
        outIdx--;
    }
    out[outIdx] = '\0';
}

/*
 * ASSIGN a Drive Number to a Directory
 * ASSIGN {N:{PathSpecc}} - Three Forms
 *   -null- - display the current mappings
 *   N: - remove assignment for Drive N
 *   N:PathSpec - assign Drive to Path Spec
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status.
 * 			If TRS_HARD_ERR set, check ERROR2 and exit
 *			If DATA is available then
 * 5. TRS Reads SIZE2 bytes. -> displays to user
 */
byte __not_in_flash_func(CMD_ASSIGN)(byte step) {

    if (step == 2) return TRS_HARD_BUSY;

    if (step == 10) {
        char *arg = (char *)fhd_data2;
        arg[TRS80_PATH_MAX - 1] = '\0';

        /* Strip terminators and leading spaces */
        for (size_t i = 0; arg[i] != '\0'; i++) {
            if (arg[i] == '\r' || arg[i] == '\n' || arg[i] == 0x03) {
                arg[i] = '\0';
                break;
            }
        }
        while (*arg == ' ' || *arg == '\t') {
            arg++;
        }

        /* Form 1: ASSIGN (no arguments -> list current mappings) */
        if (*arg != '\0') {
            /* Expect <1..9>:<optional path> */
            if (arg[0] < '1' || arg[0] > '9' || arg[1] != ':') {
                fhd_error2 = TRS_DEVICE_NOT_AVAIL; /* or your DOS syntax error */
                return STATUS_ERROR;
            }

            uint8_t driveId = (uint8_t)(arg[0] - '0');
            const char *pathPart = arg + 2;

            char normPath[TRS80_PATH_MAX];
            normalize_assign_path(pathPart, normPath, sizeof(normPath));

            /* Form 2 & 3: non-empty sets mapping; empty (e.g. "1:") clears it */
            if (normPath[0] == '\0') {
                trs80_set_mapped_dir(driveId, NULL);
            } else {
                trs80_set_mapped_dir(driveId, normPath);
            }

        	// SAVE ASSIGN configuration
        	trs80_save_assign_cfg();
        }

        size_t outLen = format_assignments_text((char *)fhd_data2, TRS80_PATH_MAX * 4);
        fhd_size2 = (WORD)outLen;
        return STATUS_OKDATA;
    }

    return STATUS_OK;
}

/*
 * Change directory
 * CD {nameOfDirectory or /} - Changes the current working directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
byte __not_in_flash_func(CMD_CHANGEDIR)(byte step)
{
	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		char* psz = (char*)&fhd_data2[0]; // next bytes are the file name

		printf("\r\nCD(%s) ", psz );
		fhd_error2 = DirChange((const TCHAR *)&fhd_data2[0]);
		if (fhd_error2 != TRS_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

// -------------------------

static bool ParseCopyArgs(char *argBuf, size_t maxLen, char **outSrc, char **outDst) {
	if (!argBuf || maxLen == 0) return false;

	argBuf[maxLen - 1] = '\0';

	char *p = argBuf;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') return false;

	*outSrc = p;
	while (*p && !isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') return false;

	*p++ = '\0';

	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') return false;

	*outDst = p;

	char *end = p + strlen(p);
	while (end > p && isspace((unsigned char)*(end - 1))) {
		end--;
	}
	*end = '\0';

	return true;
}

/*
 * COPY Command
 * COPY <sourceFilespec> <destFilespec>
 *
 * 1. TRS issues COMMAND2
 * 2. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status.
 * 			If TRS_HARD_ERR set, check ERROR2 and exit
 *			If DATA is available, then
 * 5. TRS Reads SIZE2 bytes. -> displays to user
 */
uint8_t __not_in_flash_func(CMD_COPYFILE)(uint8_t step) {
	if (step == 2) {
		return TRS_HARD_BUSY;
	}

	if (step == 10) {
		char *buf = (char *)fhd_data2;
		char *srcSpec = NULL;
		char *dstSpec = NULL;

		if (!ParseCopyArgs(buf, FHD_DATA2_SIZE, &srcSpec, &dstSpec)) {
			fhd_error2 = TRS_ILLEGAL_FILENAME;
			return STATUS_ERROR;
		}

		printf("\r\nCOPY(%s -> %s)", srcSpec, dstSpec);

		uint32_t copiedCount = 0;
		BYTE copyErr = CopyFileSpec(srcSpec, dstSpec, &copiedCount);

		if (copyErr != TRS_OK) {
			fhd_error2 = copyErr;

			if (copiedCount == 0) {
				return STATUS_ERROR;
			}

			int errLen = snprintf(
				buf,
				FHD_DATA2_SIZE,
				"%lu File(s) Copied, with Error %u\r\n",
				(unsigned long)copiedCount,
				(unsigned int)copyErr
			);
			if (errLen < 0) {
				return STATUS_ERROR;
			}

			fhd_size2 = (uint16_t)errLen + 1;
			return STATUS_OKDATA;
		}

		fhd_error2 = TRS_OK;
		int okLen = snprintf(
			buf,
			FHD_DATA2_SIZE,
			"%lu File(s) Copied.\r\n",
			(unsigned long)copiedCount
		);
		if (okLen < 0) {
			return STATUS_ERROR;
		}

		fhd_size2 = (uint16_t)okLen + 1;
		return STATUS_OKDATA;
	}

	return STATUS_OK;
}

/*
 * Make directory
 * MKDIR {nameOfDirectory} - Creates a named directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
byte __not_in_flash_func(CMD_MAKEEDIR)(byte step)
{
	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		char* psz = (char*)&fhd_data2[0]; // next bytes are the file name

		printf("\r\nMKDIR(%s) ", psz );
		fhd_error2 = DirMake((const TCHAR *)&fhd_data2[0]);
		if (fhd_error2 != FR_OK) {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * Delete File or directory
 * DELETE {nameOfDirectory} - delete file or directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
byte __not_in_flash_func(CMD_DELETE)(byte step) {
    if (step == 2) return TRS_HARD_BUSY;

    if (step == 10) {
        char *psz = (char *)&fhd_data2[0];
        uint32_t deletedCount = 0;
        BYTE lastErr = TRS_OK;
        bool hadError = false;

    	char *token = strtok(psz, " ");
    	while (token != NULL) {
    		char qualifiedPath[TRS80_PATH_MAX];
    		BYTE specErr = resolve_delete_spec(token, qualifiedPath, sizeof(qualifiedPath));

    		if (specErr != TRS_OK) {
    			printf("\r\nRM(%s) FAILED: err=%u", token, (unsigned int)specErr);
    			lastErr = specErr;
    			hadError = true;
    		} else {
    			printf("\r\nRM(%s) ", qualifiedPath);
    			uint32_t tokenDeleted = 0;
    			BYTE delErr = DeleteFileSpec(qualifiedPath, &tokenDeleted);
    			deletedCount += tokenDeleted;
    			if (delErr != TRS_OK) {
    				printf("FAILED: %d", delErr);
    				lastErr = delErr;
    				hadError = true;
    			} else {
    				printf("OK (%lu)", (unsigned long)tokenDeleted);
    			}
    		}

    		token = strtok(NULL, " ");
    	}

        if (hadError) {
            fhd_error2 = lastErr;

            if (deletedCount == 0) {
                return STATUS_ERROR;
            }

            int errLen = snprintf(
                (char *)&fhd_data2[0],
                FHD_DATA2_SIZE,
                "%lu File(s) Deleted, with Error %u\r\n",
                (unsigned long)deletedCount,
                (unsigned int)lastErr
            );
            if (errLen < 0) {
                return STATUS_ERROR;
            }

            fhd_size2 = (uint16_t)errLen + 1;
            return STATUS_OKDATA;
        }

        fhd_error2 = TRS_OK;
        int okLen = snprintf(
            (char *)&fhd_data2[0],
            FHD_DATA2_SIZE,
            "%lu File(s) Deleted.\r\n",
            (unsigned long)deletedCount
        );
        if (okLen < 0) {
            return STATUS_ERROR;
        }

        fhd_size2 = (uint16_t)okLen + 1;
        return STATUS_OKDATA;
    }

    return STATUS_OK;
}

/*
 * Rename a File or directory
 * REN {sourceName} {targetName} - delete file or directory
 *
 * 1. TRS issues COMMAND2
 * 1. TRS issues SIZE2, sends DATA2
 * 3. TRS waits
 * 4. TRS reads status. If TRS_HARD_ERR set, check ERROR2.
 */
byte __not_in_flash_func(CMD_RENAME)(byte step)
{
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {
		char *buf = (char *)&fhd_data2[0];
		char *pSpace = strchr(buf, ' ');
		if (!pSpace) {
			fhd_error2 = TRS_ILLEGAL_FILENAME;
			return STATUS_ERROR;
		}

		*pSpace = '\0';
		char *pszSrc = buf;
		char *pszDest = pSpace + 1;
		while (*pszDest == ' ') {
			pszDest++;
		}

		char cleanSrc[33];
		char cleanDest[33];
		byte driveSrc = 0;
		byte driveDest = 0;

		sanitizeTrs80Path(pszSrc, cleanSrc, &driveSrc, 32);
		sanitizeTrs80Path(pszDest, cleanDest, &driveDest, 32);

		printf("\r\nREN (%s) -> (%s)", cleanSrc, cleanDest);

		uint32_t renamedCount = 0;
		BYTE renErr = RenameFileSpec(cleanSrc, cleanDest, &renamedCount);

		if (renErr != TRS_OK) {
			fhd_error2 = renErr;
			if (renamedCount == 0) {
				return STATUS_ERROR;
			}

			int errLen = snprintf(
				buf,
				FHD_DATA2_SIZE,
				"%lu File(s) Renamed, with Error %u\r\n",
				(unsigned long)renamedCount,
				(unsigned int)renErr
			);
			if (errLen < 0) {
				return STATUS_ERROR;
			}
			fhd_size2 = (uint16_t)errLen + 1;
			return STATUS_OKDATA;
		}

		fhd_error2 = TRS_OK;
		int okLen = snprintf(
			buf,
			FHD_DATA2_SIZE,
			"%lu File(s) Renamed.\r\n",
			(unsigned long)renamedCount
		);
		if (okLen < 0) {
			return STATUS_ERROR;
		}
		fhd_size2 = (uint16_t)okLen + 1;
		return STATUS_OKDATA;
	}

	return STATUS_OK;
}

static const char *StripDrivePrefix(const char *path) {
	if (!path || path[0] == '\0') {
		return "/";
	}
	// Strip "0:" (or any single-digit drive prefix followed by ':') when followed by a path
	if (path[0] >= '0' && path[0] <= '9' && path[1] == ':') {
		const char *rest = path + 2;
		return (*rest != '\0') ? rest : "/";
	}
	return path;
}

/*
 * PWD - display working directory
 *
 * 1. TRS issues COMMAND2
 * 2. TRS waits
 * 3. TRS reads status.
 * 			If TRS_HARD_ERR set, check ERROR2 and exit
 *			If DATA is available then
 * 4. TRS Reads SIZE2 bytes. -> displays to user
 */

byte __not_in_flash_func(CMD_PWD)(byte step)
{
	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {
		char *buf = (char *)fhd_data2;

		fhd_error2 = CurrentDirectory(buf, sizeof(fhd_data2));
		if (fhd_error2 != TRS_OK) {
			return STATUS_ERROR;
		}

		printf("\r\nPWD()->%s", buf);

		size_t len = strlen(buf);
		buf[len] = 0x0D; // Overwrite the '\0' with 0x0D (TRS-80 terminator)
		fhd_size2 = len + 1; // length including terminator

		return STATUS_OKDATA;
	}
	return STATUS_OK;
}

// ====================================================================
// ====================================================================
// ====================================================================


/**
; * Pack the parameter registers at the beginning of the FCB
;   * Open Flag's (Existing New, ETC)
;   * B - LRL
;   * DE - FCB Buffer
; * Send the 32 byte FCB block to PICO, OPEN command
*/
#pragma pack(push, 1)		// Prevent compiler padding the structure
typedef struct {
	BYTE    byMode;			/* Open Mode - Defined by FF */
	BYTE    lrl;			/* Logical Record Length 256=0 */
	WORD	buffaddr;		/* address of in memory buffer */
	TCHAR	fcb[32];		/* FCB containing filename */
} OPEN_REQUEST;
#pragma pack(pop)

// void __not_in_flash_func(parseRequest)(OPEN_REQUEST* req, TFILE* f) {
// 	// 1. Extract LRL
// 	f->lrl = req->lrl;
//
// 	// 2. Use the core processor
// 	sanitizeTrs80Path((char*)req->fcb, f->fname, &f->driveId, 32);
// }

/**
;  The 32 byte FCB block sent back to Z80
; NOTES:
;  (*) typically refers to a sensible default.
;  Unspecified Bits they can be left as Zero
;  MOB refers to Middle Order Byte in 24 bit values -> LSB, MOB, MSB
;      Least Significant - Middle Order - Most Significant (Bytes)
*/
#pragma pack(push, 1)		// Prevent compiler padding the structure
typedef struct {

	BYTE	byte1;				// Bit 7 = 1 if the file is open. (*)
								// Bit 7 = 0 if the file is NOT open.

	BYTE	byte2;				// Bit 7 = 1 if accessing logical records of size LRL
								// Bit 7 = 0 if accessing full sectors LRL = 0 (256)
								// Bit 6 = 1 EOF to be set only if exceed file size - random access
								// Bit 6 = 0 EOF to be set after every write - sequential access (*)
								// Bit 5 = 1 the Z80 sector buffer is not populated (*)
								// Bit 5 = 0 the Z80 sector buffer is populated
								// Bit 4 = 1 the Z80 sector buffer contents are Dirty. Changed since last read.
								// Bit 4 = 0 the Z80 sector buffer contents are Clean. unchanged (*)

	BYTE	byte3;				// 02 Reserved Flags / Future Use
	WORD	buffaddr;			// 03 16 bit Z80 main data buffer address. copied From request
	BYTE	nextLSB;			// 05 LSB of the next byte (initially 0) in file to access.
	BYTE    driveId;		    // 06 drive number 0-9, where the file was opened.
	BYTE    fileId;				// 07 unique File identifier. used to access the File
	BYTE	eofLSB;				// 08 LSB of EOF of the File. File Size.
	BYTE	lrl;				// 09 Logical Record Length, zero means 256 full sector access
	BYTE	nextMOB;			// 0A MOB of the next byte (initially 0) in file to access.
	BYTE	nextMSB;			// 0B MSB of the next byte (initially 0) in file to access.
	BYTE	eofMOB;				// 0C MOB of EOF of the File. File Size.
	BYTE	eofMSB;				// 0D MSB of EOF of the File. File Size.

	TCHAR	trailer[18];		// trailing data must be filled with $FF (*)
} FCB;
#pragma pack(pop)

/**
 * Used to populate an FCB from an open File
 * @param fp
 * @param fcb
 */
void FileToFCB(TFILE* fp, FCB* fcb) {
	if (!fp || !fcb ) return;

	// Init structure: Zero out, then set the $FF trailer
	memset(fcb, 0, sizeof(FCB));

	// byte1: Bit 7 = 1 (Open Status)
	fcb->byte1 |= (fp->byIsOpen) ? 0x80 : 0x00;

	// 3. byte2
	// Bit 7 = 1 if accessing logical records of size LRL
	// Bit 7 = 0 if accessing full sectors LRL = 0 (256)
	fcb->byte2 |= (fp->lrl == 0) ? 0x00: 0x80;
	// Bit 5 = 1 (Z80 sector buffer is not populated)
	fcb->byte2 |= 0x20;

	// other data
	fcb->buffaddr = fp->bufferAddress;
	fcb->lrl      = fp->lrl;

	// 5. System Identifiers for mapping back to RP2350 objects
	fcb->driveId  = fp->driveId;
	fcb->fileId   = fp->fileId;

	// Extract Next Pointer (24-bit)
	uint32_t nextPos = FileGetPos(fp);
	fcb->nextLSB = (BYTE)(nextPos & 0xFF);
	fcb->nextMOB = (BYTE)((nextPos >> 8) & 0xFF);
	fcb->nextMSB = (BYTE)((nextPos >> 16) & 0xFF);

	// Extract EOF/Size (24-bit)
	uint32_t fileSize = FileGetSize(fp);
	fcb->eofLSB = (BYTE)(fileSize & 0xFF);
	fcb->eofMOB = (BYTE)((fileSize >> 8) & 0xFF);
	fcb->eofMSB = (BYTE)((fileSize >> 16) & 0xFF);

	// Trailing Data
	memset(fcb->trailer, 0xFF, sizeof(fcb->trailer));
}

// ====================================================================

/**
 * Dumps a memory buffer to the serial console in a formatted hex view.
 * @param title A string title to print before the dump (e.g., "DATA2 FCB")
 * @param buf   Pointer to the data
 * @param len   Number of bytes to dump (e.g., 32)
 */
void debug_dump_buffer(const char* title, const byte* buf, size_t len) {
	printf("\r\n--- %s (%zu bytes) ---\n", title, len);
	// printf("Offset  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
	// printf("------------------------------------------------------------------\n");

	for (size_t i = 0; i < len; i += 16) {
		// Print the offset
		printf("%04X    ", (unsigned int)i);

		// Print Hex values
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				printf("%02X ", buf[i + j]);
			} else {
				printf("   "); // Padding for partial rows
			}
			if (j == 7) printf(" "); // Visual split at 8 bytes
		}

		printf(" ");

		// Print ASCII values
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				byte c = buf[i + j];
				// Check if character is printable, else print a dot
				printf("%c", isprint(c) ? c : '.');
			}
			if (j == 7) printf(" ");
		}
		printf("\n");
	}
	// printf("------------------------------------------------------------------\n");
}

/*
 * FILE -> Open
 * 1. TRS writes COMMAND2, SIZE2 set to 36
 * 2. TRS writes DATA2 (36 bytes)
 *       byte 0 : open options
 *       byte 1 : logical record length 0=256
 *       byte 2,3 : location of Buffer in TRS memory
 *       bytes 4..36 : fcb data from TRS
 * 3. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 * 5. DATA 2 contains
 *       bytes 0..31 : Actual FCB data
 * 6. FILEID contains the Id for Open File
 */
byte __not_in_flash_func(CMD_FILE_OPEN)(byte step) {

	if (step == 1) { // SET the number of bytes upfront.
       fhd_size2 = 32 + 4; // the fcb and 4 bytes "OPEN_REQUEST"
    }

    // after getting N Bytes - Signal Busy and defer processing.
    if (step == 2) return TRS_HARD_BUSY;

    if (step == 10) {

       // debug_dump_buffer("Open Request", (byte*)fhd_data2, 32);

       OPEN_REQUEST *openRequest = (OPEN_REQUEST *)fhd_data2;
       BYTE byMode = openRequest->byMode;

       Trs80PathResult res;
       trs80_resolve_path((char *)openRequest->fcb, 32, &res);

       printf("\r\nfileOpen(%s) ", res.primary);

       TFILE *file = FileOpenByName(res.primary, byMode);

    	bool allowSecondary =
			res.hasSecondary &&
			((byMode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) == 0);

    	if (file && !file->byIsOpen && allowSecondary) {
          file = FileOpenByName(res.secondary, byMode);
          if (file && file->byIsOpen) {
             printf("[USR:%s] ", res.secondary);
          }
       }

       if (file == NULL) {
          fhd_error2 = TRS_TOO_MANY_FILES;
          printf("Err=TooManyFiles ");
          return STATUS_ERROR;
       }

       fhd_error2 = file->lastResult;

       if (fhd_error2 == TRS_OK && file->byIsOpen) {

          printf("Id=%d Size=%lu ", file->fileId, (unsigned long)FileGetSize(file));

          // copy data from request into the file object
          file->lrl = openRequest->lrl;
          file->bufferAddress = openRequest->buffaddr;
          file->driveId = res.driveId; // extracted from the filename (0..9)
       	  memcpy(file->originalFCB, openRequest->fcb, sizeof(file->originalFCB));

          // sets all info into FCB for return
          FCB fcbData;
          FileToFCB(file, &fcbData);

          // copy FCB into Data and set size to 32
          fhd_size2 = sizeof(FCB);
          memcpy((void *)fhd_data2, &fcbData, fhd_size2);

          // debug_dump_buffer("Open Response", (byte*)fhd_data2, 32);

          // set the current File Id reg
          fhd_fileid = file->fileId;

          // Check if file is -> New / Empty (0 bytes)
          if (FileGetSize(file) == 0) {
             return STATUS_OKDATA | TRS_HARD_ZERO;
          } else {
             return STATUS_OKDATA;
          }
       } else {
          printf("Err=%d ", fhd_error2);
          return STATUS_ERROR; // not opened
       }
    }
    return STATUS_OK;
}

/*
 * FILE -> Close
 * 1. TRS80 writes FILEID with Id for Open File
 * 1. TRS writes COMMAND2
 * 2. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 * 5. TRS reads 32 bytes back containing the Original FCB (including filename)
 */
byte __not_in_flash_func(CMD_FILE_CLOSE)(byte step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		printf("\r\nfileClose(%d) ",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
			return STATUS_ERROR;
		}

		printf("Name=%s ) ",file->fname);

		fhd_error2 = FileClose(file);

		if (fhd_error2 == TRS_OK) {
			fhd_fileid = 0 ;

			// Copy the original into User data buffer for restore
			memcpy((void *)fhd_data2, file->originalFCB, sizeof(file->originalFCB));
			fhd_size2 = sizeof(file->originalFCB); // 32 bytes (0x20)

			// debug_dump_buffer("File Close Data", (byte*)fhd_data2, 32);

			return STATUS_OK;
		} else {
			printf ("Err=%d ",fhd_error2);
			return STATUS_ERROR; // not opened
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Kill
 * 1. TRS80 writes FILEID with Id for Open File
 * 1. TRS writes COMMAND2
 * 2. TRS waits
 * 4. TRS checks status for error (in ERROR2 if any)
 */
byte __not_in_flash_func(CMD_FILE_KILL)(byte step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		printf("\r\nfileKill(%d) ",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
			return STATUS_ERROR;
		}

		printf("\r\nfileClose(%s) ",file->fname);

		FileClose(file); // just ignore any error

		printf("& Kill ");

		// the filename is still present.
		fhd_error2 = FileDelete(file->fname);

		if (fhd_error2 == TRS_OK) {
			return STATUS_OK;
		} else {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Read
 * 1. TRS80 writes FILEID with Id for Open File
 * 2. TRS writes COMMAND2
 * 3. TRS waits
 * 4. TRS checks status.
 *      ERR : error in ERROR2 - including EOF Error, if no bytes
 *      DRQ : should be true otherwise
 * 5. TRS reads DATA2 (SIZE2 bytes) may be smaller if last page
 * Noting the Bytes read are defined by the LRL defined at OPEN
 */
byte __not_in_flash_func(CMD_FILE_READ)(byte step) {

//	printf("\r\nfR()");

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		// printf("\r\nfileRead(%d)",fhd_fileid);

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileRead(%d) Err=IllegalFileId) ",fhd_fileid);
			return STATUS_ERROR;
		}

		// actually read the data into the target data array starting at
		fhd_error2 = FileRead((void *)file, (void *)&fhd_data2, file->lrl);

		if (fhd_error2 == TRS_OK) {

			uint32_t actualRead = file->bytesReadWritten;

			if ( actualRead > 0 ) {

				// printf("\r\nfileRead(Bytes Read=%lu)",actualRead);
				// debug_dump_buffer("Read Data", (byte*)fhd_data2, 256);

				printf("-");

				// set the data length
				fhd_size2 = actualRead; // 8bit LRL where 256->0

				return STATUS_OKDATA; // got data
			} else {
				fhd_error2 = TRS_EOF_ENCOUNTERED;
				return STATUS_ERROR;
			}
		} else {
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/*
 * FILE -> Write
 * 1. File must have been opened by openfile()
 * 2. TRS writes COMMAND2
 * 3. TRS optionally sets SIZE2 (if not, default is LRL)
 * 4  TRS sends DATA2 (SIZE2) bytes to write
 * 5. TRS waits
 * 6. TRS checks status.
 *      ERR : error in ERROR2
 */
byte __not_in_flash_func(CMD_FILE_WRITE)(byte step) {

	if (step == 1) {
		// find the file object, using "fhd_fileid" set in IO Port
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			return STATUS_ERROR;
		}

		// set the default size based on files LRL
		fhd_size2 = file->lrl;
		// printf("\r\nfileWrite(Bytes=%d)", fhd_size2);

		return STATUS_OK;
	}

	// after getting N Bytes - Signal Busy and defer processing.
	if (step == 2) return TRS_HARD_BUSY;

	if (step == 10) {

		// printf("\r\nWriteFile(%d)",fhd_fileid);

		// find the file object, using "fhd_fileid" passed in IO Port
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileWrite(%d) Err=IllegalFileId) ",fhd_fileid);
			return STATUS_ERROR;
		}

		uint16_t expectedWrite = fhd_size2 == 0 ? 256 : fhd_size2;
		// debug_dump_buffer("Write Data", (byte*)fhd_data2, expectedWrite);

		// actually write the data from the Z80 DATA buffer TO the file
		fhd_error2 = FileWrite((void *)file, (void *)&fhd_data2, expectedWrite);

		if (fhd_error2 == TRS_OK) {

			// printf("\r\nfileWrite(Bytes Written=%i)",file->bytesReadWritten);
			uint32_t actualWrite = file->bytesReadWritten;

			if (actualWrite == expectedWrite) {
				printf("+");
				return STATUS_OK; // Write success
			} else {
				fhd_error2 = TRS_UNKNOWN_ERROR; // todo Map to Better Error
				return STATUS_ERROR;
			}
		} else {
			printf("\r\nfileWrite(%d) Err=%d ",fhd_fileid,fhd_error2);
			return STATUS_ERROR;
		}
	}
	return STATUS_OK;
}

/**
 * Generic Position Handler
 * Handles REW (SOF), POS (position), BACK, and EOF
 */
byte __not_in_flash_func(CMD_FILE_POSITION)(byte step) {

	if (step == 1) {
		if (fhd_command2==TRS_FILE_POS) {
			// POS Requires Z80 to send 2 byte Record Number
			fhd_size2 = 2;
			return STATUS_OK;
		} else {
			// all other commands are parameterless
			return TRS_HARD_BUSY;
		}
	}

	if (step == 2) {
		if (fhd_command2==TRS_FILE_POS) {
			return TRS_HARD_BUSY;
		}
	}

    if (step == 10) {

		printf("\r\nfilePOS(%i) ",fhd_fileid);

    	TFILE* file = findOpenFile(fhd_fileid);
        if (!file) {
            fhd_error2 = TRS_ILL_FILENUMBER;
			printf("Err=IllegalFileId) ");
            return STATUS_ERROR;
        }

    	uint64_t position = 0;

        switch (fhd_command2) {
            case TRS_FILE_SOF:
                position = 0;
                break;
            case TRS_FILE_POS: {
		            // Get the Logical Record Number from the buffer
            		uint16_t recordNum = *((uint16_t*)fhd_data2);
            		uint32_t recordSize = (file->lrl == 0) ? 256 : file->lrl;
            		position = (FSIZE_t)recordNum * recordSize;
				}
        		break;
            case TRS_FILE_BACK:
                // Calculate back one record, floor at 0
        		uint64_t current = f_tell(&file->f);
        		const uint16_t lrl16 = file->lrl == 0 ? 256 : file->lrl;
                if (current > lrl16) {
                    position = current - lrl16;
                } else {
                    position = 0;
                }
                break;
            case TRS_FILE_EOF:
                position = f_size(&file->f);
                break;
        	default:
        		position = f_tell(&file->f); // default to current POS
        		break;
        }

        fhd_error2 = FileSeek(file, position);

        if (fhd_error2 == TRS_OK) {

	    	printf("Pos=%llu ",position);

            // Prepare the 32-byte FCB for the Z80 to pull back
            FCB fcbData;
            FileToFCB(file, &fcbData);

        	// copy a subset of FCB data into the man data buffer
        	fhd_size2 = 9; // copy 9 bytes, starting at the fifth byte
        	memcpy((void *)fhd_data2, &((byte*)&fcbData)[5], fhd_size2);

            return STATUS_OKDATA;
        } else {
        	printf("Err=%d ",fhd_error2);
        	return STATUS_ERROR;
        }
    }
    return STATUS_OK;
}

/*
 * FILE -> Read FCB
 * 1. TRS80 writes FILEID with Id for Open File
 * 2. TRS writes COMMAND2
 * 3. TRS waits
 * 4. TRS checks status.
 *      ERR : error in ERROR2 - including EOF Error, if no bytes
 *      DRQ : should be true otherwise
 * 5. TRS reads DATA2 FCB, should be 9 bytes 5th to 13 byte
 */
byte __not_in_flash_func(CMD_FILE_FCB)(byte step) {

	if (step == 1) return TRS_HARD_BUSY;

	if (step == 10) {

		// find the file object
		TFILE* file = findOpenFile( fhd_fileid );

		if (!file) {
			fhd_error2 = TRS_ILL_FILENUMBER;
			printf("\r\nfileFCB(%d) Err=IllegalFileId", fhd_fileid);
			return STATUS_ERROR;
		}

		// printf("\r\nfileFCB(%s)",file->fname);

        // Prepare the 32-byte FCB for the Z80 to pull back
		FCB fcbData;
		FileToFCB(file, &fcbData);

		// copy a subset of FCB data into the man data buffer
		fhd_size2 = 9; // copy 9 bytes, starting at the fifth byte
		memcpy((void *)fhd_data2, &((byte*)&fcbData)[5], fhd_size2);

		// debug_dump_buffer("FCB Response", (byte*)fhd_data2, 32);

		return STATUS_OKDATA;
	}
	return STATUS_OK;
}

//
// Command Listing - NOTE We need this to be IN SRAM
//

const command_func_t __not_in_flash("frehd") command_table[MAX_COMMANDS] = {

	// Standard FreHD Commands
	[TRS_EXTRA_VERSION]		= CMD_VER,
	[TRS_EXTRA_OPENFILE]	= CMD_OPEN,
	[TRS_EXTRA_READFILE]	= CMD_READ,
	[TRS_EXTRA_WRITEFILE]	= CMD_WRITE,
	[TRS_EXTRA_CLOSEFILE]	= CMD_CLOSE,

	// General Purpose  ( $10 - $13 )
	[TRS_NEWER_DIR]			= CMD_DIR,

	// File Control ( $14 - $17 )
	[TRS_FILE_OPEN]			= CMD_FILE_OPEN,
	[TRS_FILE_CLOSE]		= CMD_FILE_CLOSE,
	[TRS_FILE_KILL]			= CMD_FILE_KILL,

	// Directory Command ( $18 - $1F )
	[TRS_NEWER_ASSIGN]	    = CMD_ASSIGN,
	[TRS_NEWER_CHANGEDIR]	= CMD_CHANGEDIR,
	[TRS_NEWER_COPYFILE]	= CMD_COPYFILE, // todo add :N handling
	[TRS_NEWER_DELETE]		= CMD_DELETE, // todo add :N handling
	[TRS_NEWER_MAKEDIR]		= CMD_MAKEEDIR,
	[TRS_NEWER_RENAME]		= CMD_RENAME, // todo add :N handling
	[TRS_NEWER_PWD]			= CMD_PWD,

	// File Handling Commands ( $20 - $2F )
	[TRS_FILE_READ]			= CMD_FILE_READ,
	[TRS_FILE_WRITE]		= CMD_FILE_WRITE,
	[TRS_FILE_SOF]			= CMD_FILE_POSITION,
	[TRS_FILE_POS]			= CMD_FILE_POSITION,
	[TRS_FILE_BACK]			= CMD_FILE_POSITION,
	[TRS_FILE_EOF]			= CMD_FILE_POSITION,
	[TRS_FILE_FCB]			= CMD_FILE_FCB

};
//-----------------------------------------------------------------------------

void __not_in_flash_func(command_dispatch)(byte step) {

	// reject if out of bounds
	if (__builtin_expect(fhd_command2 >= MAX_COMMANDS, 0)) {
    	// printf("\r\nUnknown Command %i",fhd_command2);
		return;
	}

    command_func_t func = command_table[fhd_command2]; // get the command
    if (func == NULL) {
    	// printf("\r\nUnknown Command %i",fhd_command2);
    	return; // if NO command, then nothing to do
    }

    // Actually EXECUTE the command
    byte status_result = func(step);

	// Ensure all memory writes before status changes as
	// this is a handoff between processing threads
	__DSB(); // Data Sync Barrier

	// if the process Z80 Side signals BUSY then we set this State
	// otherwise if any other status we must ensure BUST is NOT Set
	if (status_result == TRS_HARD_BUSY) {
		fhd_status = TRS_HARD_BUSY;
	} else {
	    fhd_status = status_result & ~TRS_HARD_BUSY;
		if (fhd_status == STATUS_ERROR) {
			fhd_size2 = 0;
		}
	}

}

//-----------------------------------------------------------------------------
// ONLY USED by Z80 Core

// BOOT loader Flags used by Z80 Access
static bool  fhd_bootMode = false;
static int	 fhd_bootIndex = 0;

// Index pointer into DATA2 for Z80 R/W
static byte  fhd_data2Index = 0;

//-----------------------------------------------------------------------------
void __not_in_flash_func(control_port_out)(word addr, byte data)
{
	switch (addr) {
		case DATA2:
			fhd_data2[fhd_data2Index++] = data; // store the data byte into the buffer
			// fhd_data2[fhd_data2Index  ] = 0; 	// precaution, puts a null after last byte
			// printf(" %d",fhd_data2Index);
			// STEP 2 : When all (size2) bytes have been received dispatch
			if (fhd_data2Index==fhd_size2) command_dispatch(2);

			break;
		case SIZE2:
			fhd_size2 = data;
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			break;
		case COMMAND2:
			fhd_command2 = data;
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			fhd_bootMode = false; // boot mode should be disabled

			// STEP 1 : After receiving the command
			command_dispatch(1);

			break;
		case FILEID2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			fhd_fileid = data;
			break;

		case WRITEROM: // (OUT on ERROR2) Bootloader Initiate Command
			if (g_byEnableFreHDBoot) { // only if enabled
				fhd_bootMode = true; // boot loader Mode
				fhd_bootIndex = 0; // reset the data pointer
				boot_data_block[2] = data; // byte from out command.
			}
		 	break;
	}
}

//-----------------------------------------------------------------------------
byte __not_in_flash_func(control_port_in)(word addr)
{
    byte data = 0xFF;

	switch (addr)
	{
		case DATA2:
			data = fhd_data2[fhd_data2Index++];
			if (fhd_data2Index==fhd_size2) {   // if sent SIZE bytes out of the buffer
				command_dispatch(3); // STEP 3 : When all bytes have been Sent Back
			}
			break;
		case SIZE2:
			// reading the Size would indicate we are about to read the BYTES
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_size2; // return the size2 register
			break;
		case READROM: // (In on COMMAND2) Bootloader Data Read
			if (fhd_bootMode) {
				// if correct mode then return data
				data = boot_data_block[fhd_bootIndex++];
			}
			break;
		case ERROR2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_error2;
			break;
		case STATUS:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_status;
			break;
		case FILEID2:
			fhd_data2Index = 0; // pointer to the beginning of data buffer
			data = fhd_fileid;
			break;
	}

	return data;
}

//-----------------------------------------------------------------------------

void __not_in_flash_func(ServicePortsStateMachine)(void) {

	// IF Busy Flag Set then we need process a command
	if ( fhd_status == TRS_HARD_BUSY ) {

		// STEP 10; Service the command by CORE 0
		command_dispatch(10);
	}

}

