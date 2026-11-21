
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

/**
 * Deletes one or more files matching delSpec.
 * Updates *deletedCount and returns FR_OK on success (including 0 wildcard matches),
 * or a FatFs FRESULT error code on failure.
 */
FRESULT DeleteFileSpec(const char *delSpec, uint32_t *deletedCount);

/**
 * Renames or moves a single file/directory or a wildcard set of matching files.
 *
 * Supports:
 * - Single-target rename or move (files or directories) via FatFs f_rename(),
 *   including moving across directories on the single mounted volume.
 * - Wildcard batch renames (e.g., "*.BAK *.OLD", "MYDB.* MYWORK.*"), restricted
 *   to files only (subdirectories, hidden/system files, and macOS "._*" files
 *   are skipped during wildcard scans).
 *
 * @param srcSpec       Source path, filename, or wildcard pattern.
 * @param dstSpec       Destination path, filename, directory, or wildcard pattern.
 * @param renamedCount  Optional counter updated with the number of items renamed.
 * @return              TRS_OK (0) on success, or a TRS-* error code on failure.
 */
BYTE RenameFileSpec(const char *srcSpec, const char *dstSpec, uint32_t *renamedCount);

#endif //FLOPPY90_FILE_COPY_H