
#ifndef FLOPPY90_FILE_COPY_H
#define FLOPPY90_FILE_COPY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ff.h"

/**
 * Copies a single file from srcPath to dstPath.
 * Returns FR_OK on success, or a FatFs FRESULT error code.
 */
FRESULT CopySingleFile(const char *srcPath, const char *dstPath);

/**
 * Copies one or more files matching srcSpec to dstSpec.
 * Supports '*' wildcards, path prefixes, destination directories,
 * and destination wildcard mappings (such as "*.DAT" to "*.BAK").
 * Updates *copiedCount and returns FR_OK on full completion,
 * or the FatFs FRESULT code on failure.
 */
FRESULT CopyFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *copiedCount);

#endif //FLOPPY90_FILE_COPY_H