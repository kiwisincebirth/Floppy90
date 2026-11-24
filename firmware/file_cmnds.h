
#ifndef FLOPPY90_FILE_COPY_H
#define FLOPPY90_FILE_COPY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ff.h"

// Copy Buffer Size
#define COPY_CHUNK_SIZE 4096

// Maximums for String names and paths
#define TRS80_NAME_MAX 64
#define TRS80_PATH_MAX 128

typedef struct {
    char sanitized[TRS80_NAME_MAX];     /* Uppercased, legacy-converted name/pattern (no :N) */
    char primary[TRS80_PATH_MAX];       /* Mapped /:N/ + sanitized, or sanitized if :0/unmapped */
    char secondary[TRS80_PATH_MAX];     /* "/USR/" + sanitized when eligible */
    uint8_t driveId;                    /* Validated 0..9 (0 if absent or out of range) */
    bool hasExplicitSlash;              /* True if raw input had '/' before legacy conversion */
    bool hasMappedDir;                  /* True if driveId 1..9 had a non-empty configured path */
    bool hasSecondary;                  /* True if secondary (/USR/...) is populated & eligible */
    bool truncated;                     /* True if any output buffer hit its capacity limit */
} Trs80PathResult;

/* Load Configuration */
void trs80_load_assign_cfg(void);

/* Save Configuration */
bool trs80_save_assign_cfg(void);

/* Returns mapped path string for driveId 1..9, or NULL if unmapped / out of range */
const char *trs80_get_mapped_dir(uint8_t driveId);

/* Optional setter for config loader; enforces index 0 == NULL and range 1..9 */
void trs80_set_mapped_dir(uint8_t driveId, const char *dirPath);

/* Single entry point for callers: parses token, resolves :1..:9, builds /USR/ fallback */
void trs80_resolve_path(const char *src, int maxLen, Trs80PathResult *out);

// =============================

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