/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Memory-mapped file IO that implements all the OS-specific details and error handling. */

struct BLI_mmap_file;

typedef struct BLI_mmap_file BLI_mmap_file;

/* Prepares an opened file for memory-mapped IO.
 * May return NULL if the operation fails.
 * Note that this seeks to the end of the file to determine its length. */
BLI_mmap_file *BLI_mmap_open(int fd) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;

/* Reads length bytes from file at the given offset into dest.
 * Returns whether the operation was successful (may fail when reading beyond the file
 * end or when IO errors occur). */
bool BLI_mmap_read(BLI_mmap_file *file, void *dest, size_t offset, size_t length)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

void *BLI_mmap_get_pointer(BLI_mmap_file *file) ATTR_WARN_UNUSED_RESULT;

void BLI_mmap_free(BLI_mmap_file *file) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
