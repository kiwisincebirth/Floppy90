//
// Support for The TRS-80 COPY command.
// Created by Mark Pruden on 19/11/2026.
//

#include "file_cmnds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "defines.h"
#include "ff.h"
#include "file.h"


/* Index 0 is always NULL; indices 1..9 hold configured directory paths */
static char s_mappedDirStorage[10][TRS80_PATH_MAX];

static const char *s_mappedDirs[10] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};


/**
 * Loads TRS-80 drive assignment mappings from the system assignment
 * configuration file.
 *
 * Reads `/SYS/ASSIGN.CFG`, ignores blank lines and comment lines beginning
 * with `;` or `#`, and processes assignment entries in the form `N:path`,
 * where `N` is a drive number from 1 through 9. Leading and trailing
 * whitespace is ignored. An entry with no path clears the mapped directory
 * for the specified drive.
 */
void trs80_load_assign_cfg(void)
{
    char szLine[TRS80_PATH_MAX + 8];

    TFILE *f = FileOpenByName("/SYS/ASSIGN.CFG", FA_READ);
    if (f == NULL || !f->byIsOpen) {
        if (f) FileClose(f);
        return;
    }

    while (FileReadLine(f, szLine, (int)(sizeof(szLine) - 1)) >= 0) {
        char *p = szLine;

        /* Trim trailing CR/LF/spaces */
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n' ||
                           p[len - 1] == ' '  || p[len - 1] == '\t')) {
            p[--len] = '\0';
                           }

        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0' || *p == ';' || *p == '#') {
            continue;
        }

        if (p[0] >= '1' && p[0] <= '9' && p[1] == ':') {
            uint8_t driveId = (uint8_t)(p[0] - '0');
            const char *path = p + 2;
            while (*path == ' ' || *path == '\t') {
                path++;
            }

            if (*path == '\0') {
                trs80_set_mapped_dir(driveId, NULL);
            } else {
                trs80_set_mapped_dir(driveId, path);
            }
        }
    }

    FileClose(f);
}

/**
 * Saves the current TRS-80 drive assignment mappings to the system assignment
 * configuration file.
 *
 * Recreates `/SYS/ASSIGN.CFG` and writes one entry for each mapped drive from
 * 1 through 9. Drives without a mapped directory are omitted. Each saved entry
 * uses the `N:path` format and is terminated with CRLF.
 *
 * @return true if the configuration file was opened, written, synchronized,
 * and closed successfully; false if the file could not be opened or any write
 * or synchronization operation failed.
 */
bool trs80_save_assign_cfg(void)
{
    TFILE *f = FileOpenByName("/SYS/ASSIGN.CFG", FA_CREATE_ALWAYS | FA_WRITE);
    if (f == NULL || !f->byIsOpen) {
        if (f) FileClose(f);
        return false;
    }

    bool ok = true;
    for (uint8_t d = 1; d <= 9; d++) {
        const char *path = trs80_get_mapped_dir(d);
        if (path != NULL && path[0] != '\0') {
            char line[TRS80_PATH_MAX + 8];
            int n = snprintf(line, sizeof(line), "%u:%s\r\n", (unsigned int)d, path);
            if (n > 0) {
                UINT bw = 0;
                FRESULT fr = f_write(&f->f, line, (UINT)n, &bw);
                if (fr != FR_OK || bw != (UINT)n) {
                    ok = false;
                    break;
                }
            }
        }
    }

    if (f_sync(&f->f) != FR_OK) {
        ok = false;
    }

    FileClose(f);
    return ok;
}

// ============================

static uint8_t sanitize_drive_id(int rawDrive)
{
    if (rawDrive < 1 || rawDrive > 9) {
        return 0;
    }
    return (uint8_t)rawDrive;
}

const char *trs80_get_mapped_dir(uint8_t driveId)
{
    if (driveId < 1 || driveId > 9) {
        return NULL;
    }
    return s_mappedDirs[driveId];
}

static const char *get_mapped_dir(uint8_t driveId)
{
    uint8_t validId = sanitize_drive_id((int)driveId);
    if (validId == 0) {
        return NULL;
    }
    if (s_mappedDirs[validId] == NULL || s_mappedDirs[validId][0] == '\0') {
        return NULL;
    }
    return s_mappedDirs[validId];
}

static bool append_str(char *dst, size_t dstSize, size_t *pos, const char *src, bool *truncated)
{
    if (dstSize == 0) {
        if (truncated) *truncated = true;
        return false;
    }
    while (*src != '\0') {
        if (*pos + 1 >= dstSize) {
            if (truncated) *truncated = true;
            dst[*pos] = '\0';
            return false;
        }
        dst[(*pos)++] = *src++;
    }
    dst[*pos] = '\0';
    return true;
}

