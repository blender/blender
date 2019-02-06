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

/** \file \ingroup spfile
 */

#include "BLI_rect.h"
#include "BLI_listbase.h"

#include "BLO_readfile.h"

#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_fileselect.h"

#include "WM_types.h"

#include "file_intern.h"


void file_tile_boundbox(const ARegion *ar, FileLayout *layout, const int file, rcti *r_bounds)
{
	int xmin, ymax;

	ED_fileselect_layout_tilepos(layout, file, &xmin, &ymax);
	ymax = (int)ar->v2d.tot.ymax - ymax; /* real, view space ymax */
	BLI_rcti_init(r_bounds, xmin, xmin + layout->tile_w + layout->tile_border_x,
	              ymax - layout->tile_h - layout->tile_border_y, ymax);
}
