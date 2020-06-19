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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BLO_WRITEFILE_H__
#define __BLO_WRITEFILE_H__

/** \file
 * \ingroup blenloader
 * \brief external writefile function prototypes.
 */

struct BlendThumbnail;
struct Main;
struct MemFile;
struct ReportList;

/**
 * Adjust paths when saving (kept unless #G_FILE_SAVE_COPY is set).
 */
typedef enum eBLO_WritePathRemap {
  /** No path manipulation. */
  BLO_WRITE_PATH_REMAP_NONE = 0,
  /** Remap existing relative paths (default). */
  BLO_WRITE_PATH_REMAP_RELATIVE = 1,
  /** Remap paths making all paths relative to the new location. */
  BLO_WRITE_PATH_REMAP_RELATIVE_ALL = 2,
  /** Make all paths absolute. */
  BLO_WRITE_PATH_REMAP_ABSOLUTE = 3,
} eBLO_WritePathRemap;

/** Similar to #BlendFileReadParams. */
struct BlendFileWriteParams {
  eBLO_WritePathRemap remap_mode;
  /** Save `.blend1`, `.blend2`... etc. */
  uint use_save_versions : 1;
  /** On write, restore paths after editing them (see #BLO_WRITE_PATH_REMAP_RELATIVE). */
  uint use_save_as_copy : 1;
  uint use_userdef : 1;
  const struct BlendThumbnail *thumb;
};

extern bool BLO_write_file(struct Main *mainvar,
                           const char *filepath,
                           const int write_flags,
                           const struct BlendFileWriteParams *params,
                           struct ReportList *reports);

extern bool BLO_write_file_mem(struct Main *mainvar,
                               struct MemFile *compare,
                               struct MemFile *current,
                               int write_flags);

#endif