static void join_dir_and_name(
    const char *prefix,
    const char *name,
    bool trimTrailingSlashOnEmptyName,
    char *out,
    size_t outSize,
    bool *truncated)
{
    size_t pos = 0;
    if (outSize == 0) {
        if (truncated) *truncated = true;
        return;
    }
    out[0] = '\0';

    if (prefix == NULL || prefix[0] == '\0') {
        append_str(out, outSize, &pos, name ? name : "", truncated);
        return;
    }

    /* Copy prefix, stripping trailing '/' unless prefix is "/" */
    size_t pLen = strlen(prefix);
    while (pLen > 1 && prefix[pLen - 1] == '/') {
        pLen--;
    }

    for (size_t i = 0; i < pLen; i++) {
        if (pos + 1 >= outSize) {
            if (truncated) *truncated = true;
            out[pos] = '\0';
            return;
        }
        out[pos++] = prefix[i];
    }
    out[pos] = '\0';

    const char *cleanName = name ? name : "";
    while (*cleanName == '/') {
        cleanName++;
    }

    if (*cleanName != '\0') {
        if (pos == 0 || out[pos - 1] != '/') {
            if (!append_str(out, outSize, &pos, "/", truncated)) {
                return;
            }
        }
        append_str(out, outSize, &pos, cleanName, truncated);
    } else if (!trimTrailingSlashOnEmptyName) {
        if (pos == 0 || out[pos - 1] != '/') {
            append_str(out, outSize, &pos, "/", truncated);
        }
    }
}

static void apply_legacy_slash_extension(char *buf, size_t len, int lastSlashIdx)
{
    if (lastSlashIdx < 0 || (size_t)lastSlashIdx >= len) {
        return;
    }

    bool dotInFinalSegment = false;
    for (size_t i = (size_t)lastSlashIdx + 1; i < len; i++) {
        if (buf[i] == '.') {
            dotInFinalSegment = true;
            break;
        }
    }

    if (!dotInFinalSegment) {
        int charsAfterSlash = ((int)len - 1) - lastSlashIdx;
        if (charsAfterSlash > 0 && charsAfterSlash <= 3) {
            buf[lastSlashIdx] = '.';
        }
    }
}

static bool should_truncate_password_dot(
    const char *src,
    int sIdx,
    int maxLen,
    const char *dest,
    int dIdx,
    int lastSlashIdx,
    bool hasDot)
{
    bool afterSlashExt = (lastSlashIdx != -1 && ((dIdx - 1 - lastSlashIdx) <= 3));
    if (!hasDot && !afterSlashExt) {
        return false;
    }
    if (dIdx <= 0 || dest[dIdx - 1] == '.') {
        return false;
    }

    int lookAhead = 0;
    while ((sIdx + lookAhead) < maxLen &&
           isalnum((unsigned char)src[sIdx + lookAhead])) {
        lookAhead++;
    }
    return (lookAhead >= 4);
}

static void parse_raw_token(
    const char *src,
    int maxLen,
    char *sanitized,
    size_t sanitizedSize,
    uint8_t *outDriveId,
    bool *outHasExplicitSlash,
    bool *outTruncated)
{
    int sIdx = 0;
    size_t dIdx = 0;
    int lastSlashIdx = -1;
    bool hasDot = false;
    uint8_t driveId = 0;
    bool hasSlash = false;

    sanitized[0] = '\0';

    if (src == NULL || maxLen <= 0 || sanitizedSize == 0) {
        *outDriveId = 0;
        *outHasExplicitSlash = false;
        return;
    }

    while (sIdx < maxLen) {
        unsigned char c = (unsigned char)src[sIdx++];

        if (c == 0x0D || c == 0x03 || c == 0x00) {
            break;
        }

        if (c == ':') {
            if (sIdx < maxLen && isdigit((unsigned char)src[sIdx])) {
                driveId = sanitize_drive_id(src[sIdx] - '0');
            }
            break;
        }

        if (c == '.') {
            if (should_truncate_password_dot(
                    src, sIdx, maxLen, sanitized, (int)dIdx, lastSlashIdx, hasDot)) {
                break;
            }
        }

        if (isalnum(c) || c == '/' || c == '.' || c == '*') {
            char up = (char)toupper(c);
            if (up == '/') {
                hasSlash = true;
                lastSlashIdx = (int)dIdx;
                hasDot = false;
            } else if (up == '.') {
                hasDot = true;
            }

            if (dIdx + 1 < sanitizedSize) {
                sanitized[dIdx++] = up;
                sanitized[dIdx] = '\0';
            } else {
                *outTruncated = true;
            }
        }
    }

    apply_legacy_slash_extension(sanitized, dIdx, lastSlashIdx);

    *outDriveId = driveId;
    *outHasExplicitSlash = hasSlash;
}

void trs80_set_mapped_dir(uint8_t driveId, const char *dirPath)
{
    /* Slot 0 is permanently NULL; only 1..9 are valid */
    s_mappedDirs[0] = NULL;
    uint8_t validId = sanitize_drive_id((int)driveId);
    if (validId == 0) {
        return;
    }

    if (dirPath == NULL || dirPath[0] == '\0') {
        s_mappedDirStorage[validId][0] = '\0';
        s_mappedDirs[validId] = NULL;
        return;
    }

    size_t len = strlen(dirPath);
    if (len >= TRS80_PATH_MAX) {
        len = TRS80_PATH_MAX - 1;
    }
    memcpy(s_mappedDirStorage[validId], dirPath, len);
    s_mappedDirStorage[validId][len] = '\0';
    s_mappedDirs[validId] = s_mappedDirStorage[validId];
}

