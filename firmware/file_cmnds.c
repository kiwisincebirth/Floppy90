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

// Copy Buffer Size
#define COPY_CHUNK_SIZE 4096

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

// ===============================================================

/**
 * Copies one or more files matching srcSpec to dstSpec.
 * Returns FR_OK on full completion (including 0 matches), or the error code on failure.
 */
FRESULT CopyFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *copiedCount) {
    if (copiedCount) {
        *copiedCount = 0;
    }
    if (!srcSpec || !dstSpec || srcSpec[0] == '\0' || dstSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char srcDir[128];
    char srcPat[64];
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
        char fullSrc[196];
        char fullDst[196];

        if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, srcPat)) {
            return TRS_ILLEGAL_FILENAME;
        }

        if (dstIsDir) {
            if (!JoinPath(fullDst, sizeof(fullDst), dstSpec, srcPat)) {
                return TRS_ILLEGAL_FILENAME;
            }
        } else if (dstHasWildcard) {
            char mappedName[64];
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

        if (ShouldSkipEntry(fno,false)) {
            continue;
        }

        if (MatchWildcard(srcPat, fno->fname)) {
            char fullSrc[196];
            char fullDst[196];

            if (!JoinPath(fullSrc, sizeof(fullSrc), srcDir, fno->fname)) {
                fr = TRS_ILLEGAL_FILENAME;
                break;
            }

            if (dstHasWildcard) {
                char mappedName[64];
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

// ===============================================================

/**
 * Deletes one or more files matching delSpec.
 * Updates *deletedCount and returns FR_OK on success (including 0 wildcard matches),
 * or a FatFs FRESULT error code on failure.
 */
FRESULT DeleteFileSpec(const char *delSpec, uint32_t *deletedCount) {
if (deletedCount) {
        *deletedCount = 0;
    }
    if (!delSpec || delSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char dirPart[128];
    char patPart[64];
    SplitSrcSpec(delSpec, dirPart, sizeof(dirPart), patPart, sizeof(patPart));

    if (patPart[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    if (!HasWildcard(patPart)) {
        char fullPath[196];
        if (!JoinPath(fullPath, sizeof(fullPath), dirPart, patPart)) {
            return TRS_ILLEGAL_FILENAME;
        }

        FILINFO info;
        FRESULT statFr = f_stat(fullPath, &info);
        if (statFr != TRS_OK) {
            return trans(statFr);
        }
        // if (info.fattrib & AM_DIR) {
        //     return TRS_ACCESS_DENIED;
        // }
        if (info.fattrib & AM_RDO) {
            return TRS_OK;
        }

        BYTE delErr = FileDelete(fullPath);
        if (delErr == TRS_OK) {
            if (deletedCount) {
                (*deletedCount)++;
            }
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

    FRESULT fr = f_opendir(dir, dirPart);
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
        if (ShouldSkipEntry(fno,false) || (fno->fattrib & AM_RDO)) {
            continue;
        }

        if (MatchWildcard(patPart, fno->fname)) {
            char fullPath[196];
            if (!JoinPath(fullPath, sizeof(fullPath), dirPart, fno->fname)) {
                trsResult = TRS_ILLEGAL_FILENAME;
                break;
            }

            BYTE delErr = FileDelete(fullPath);
            if (delErr != TRS_OK) {
                trsResult = delErr;
                break;
            }

            if (deletedCount) {
                (*deletedCount)++;
            }
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return trsResult;
}

BYTE RenameFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *renamedCount) {
    if (renamedCount) {
        *renamedCount = 0;
    }
    if (!srcSpec || !dstSpec || srcSpec[0] == '\0' || dstSpec[0] == '\0') {
        return TRS_ILLEGAL_FILENAME;
    }

    char srcDir[128];
    char srcPat[64];
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
        if (err == TRS_OK && renamedCount) {
            (*renamedCount)++;
        }
        return err;
    }

    // Wildcard rename requires a wildcard pattern in source
    if (!srcWild) {
        return TRS_ILLEGAL_FILENAME;
    }

    char dstDir[128];
    char dstPat[64];
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
            char fullSrc[196];
            char fullDst[196];
            char mappedName[64];

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

            if (renamedCount) {
                (*renamedCount)++;
            }
        }
    }

    f_closedir(dir);
    free(dir);
    free(fno);

    return trsResult;
}
