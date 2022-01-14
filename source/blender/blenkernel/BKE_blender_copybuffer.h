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
 */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;
struct ReportList;
struct bContext;

/* Copy-buffer (wrapper for BKE_blendfile_write_partial). */

/**
 * Initialize a copy operation.
 */
void BKE_copybuffer_copy_begin(struct Main *bmain_src);
/**
 * Mark an ID to be copied. Should only be called after a call to #BKE_copybuffer_copy_begin.
 */
void BKE_copybuffer_copy_tag_ID(struct ID *id);
/**
 * Finalize a copy operation into given .blend file 'buffer'.
 *
 * \param filename: Full path to the .blend file used as copy/paste buffer.
 *
 * \return true on success, false otherwise.
 */
bool BKE_copybuffer_copy_end(struct Main *bmain_src,
                             const char *filename,
                             struct ReportList *reports);
/**
 * Paste data-blocks from the given .blend file 'buffer' (i.e. append them).
 *
 * Unlike #BKE_copybuffer_paste, it does not perform any instantiation of collections/objects/etc.
 *
 * \param libname: Full path to the .blend file used as copy/paste buffer.
 * \param id_types_mask: Only directly link IDs of those types from the given .blend file buffer.
 *
 * \return true on success, false otherwise.
 */
bool BKE_copybuffer_read(struct Main *bmain_dst,
                         const char *libname,
                         struct ReportList *reports,
                         uint64_t id_types_mask);
/**
 * Paste data-blocks from the given .blend file 'buffer'  (i.e. append them).
 *
 * Similar to #BKE_copybuffer_read, but also handles instantiation of collections/objects/etc.
 *
 * \param libname: Full path to the .blend file used as copy/paste buffer.
 * \param flag: A combination of #eBLOLibLinkFlags and ##eFileSel_Params_Flag to control
 * link/append behavior.
 * \note Ignores #FILE_LINK flag, since it always appends IDs.
 * \param id_types_mask: Only directly link IDs of those types from the given .blend file buffer.
 *
 * \return Number of IDs directly pasted from the buffer
 * (does not includes indirectly linked ones).
 */
int BKE_copybuffer_paste(struct bContext *C,
                         const char *libname,
                         int flag,
                         struct ReportList *reports,
                         uint64_t id_types_mask);

#ifdef __cplusplus
}
#endif