void trs80_resolve_path(const char *src, int maxLen, Trs80PathResult *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    s_mappedDirs[0] = NULL;

    parse_raw_token(
        src,
        maxLen,
        out->sanitized,
        sizeof(out->sanitized),
        &out->driveId,
        &out->hasExplicitSlash,
        &out->truncated);

    const char *mapped = get_mapped_dir(out->driveId);
    if (mapped != NULL) {
        out->hasMappedDir = true;
        join_dir_and_name(
            mapped,
            out->sanitized,
            true,
            out->primary,
            sizeof(out->primary),
            &out->truncated);
    } else {
        out->hasMappedDir = false;
        size_t pos = 0;
        append_str(out->primary, sizeof(out->primary), &pos, out->sanitized, &out->truncated);
    }

    /* /USR/ fallback applies only to bare names with no explicit :1..:9 or / path */
    if (out->driveId == 0 && !out->hasExplicitSlash && out->sanitized[0] != '\0') {
        join_dir_and_name(
            "/USR",
            out->sanitized,
            false,
            out->secondary,
            sizeof(out->secondary),
            &out->truncated);
        out->hasSecondary = (out->secondary[0] != '\0');
    } else {
        out->hasSecondary = false;
        out->secondary[0] = '\0';
    }
}

/*
 * parse_dir_target
 *
 * Intent:
 *   Parses a DIR command argument string into a target FatFs directory path and
 *   a wildcard/filter pattern, supporting TRSDOS-style postfixed drive numbers
 *   (:0..:9) and explicit path prefixes (/, ./, ../) while rejecting invalid
 *   combinations.
 *
 * Supported Usage Patterns:
 *   - DIR                  -> dir = ".", pattern = "*"
 *   - DIR *.BAS            -> dir = ".", pattern = "*.BAS"
 *   - DIR :2               -> dir = <mapped drive 2>, pattern = "*"
 *   - DIR *.BAS:2          -> dir = <mapped drive 2>, pattern = "*.BAS"
 *   - DIR /SYS             -> dir = "/SYS", pattern = "*" (if /SYS is a dir)
 *   - DIR /SYS/*.SYS       -> dir = "/SYS", pattern = "*.SYS"
 *   - DIR ./SUB            -> dir = "./SUB", pattern = "*" (if ./SUB is a dir)
 *   - DIR ../OTHER         -> dir = "<parent of CWD>/OTHER", pattern = "*"
 *
 * Rejected / Error Cases (*errFlag = true):
 *   - Mixing explicit path prefixes (/, ./, ../) with drive numbers :1..:9
 *     (for example, "DIR /TEMP:4").
 *   - Requesting a logical drive :1..:9 that has no active ASSIGN mapping.
 *
 * Arguments:
 *   rawInput       - Raw null-terminated argument string from Z80/fhd_data2
 *                    (may include leading/trailing spaces or CR/LF/0x03).
 *   outDir         - Output buffer receiving the resolved directory path.
 *   outDirSize     - Byte capacity of outDir.
 *   outPattern     - Output buffer receiving the wildcard/name filter pattern.
 *   outPatternSize - Byte capacity of outPattern.
 *   errFlag        - Output flag set to true if syntax, drive mapping, or
 *                    conflicting path/drive rules fail; false on valid parse.
 */
