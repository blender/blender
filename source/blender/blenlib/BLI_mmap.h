/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

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
