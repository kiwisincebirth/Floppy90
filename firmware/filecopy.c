//
// Support for The TRS-80 COPY command.
// Created by Mark Pruden on 19/11/2026.
//

#include "filecopy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ff.h"

#define COPY_CHUNK_SIZE 4096

/**
 * Copies a single file from srcPath to dstPath.
 * Returns FR_OK on success, or a FatFs FRESULT error code.
 */
FRESULT CopySingleFile(const char *srcPath, const char *dstPath) {
    if (!srcPath || !dstPath || srcPath[0] == '\0' || dstPath[0] == '\0') {
        return FR_INVALID_NAME;
    }

    // Prevent copying a file directly onto itself if paths match identically
    if (strcmp(srcPath, dstPath) == 0) {
        return FR_INVALID_PARAMETER;
    }

    FIL *fsrc = (FIL *)malloc(sizeof(FIL));
    FIL *fdst = (FIL *)malloc(sizeof(FIL));
    uint8_t *buf = (uint8_t *)malloc(COPY_CHUNK_SIZE);

    if (!fsrc || !fdst || !buf) {
        free(fsrc);
        free(fdst);
        free(buf);
        return FR_INT_ERR;
    }

    FRESULT fr = f_open(fsrc, srcPath, FA_READ);
    if (fr != FR_OK) {
        free(fsrc);
        free(fdst);
        free(buf);
        return fr;
    }

    // Non-interactive: create or overwrite destination file
    fr = f_open(fdst, dstPath, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        f_close(fsrc);
        free(fsrc);
        free(fdst);
        free(buf);
        return fr;
    }

    UINT br = 0, bw = 0;
    while (1) {
        fr = f_read(fsrc, buf, COPY_CHUNK_SIZE, &br);
        if (fr != FR_OK) {
            break;
        }
        if (br == 0) {
            // Reached EOF cleanly
            fr = FR_OK;
            break;
        }

        fr = f_write(fdst, buf, br, &bw);
        if (fr != FR_OK || bw < br) {
            if (fr == FR_OK) {
                fr = FR_DISK_ERR;
            }
            break;
        }
    }

    FRESULT frCloseDst = f_close(fdst);
    FRESULT frCloseSrc = f_close(fsrc);

    free(fsrc);
    free(fdst);
    free(buf);

    if (fr != FR_OK) return fr;
    if (frCloseDst != FR_OK) return frCloseDst;
    if (frCloseSrc != FR_OK) return frCloseSrc;

    return FR_OK;
}

// ---------------------------------------------
// ---------------------------------------------
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

/**
 * Maps a matched filename using a destination wildcard pattern (e.g. "*.BAK").
 * Uses the text before '*' as prefix and after '*' as suffix.
 */