BYTE parse_dir_target(
    const char *rawInput,
    char *outDir,
    size_t outDirSize,
    char *outPattern,
    size_t outPatternSize)
{
    char buf[TRS80_PATH_MAX];

    if (outDir && outDirSize > 0) outDir[0] = '\0';
    if (outPattern && outPatternSize > 0) outPattern[0] = '\0';

    snprintf(buf, sizeof(buf), "%s", rawInput ? rawInput : "");

    /* Trim trailing whitespace or control terminators */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t' ||
                       buf[len - 1] == '\r' || buf[len - 1] == '\n' ||
                       buf[len - 1] == 0x03)) {
        buf[--len] = '\0';
    }

    /* Trim leading spaces */
    char *p = buf;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    len = strlen(p);

    /* 1. Extract optional trailing :N (0..9) */
    uint8_t driveId = 0;
    bool hasDriveSuffix = false;
    if (len >= 2 && p[len - 2] == ':' && p[len - 1] >= '0' && p[len - 1] <= '9') {
        driveId = (uint8_t)(p[len - 1] - '0');
        hasDriveSuffix = true;
        p[len - 2] = '\0';
        len -= 2;
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
            p[--len] = '\0';
        }
    }

    bool hasExplicitPrefix =
        (p[0] == '/') ||
        (strcmp(p, ".") == 0) ||
        (strncmp(p, "./", 2) == 0) ||
        (strcmp(p, "..") == 0) ||
        (strcmp(p, "../") == 0) ||
        (strncmp(p, "../", 3) == 0);

    /* Reject illogical combinations like DIR /TEMP:4 */
    if (hasDriveSuffix && driveId >= 1 && hasExplicitPrefix) {
        return TRS_ILLEGAL_FILENAME;
    }

    /* Empty input -> default dir, wildcard "*" */
    if (*p == '\0') {
        if (hasDriveSuffix && driveId >= 1) {
            const char *mapped = trs80_get_mapped_dir(driveId);
            if (!mapped || mapped[0] == '\0') {
                return TRS_DEVICE_NOT_AVAIL;
            }
            snprintf(outDir, outDirSize, "%s", mapped);
        } else {
            snprintf(outDir, outDirSize, ".");
        }
        snprintf(outPattern, outPatternSize, "*");
        return TRS_OK;
    }

    /* 2. Explicit path prefix (/ , ./ , ../) */
    if (hasExplicitPrefix) {
        char resolved[TRS80_PATH_MAX];

        if (p[0] == '/') {
            snprintf(resolved, sizeof(resolved), "%s", p);
        } else if (strcmp(p, ".") == 0 || strncmp(p, "./", 2) == 0) {
            const char *sub = (p[0] == '.') ? (p + 1) : p;
            if (*sub == '\0') {
                snprintf(resolved, sizeof(resolved), ".");
            } else {
                snprintf(resolved, sizeof(resolved), ".%s", sub);
            }
        } else {
            /* .. / ../ / ../... handling via normalized CWD */
            char normCwd[TRS80_PATH_MAX];
            if (get_normalized_cwd(normCwd, sizeof(normCwd), NULL) != FR_OK || normCwd[0] == '\0') {
                snprintf(normCwd, sizeof(normCwd), "/");
            }

            /* Step up one directory level */
            char *lastSlash = strrchr(normCwd, '/');
            if (lastSlash && lastSlash != normCwd) {
                *lastSlash = '\0';
            } else {
                snprintf(normCwd, sizeof(normCwd), "/");
            }

            /* Extract optional suffix after "../" */
            const char *suffix = "";
            if (strncmp(p, "../", 3) == 0) {
                suffix = p + 3;
            }
            while (*suffix == '/') {
                suffix++;
            }

            if (strcmp(normCwd, "/") == 0) {
                if (*suffix != '\0') {
                    snprintf(resolved, sizeof(resolved), "/%s", suffix);
                } else {
                    snprintf(resolved, sizeof(resolved), "/");
                }
            } else {
                if (*suffix != '\0') {
                    snprintf(resolved, sizeof(resolved), "%s/%s", normCwd, suffix);
                } else {
                    snprintf(resolved, sizeof(resolved), "%s", normCwd);
                }
            }
        }

        /* Strip trailing slash except for lone "/" */
        size_t rlen = strlen(resolved);
        while (rlen > 1 && resolved[rlen - 1] == '/') {
            resolved[--rlen] = '\0';
        }

        FILINFO st;
        if (f_stat(resolved, &st) == FR_OK && (st.fattrib & AM_DIR)) {
            snprintf(outDir, outDirSize, "%s", resolved);
            snprintf(outPattern, outPatternSize, "*");
            return TRS_OK;
        }

        /* Split parent dir and trailing filter/pattern */
        char *slash = strrchr(resolved, '/');
        if (slash) {
            const char *pat = slash + 1;
            if (slash == resolved) {
                snprintf(outDir, outDirSize, "/");
            } else {
                *slash = '\0';
                snprintf(outDir, outDirSize, "%s", resolved);
            }
            snprintf(outPattern, outPatternSize, "%s", (*pat != '\0') ? pat : "*");
        } else {
            snprintf(outDir, outDirSize, ".");
            snprintf(outPattern, outPatternSize, "%s", resolved);
        }
        return TRS_OK;
    }

    /* 3. Non-prefixed filter/wildcard with optional :N */
    if (hasDriveSuffix && driveId >= 1) {
        const char *mapped = trs80_get_mapped_dir(driveId);
        if (!mapped || mapped[0] == '\0') {
            return TRS_DEVICE_NOT_AVAIL;
        }
        snprintf(outDir, outDirSize, "%s", mapped);
    } else {
        snprintf(outDir, outDirSize, ".");
    }

    snprintf(outPattern, outPatternSize, "%s", p);
    return TRS_OK;
}

/*
 * resolve_delete_spec
 *
 * Intent:
 *   Resolves a single DEL argument token (supporting :0..:9 drive suffixes,
 *   ASSIGN mappings, /, ./, ../ prefixes, and wildcards) into a fully
 *   qualified FatFs file/wildcard path for DeleteFileSpec().
 */
