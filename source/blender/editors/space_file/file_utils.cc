/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BKE_blendfile.h"
#include "BKE_context.h"

#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "WM_types.hh"

#include "file_intern.hh"

void file_tile_boundbox(const ARegion *region, FileLayout *layout, const int file, rcti *r_bounds)
{
  int xmin, ymax;

  ED_fileselect_layout_tilepos(layout, file, &xmin, &ymax);
  ymax = int(region->v2d.tot.ymax) - ymax; /* real, view space ymax */
  BLI_rcti_init(r_bounds,
                xmin,
                xmin + layout->tile_w + layout->tile_border_x,
                ymax - layout->tile_h - layout->tile_border_y,
                ymax);
}

void file_path_to_ui_path(const char *path, char *r_path, int max_size)
{
  char tmp_path[PATH_MAX];
  STRNCPY(tmp_path, path);
  BLI_path_slash_rstrip(tmp_path);
  BLI_strncpy(r_path, BKE_blendfile_extension_check(tmp_path) ? tmp_path : path, max_size);
}
