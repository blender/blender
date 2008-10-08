/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_VIEW2D_H
#define BIF_VIEW2D_H

/* start of a generic 2d view with should allow drawing grids,
 * panning, zooming, scrolling, .. */

#define V2D_UNIT_SECONDS	0
#define V2D_UNIT_FRAMES		1

#define V2D_GRID_CLAMP		0
#define V2D_GRID_NOCLAMP	1

#define V2D_IS_CLIPPED	12000

#define V2D_HORIZONTAL_LINES	1
#define V2D_VERTICAL_LINES		2
#define V2D_HORIZONTAL_AXIS		4
#define V2D_VERTICAL_AXIS		8

struct View2D;
struct View2DGrid;
struct bContext;

typedef struct View2DGrid View2DGrid;

/* opengl drawing setup */
void BIF_view2d_ortho(const struct bContext *C, struct View2D *v2d);

/* grid drawing */
View2DGrid *BIF_view2d_calc_grid(const struct bContext *C, struct View2D *v2d, int unit, int type, int winx, int winy);
void BIF_view2d_draw_grid(const struct bContext *C, struct View2D *v2d, View2DGrid *grid, int flag);
void BIF_view2d_free_grid(View2DGrid *grid);

/* coordinate conversion */
void BIF_view2d_region_to_view(struct View2D *v2d, short x, short y, float *viewx, float *viewy);
void BIF_view2d_view_to_region(struct View2D *v2d, float x, float y, short *regionx, short *regiony);
void BIF_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, short *regionx, short *region_y);

#endif /* BIF_VIEW2D_H */