BYTE resolve_delete_spec(
    const char *token,
    char *outSpec,
    size_t outSpecSize)
{
    char dirBuf[TRS80_PATH_MAX];
    char patBuf[TRS80_PATH_MAX];

    if (token == NULL || token[0] == '\0' || outSpec == NULL || outSpecSize == 0) {
        return TRS_ILLEGAL_FILENAME;
    }
    outSpec[0] = '\0';

    BYTE parseErr = parse_dir_target(
        token,
        dirBuf,
        sizeof(dirBuf),
        patBuf,
        sizeof(patBuf));

    if (parseErr!=TRS_OK) return parseErr;

    if (dirBuf[0] == '\0' || patBuf[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    /* If token targeted a directory directly (e.g. /DIR or ..) with default "*",
     * guard against accidental wildcard deletion of everything in that folder. */
    if (strcmp(patBuf, "*") == 0) {
        FILINFO st;
        if (f_stat(dirBuf, &st) == FR_OK && (st.fattrib & AM_DIR)) {
            return TRS_ACCESS_DENIED;
        }
    }

    /* Normalize "." to current working directory so joined paths are explicit */
    char baseDir[TRS80_PATH_MAX];
    if (strcmp(dirBuf, ".") == 0) {
        if (get_normalized_cwd(baseDir, sizeof(baseDir), NULL) != FR_OK || baseDir[0] == '\0') {
            snprintf(baseDir, sizeof(baseDir), "/");
        }
    } else {
        snprintf(baseDir, sizeof(baseDir), "%s", dirBuf);
    }

    /* Strip trailing slashes except root "/" */
    size_t blen = strlen(baseDir);
    while (blen > 1 && baseDir[blen - 1] == '/') {
        baseDir[--blen] = '\0';
    }

    /* Trim leading slashes on pattern */
    const char *pat = patBuf;
    while (*pat == '/') {
        pat++;
    }
    if (*pat == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    if (strcmp(baseDir, "/") == 0) {
        snprintf(outSpec, outSpecSize, "/%s", pat);
    } else {
        snprintf(outSpec, outSpecSize, "%s/%s", baseDir, pat);
    }

    return TRS_OK;
}

// ========================================================
// COPY SINGLE FILE
// ========================================================

// Copy Buffer
static uint8_t s_copyBuf[COPY_CHUNK_SIZE] __attribute__((aligned(4)));

/**
 * Copies a single file from srcPath to dstPath.
 * Returns FR_OK on success, or a FatFs FRESULT error code.
 */
FRESULT CopySingleFile(const char *srcPath, const char *dstPath) {
    if (!srcPath || !dstPath || srcPath[0] == '\0' || dstPath[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    // Prevent copying a file directly onto itself if paths match identically
    if (strcmp(srcPath, dstPath) == 0) {
        return TRS_ILLEGAL_FILENAME;
    }

    FIL *fsrc = (FIL *)malloc(sizeof(FIL));
    FIL *fdst = (FIL *)malloc(sizeof(FIL));

    if (!fsrc || !fdst) {
        free(fsrc);
        free(fdst);
        return TRS_DEVICE_NOT_AVAIL;
    }

    FRESULT fr = f_open(fsrc, srcPath, FA_READ);
    if (fr != FR_OK) {
        free(fsrc);
        free(fdst);
        return trans(fr);
    }

    // Non-interactive: create or overwrite destination file
    fr = f_open(fdst, dstPath, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        f_close(fsrc);
        free(fsrc);
        free(fdst);
        return trans(fr);
    }

    UINT br = 0, bw = 0;
    while (1) {
        fr = f_read(fsrc, s_copyBuf, COPY_CHUNK_SIZE, &br);
        if (fr != FR_OK) {
            break;
        }
        if (br == 0) {
            // Reached EOF cleanly
            fr = FR_OK;
            break;
        }

        fr = f_write(fdst, s_copyBuf, br, &bw);
        if (fr != FR_OK || bw < br) {
            if (fr == FR_OK) {
                fr = trans(FR_DISK_ERR);
            }
            break;
        }
    }

    FRESULT frCloseDst = f_close(fdst);
    FRESULT frCloseSrc = f_close(fsrc);

    free(fsrc);
    free(fdst);

    if (fr != FR_OK) return trans(fr);
    if (frCloseDst != FR_OK) return trans(frCloseDst);
    if (frCloseSrc != FR_OK) return trans(frCloseSrc);

    return FR_OK;
}

// ---------------------------------------------

static bool MatchWildcard(const char *pattern, const char *str) {
    const char *p = pattern;
    const char *s = str;
    const char *starP = NULL;
    const char *starS = NULL;

    while (*s) {
        if (*p == '*') {
            starP = p++;
            starS = s;
        } else if (toupper((unsigned char)*p) == toupper((unsigned char)*s)) {
            p++;
            s++;
        } else if (starP) {
            p = starP + 1;
            s = ++starS;
        } else {
            return false;
        }
    }

    while (*p == '*') {
        p++;
    }

    return (*p == '\0');
}

static bool HasWildcard(const char *s) {
    return (strchr(s, '*') != NULL);
}

static bool EndsWithSep(const char *s) {
    size_t len = strlen(s);
    if (len == 0) return false;
    char last = s[len - 1];
    return (last == '/' || last == '\\');
}

static bool JoinPath(char *outBuf, size_t outSize, const char *dir, const char *name) {
    size_t dirLen = strlen(dir);
    bool needSep = (dirLen > 0 && !EndsWithSep(dir) && dir[dirLen - 1] != ':');

    int written = snprintf(
        outBuf,
        outSize,
        needSep ? "%s/%s" : "%s%s",
        dir,
        name
    );

    return (written > 0 && (size_t)written < outSize);
}

static void SplitNameExt(const char *name, char *stem, size_t stemCap, char *ext, size_t extCap)
{
    const char *dot = strrchr(name, '.');
    if (dot && dot != name) {
        size_t stemLen = (size_t)(dot - name);
        if (stemLen >= stemCap) stemLen = stemCap - 1;
        memcpy(stem, name, stemLen);
        stem[stemLen] = '\0';

        snprintf(ext, extCap, "%s", dot + 1);
    } else {
        snprintf(stem, stemCap, "%s", name);
        if (extCap > 0) ext[0] = '\0';
    }
}

static bool ApplyWildcardSegment(const char *patSeg, const char *srcSeg, char *outSeg, size_t outCap)
{
    const char *star = strchr(patSeg, '*');
    if (!star) {
        // Literal segment (e.g., "HELLO" or "OLD")
        if (strlen(patSeg) >= outCap) return false;
        snprintf(outSeg, outCap, "%s", patSeg);
        return true;
    }

    // Wildcard '*' in segment: prefix + srcSeg + suffix
    size_t prefixLen = (size_t)(star - patSeg);
    const char *suffix = star + 1;
    size_t suffixLen = strlen(suffix);
    size_t srcLen = strlen(srcSeg);

    // Optional: if srcSeg starts with prefix, strip prefix to avoid duplicating it
    // e.g., src="MYDB1", pat="MYDB*" -> prefix="MYDB", rest="1" -> "MYDB1"
    const char *insertStr = srcSeg;
    size_t insertLen = srcLen;
    if (prefixLen > 0 && strncasecmp(srcSeg, patSeg, prefixLen) == 0 && srcLen >= prefixLen) {
        insertStr = srcSeg + prefixLen;
        insertLen = srcLen - prefixLen;
    }

    if (prefixLen + insertLen + suffixLen >= outCap) {
        return false;
    }

    size_t pos = 0;
    memcpy(outSeg + pos, patSeg, prefixLen);
    pos += prefixLen;
    memcpy(outSeg + pos, insertStr, insertLen);
    pos += insertLen;
    memcpy(outSeg + pos, suffix, suffixLen);
    pos += suffixLen;
    outSeg[pos] = '\0';
    return true;
}

/**
 * Maps a matched filename using a destination wildcard pattern (e.g. "*.BAK").
 * Uses the text before '*' as prefix and after '*' as suffix.
 */
static bool ApplyDestWildcard(const char *dstPat, const char *srcFilename, char *outName, size_t outCap)
{
    char srcStem[32], srcExt[16];
    char patStem[32], patExt[16];
    char outStem[32], outExt[16];

    SplitNameExt(srcFilename, srcStem, sizeof(srcStem), srcExt, sizeof(srcExt));
    SplitNameExt(dstPat, patStem, sizeof(patStem), patExt, sizeof(patExt));

    if (!ApplyWildcardSegment(patStem, srcStem, outStem, sizeof(outStem))) {
        return false;
    }

    // If destination pattern had no dot (e.g. "NEWNAME"), keep source extension if appropriate
    // or leave empty when extension pattern is empty
    const char *dstDot = strchr(dstPat, '.');
    if (dstDot) {
        if (!ApplyWildcardSegment(patExt, srcExt, outExt, sizeof(outExt))) {
            return false;
        }
    } else {
        outExt[0] = '\0';
    }

    if (outExt[0] != '\0') {
        int n = snprintf(outName, outCap, "%s.%s", outStem, outExt);
        return (n > 0 && (size_t)n < outCap);
    } else {
        int n = snprintf(outName, outCap, "%s", outStem);
        return (n > 0 && (size_t)n < outCap);
    }
}

static void SplitSrcSpec(const char *srcSpec, char *dirPart, size_t dirSize, char *patPart, size_t patSize) {
    const char *lastSlash = strrchr(srcSpec, '/');
    const char *lastBSlash = strrchr(srcSpec, '\\');
    const char *sep = lastSlash;
    if (lastBSlash && (!sep || lastBSlash > sep)) {
        sep = lastBSlash;
    }

    if (sep) {
        size_t dLen = (size_t)(sep - srcSpec);
        if (dLen == 0 || (dLen == 2 && srcSpec[1] == ':')) {
            dLen++;
        }
        if (dLen >= dirSize) dLen = dirSize - 1;
        memcpy(dirPart, srcSpec, dLen);
        dirPart[dLen] = '\0';

        snprintf(patPart, patSize, "%s", sep + 1);
    } else {
        const char *colon = strchr(srcSpec, ':');
        if (colon) {
            size_t dLen = (size_t)(colon - srcSpec) + 1;
            if (dLen >= dirSize) dLen = dirSize - 1;
            memcpy(dirPart, srcSpec, dLen);
            dirPart[dLen] = '\0';
            snprintf(patPart, patSize, "%s", colon + 1);
        } else {
            snprintf(dirPart, dirSize, ".");
            snprintf(patPart, patSize, "%s", srcSpec);
        }
    }
}

/* Reuse or share MatchWildcard, HasWildcard, EndsWithSep, JoinPath, SplitSrcSpec
 * with copyfile.c so wildcard and path rules stay identical across COPY and DEL.
 */

static bool ShouldSkipEntry(const FILINFO *fno, bool allowDirs) {
    if (!fno || fno->fname[0] == '\0') {
        return true;
    }
    if (strcmp(fno->fname, ".") == 0 || strcmp(fno->fname, "..") == 0) {
        return true;
    }
    if (!allowDirs && (fno->fattrib & AM_DIR)) {
        return true;
    }
    if (fno->fattrib & (AM_HID | AM_SYS)) {
        return true;
    }
    if (strncmp(fno->fname, "._", 2) == 0) {
        return true;
    }
    return false;
}

// ========================================================
// COPY FILE(s)
// ========================================================

/**
 * Copies one or more files matching srcSpec to dstSpec.
 * Returns FR_OK on full completion (including 0 matches), or the error code on failure.
 */
FRESULT CopyFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *copiedCount) {
    if (copiedCount) *copiedCount = 0;

    if (!srcSpec || !dstSpec || srcSpec[0] == '\0' || dstSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char srcDir[TRS80_PATH_MAX];
    char srcPat[TRS80_PATH_MAX];
    SplitSrcSpec(srcSpec, srcDir, sizeof(srcDir), srcPat, sizeof(srcPat));

    if (srcPat[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    bool wildcard = HasWildcard(srcPat);
    bool dstHasWildcard = HasWildcard(dstSpec);

    FILINFO dstInfo;
    FRESULT dstStatFr = trans(f_stat(dstSpec, &dstInfo));
    bool dstIsDir = EndsWithSep(dstSpec) ||
                    (dstStatFr == FR_OK && (dstInfo.fattrib & AM_DIR));

    // Wildcard source requires either a destination directory or a destination wildcard
    if (wildcard && !dstIsDir && !dstHasWildcard) {
        return TRS_ILLEGAL_FILENAME;
    }

    if (!wildcard) {
        char fullSrc[TRS80_PATH_MAX];
        char fullDst[TRS80_PATH_MAX];

        if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, srcPat)) {
            return TRS_ILLEGAL_FILENAME;
        }

        if (dstIsDir) {
            if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, srcPat)) {
                return TRS_ILLEGAL_FILENAME;
            }
        } else if (dstHasWildcard) {
            char mappedName[TRS80_PATH_MAX];
            if (!ApplyDestWildcard(dstSpec, srcPat, mappedName, sizeof(mappedName))) {
                return TRS_ILLEGAL_FILENAME;
            }
            snprintf(fullDst, sizeof(fullDst), "%s", mappedName);
        } else {
            snprintf(fullDst, sizeof(fullDst), "%s", dstSpec);
        }

        BYTE trsErr = CopySingleFile(fullSrc, fullDst);
        if (trsErr == FR_OK && copiedCount) {
            *copiedCount = 1;
        }
        return trsErr;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    FILINFO *fno = (FILINFO *)malloc(sizeof(FILINFO));
    if (!dir || !fno) {
        free(dir);
        free(fno);
        return TRS_DEVICE_NOT_AVAIL;
    }

    FRESULT fr = f_opendir(dir, srcDir);
    if (fr != FR_OK) {
        free(dir);
        free(fno);
        return trans(fr);
    }

    char dstDir[TRS80_PATH_MAX];
    char dstPat[TRS80_PATH_MAX];
    if (dstHasWildcard) {
        SplitSrcSpec(dstSpec, dstDir, sizeof(dstDir), dstPat, sizeof(dstPat));
    }

    while (1) {
        fr = f_readdir(dir, fno);
        if (fr != FR_OK) {
            break;
        }
        if (fno->fname[0] == '\0') {
            fr = FR_OK;
            break;
        }

        if (ShouldSkipEntry(fno,false)) {
            continue;
        }

        if (MatchWildcard(srcPat, fno->fname)) {
            char fullSrc[TRS80_PATH_MAX];
            char fullDst[TRS80_PATH_MAX];

            if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, fno->fname)) {
                fr = TRS_ILLEGAL_FILENAME;
                break;
            }

            if (dstHasWildcard) {
                char mappedName[TRS80_PATH_MAX];
                if (!ApplyDestWildcard(dstPat, fno->fname, mappedName, sizeof(mappedName)) ||
                    !JoinPath(fullDst, sizeof(fullDst), dstDir, mappedName)) {
                    fr = TRS_ILLEGAL_FILENAME;
                    break;
                }
            } else {
                if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, fno->fname)) {
                    fr = TRS_ILLEGAL_FILENAME;
                    break;
                }
            }

            BYTE copyErr = CopySingleFile(fullSrc, fullDst);
            if (copyErr != FR_OK) {
                fr = copyErr;
                break;
            }

            if (copiedCount) (*copiedCount)++;
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return fr;
}

// ========================================================
// DELETE FILE(s)
// ========================================================

/**
 * Deletes one or more files matching delSpec.
 * Updates *deletedCount and returns FR_OK on success (including 0 wildcard matches),
 * or a BYTE error code on failure.
 */
BYTE DeleteFileSpec(const char *delSpec, uint32_t *deletedCount) {
    if (deletedCount) *deletedCount = 0;

    if (!delSpec || delSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char dirPart[TRS80_PATH_MAX];
    char patPart[TRS80_PATH_MAX];
    SplitSrcSpec(delSpec, dirPart, sizeof(dirPart), patPart, sizeof(patPart));

    if (patPart[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    if (!HasWildcard(patPart)) {
        char fullPath[TRS80_PATH_MAX];
        if (!JoinPath(fullPath, sizeof(fullPath), dirPart, patPart)) {
            return TRS_ILLEGAL_FILENAME;
        }

        FILINFO info;
        FRESULT statFr = f_stat(fullPath, &info);
        if (statFr != FR_OK) {
            return trans(statFr);
        }
        if (info.fattrib & AM_RDO) {
            return TRS_ACCESS_DENIED;
        }

        BYTE delErr = FileDelete(fullPath);
        if (delErr == TRS_OK) {
            if (deletedCount) (*deletedCount)++;
            return TRS_OK;
        }
        return delErr;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    FILINFO *fno = (FILINFO *)malloc(sizeof(FILINFO));
    if (!dir || !fno) {
        free(dir);
        free(fno);
        return TRS_DEVICE_NOT_AVAIL;
    }

    BYTE fr = f_opendir(dir, dirPart);
    if (fr != TRS_OK) {
        free(dir);
        free(fno);
        return trans(fr);
    }

    BYTE trsResult = TRS_OK;

    while (1) {
        fr = f_readdir(dir, fno);
        if (fr != FR_OK) {
            trsResult = trans(fr);
            break;
        }
        if (fno->fname[0] == '\0') {
            trsResult = TRS_OK;
            break;
        }
        if (ShouldSkipEntry(fno,false) || (fno->fattrib & AM_RDO)) {
            continue;
        }

        if (MatchWildcard(patPart, fno->fname)) {
            char fullPath[TRS80_PATH_MAX];
            if (!JoinPath(fullPath, sizeof(fullPath), dirPart, fno->fname)) {
                trsResult = TRS_ILLEGAL_FILENAME;
                break;
            }

            BYTE delErr = FileDelete(fullPath);
            if (delErr != TRS_OK) {
                trsResult = delErr;
                break;
            }

            if (deletedCount) (*deletedCount)++;
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return trsResult;
}

// ========================================================
// RENAME FILE(s)
// ========================================================

BYTE RenameFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *renamedCount) {
    if (renamedCount) *renamedCount = 0;
    if (!srcSpec || !dstSpec || srcSpec[0] == '\0' || dstSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char srcDir[TRS80_PATH_MAX];
    char srcPat[TRS80_PATH_MAX];
    SplitSrcSpec(srcSpec, srcDir, sizeof(srcDir), srcPat, sizeof(srcPat));

    if (srcPat[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    bool srcWild = HasWildcard(srcPat);
    bool dstWild = HasWildcard(dstSpec);

    // Non-wildcard rename / move (supports files, directories, and cross-dir moves)
    if (!srcWild && !dstWild) {
        char fullSrc[196];
        char fullDst[196];

        if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, srcPat)) {
            return TRS_ILLEGAL_FILENAME;
        }

        FILINFO dstInfo;
        FRESULT dstStat = f_stat(dstSpec, &dstInfo);
        if (EndsWithSep(dstSpec) || (dstStat == FR_OK && (dstInfo.fattrib & AM_DIR))) {
            if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, srcPat)) {
                return TRS_ILLEGAL_FILENAME;
            }
        } else {
            snprintf(fullDst, sizeof(fullDst), "%s", dstSpec);
        }

        if (strcmp(fullSrc, fullDst) == 0) {
            return TRS_OK;
        }

        BYTE err = FileRename(fullSrc, fullDst);
        if (err == TRS_OK && renamedCount) (*renamedCount)++;
        return err;
    }

    // Wildcard rename requires a wildcard pattern in source
    if (!srcWild) {
        return TRS_ILLEGAL_FILENAME;
    }

    char dstDir[TRS80_PATH_MAX];
    char dstPat[TRS80_PATH_MAX];
    if (dstWild) {
        SplitSrcSpec(dstSpec, dstDir, sizeof(dstDir), dstPat, sizeof(dstPat));
    } else {
        snprintf(dstDir, sizeof(dstDir), "%s", dstSpec);
        snprintf(dstPat, sizeof(dstPat), "%s", "*");
    }

    bool allowDirs = (strchr(srcPat, '.') == NULL);

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    FILINFO *fno = (FILINFO *)malloc(sizeof(FILINFO));
    if (!dir || !fno) {
        free(dir);
        free(fno);
        return TRS_DEVICE_NOT_AVAIL;
    }

    FRESULT fr = f_opendir(dir, srcDir);
    if (fr != FR_OK) {
        free(dir);
        free(fno);
        return trans(fr);
    }

    BYTE trsResult = TRS_OK;

    while (1) {
        fr = f_readdir(dir, fno);
        if (fr != FR_OK) {
            trsResult = trans(fr);
            break;
        }
        if (fno->fname[0] == '\0') {
            trsResult = TRS_OK;
            break;
        }
        if (ShouldSkipEntry(fno, allowDirs)) {
            continue;
        }

        if (MatchWildcard(srcPat, fno->fname)) {
            char fullSrc[TRS80_PATH_MAX];
            char fullDst[TRS80_PATH_MAX];
            char mappedName[TRS80_PATH_MAX];

            if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, fno->fname)) {
                trsResult = TRS_ILLEGAL_FILENAME;
                break;
            }

            if (dstWild) {
                if (!ApplyDestWildcard(dstPat, fno->fname, mappedName, sizeof(mappedName)) ||
                    !JoinPath(fullDst, sizeof(fullDst), dstDir, mappedName)) {
                    trsResult = TRS_ILLEGAL_FILENAME;
                    break;
                }
            } else {
                if (!JoinPath(fullDst, sizeof(fullDst), dstDir, fno->fname)) {
                    trsResult = TRS_ILLEGAL_FILENAME;
                    break;
                }
            }

            BYTE renErr = FileRename(fullSrc, fullDst);
            if (renErr != TRS_OK) {
                trsResult = renErr;
                break;
            }

            if (renamedCount) (*renamedCount)++;
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return trsResult;
}

// =================================================