static bool ApplyDestWildcard(const char *dstPattern, const char *srcName, char *outName, size_t outSize) {
    const char *star = strchr(dstPattern, '*');
    if (!star) {
        return false;
    }

    size_t prefixLen = (size_t)(star - dstPattern);
    const char *suffix = star + 1;

    // If srcName starts with prefix, strip prefix before applying suffix
    const char *core = srcName;
    if (prefixLen > 0 && strncasecmp(srcName, dstPattern, prefixLen) == 0) {
        core = srcName + prefixLen;
    }

    // If suffix starts with '.', strip existing extension from core when appending suffix
    char base[64];
    snprintf(base, sizeof(base), "%s", core);
    if (suffix[0] == '.') {
        char *dot = strrchr(base, '.');
        if (dot) {
            *dot = '\0';
        }
    }

    char prefixBuf[32];
    if (prefixLen >= sizeof(prefixBuf)) prefixLen = sizeof(prefixBuf) - 1;
    memcpy(prefixBuf, dstPattern, prefixLen);
    prefixBuf[prefixLen] = '\0';

    int written = snprintf(outName, outSize, "%s%s%s", prefixBuf, base, suffix);
    return (written > 0 && (size_t)written < outSize);
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

/**
 * Copies one or more files matching srcSpec to dstSpec.
 * Returns FR_OK on full completion (including 0 matches), or the error code on failure.
 */
FRESULT CopyFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *copiedCount) {
    if (copiedCount) {
        *copiedCount = 0;
    }
    if (!srcSpec || !dstSpec || srcSpec[0] == '\0' || dstSpec[0] == '\0') {
        return FR_INVALID_NAME;
    }

    char srcDir[128];
    char srcPat[64];
    SplitSrcSpec(srcSpec, srcDir, sizeof(srcDir), srcPat, sizeof(srcPat));

    if (srcPat[0] == '\0') {
        return FR_INVALID_NAME;
    }

    bool wildcard = HasWildcard(srcPat);
    bool dstHasWildcard = HasWildcard(dstSpec);

    FILINFO dstInfo;
    FRESULT dstStatFr = f_stat(dstSpec, &dstInfo);
    bool dstIsDir = EndsWithSep(dstSpec) ||
                    (dstStatFr == FR_OK && (dstInfo.fattrib & AM_DIR));

    // Wildcard source requires either a destination directory or a destination wildcard
    if (wildcard && !dstIsDir && !dstHasWildcard) {
        return FR_INVALID_PARAMETER;
    }

    if (!wildcard) {
        char fullSrc[196];
        char fullDst[196];

        if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, srcPat)) {
            return FR_INVALID_NAME;
        }

        if (dstIsDir) {
            if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, srcPat)) {
                return FR_INVALID_NAME;
            }
        } else if (dstHasWildcard) {
            char mappedName[64];
            if (!ApplyDestWildcard(dstSpec, srcPat, mappedName, sizeof(mappedName))) {
                return FR_INVALID_NAME;
            }
            snprintf(fullDst, sizeof(fullDst), "%s", mappedName);
        } else {
            snprintf(fullDst, sizeof(fullDst), "%s", dstSpec);
        }

        FRESULT fr = CopySingleFile(fullSrc, fullDst);
        if (fr == FR_OK && copiedCount) {
            *copiedCount = 1;
        }
        return fr;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    FILINFO *fno = (FILINFO *)malloc(sizeof(FILINFO));
    if (!dir || !fno) {
        free(dir);
        free(fno);
        return FR_INT_ERR;
    }

    FRESULT fr = f_opendir(dir, srcDir);
    if (fr != FR_OK) {
        free(dir);
        free(fno);
        return fr;
    }

    char dstDir[128];
    char dstPat[64];
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
        // Skip subdirectories, dot entries, hidden/system files, and macOS AppleDouble files
        if (fno->fattrib & AM_DIR) {
            continue;
        }
        if (strcmp(fno->fname, ".") == 0 || strcmp(fno->fname, "..") == 0) {
            continue;
        }
        if (fno->fattrib & (AM_HID | AM_SYS)) {
            continue;
        }
        if (strncmp(fno->fname, "._", 2) == 0) {
            continue;
        }

        if (MatchWildcard(srcPat, fno->fname)) {
            char fullSrc[196];
            char fullDst[196];

            if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, fno->fname)) {
                fr = FR_INVALID_NAME;
                break;
            }

            if (dstHasWildcard) {
                char mappedName[64];
                if (!ApplyDestWildcard(dstPat, fno->fname, mappedName, sizeof(mappedName)) ||
                    !JoinPath(fullDst, sizeof(fullDst), dstDir, mappedName)) {
                    fr = FR_INVALID_NAME;
                    break;
                }
            } else {
                if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, fno->fname)) {
                    fr = FR_INVALID_NAME;
                    break;
                }
            }

            FRESULT copyFr = CopySingleFile(fullSrc, fullDst);
            if (copyFr != FR_OK) {
                fr = copyFr;
                break;
            }

            if (copiedCount) {
                (*copiedCount)++;
            }
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return fr;
}

