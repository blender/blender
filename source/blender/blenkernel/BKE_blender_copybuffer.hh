/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.hh"

namespace blender {

struct Main;
struct ReportList;
struct bContext;

/** Paste-buffer helper API. For copy, use directly the #PartialWriteContext API. */

/**
 * Import data-blocks from the given .blend filepath (i.e. append them) into the given Main
 * data-base.
 *
 * Unlike #BKE_copybuffer_paste, it does not perform any instantiation of collections/objects/etc.,
 * and does not clear the potential #ID_FLAG_CLIPBOARD_MARK flag from read IDs.
 *
 * Typically used for complex copy/pasting, using a temporary Main instead of the global one.
 *
 * \param libname: Full path to the .blend file used as copy/paste buffer.
 * \param id_types_mask: Only directly link IDs of those types from the given .blend file buffer.
 *
 * \return true on success, false otherwise.
 */
bool BKE_copybuffer_read(Main *bmain_dst,
                         const char *libname,
                         ReportList *reports,
                         uint64_t id_types_mask);

/**
 * Paste data-blocks from the given .blend filepath 'buffer' (i.e. append them) into the context's
 * bmain.
 *
 * Similar to #BKE_copybuffer_read, but also handles instantiation of collections/objects/etc., and
 * does clear the #ID_FLAG_CLIPBOARD_MARK from pasted data.
 *
 * \param libname: Full path to the .blend file used as copy/paste buffer.
 * \param flag: A combination of #eBLOLibLinkFlags and ##eFileSel_Params_Flag to control
 * link/append behavior.
 * \note Ignores #FILE_LINK flag, since it always appends IDs.
 * \param id_types_mask: Only directly link IDs of those types from the given .blend file buffer.
 *
 * \return Number of IDs directly pasted from the buffer (does not includes indirectly linked
 * ones).
 */
int BKE_copybuffer_paste(
    bContext *C, const char *libname, int flag, ReportList *reports, uint64_t id_types_mask);

}  // namespace blender
