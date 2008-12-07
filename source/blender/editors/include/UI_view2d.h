/**
 * $Id$
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
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 *
 * Generic 2d view with should allow drawing grids,
 * panning, zooming, scrolling, .. 
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef UI_VIEW2D_H
#define UI_VIEW2D_H

/* ------------------------------------------ */
/* Settings: 								*/

/* generic value to use when coordinate lies out of view when converting */
#define V2D_IS_CLIPPED	12000

/* 'dummy' argument to pass when argument is irrelevant */
#define V2D_ARG_DUMMY		-1

/* grid-units (for drawing time) */
#define V2D_UNIT_SECONDS	0
#define V2D_UNIT_FRAMES		1

/* grid-units (for drawing values) */
#define V2D_UNIT_VALUES		2
#define V2D_UNIT_DEGREES	3
#define V2D_UNIT_TIME		4
#define V2D_UNIT_SECONDSSEQ	5

/* clamping of grid values to whole numbers */
#define V2D_GRID_NOCLAMP	0
#define V2D_GRID_CLAMP		1


/* flags for grid-lines to draw */
#define V2D_HORIZONTAL_LINES	(1<<0)
#define V2D_VERTICAL_LINES		(1<<1)
#define V2D_HORIZONTAL_AXIS		(1<<2)
#define V2D_VERTICAL_AXIS		(1<<3)


/* ------------------------------------------ */
/* Macros:								*/

/* test if mouse in a scrollbar */
#define IN_2D_VERT_SCROLL(v2d, co) (BLI_in_rcti(&v2d->vert, co[0], co[1]))
#define IN_2D_HORIZ_SCROLL(v2d, co) (BLI_in_rcti(&v2d->hor, co[0], co[1]))

/* ------------------------------------------ */
/* Type definitions: 						*/

struct View2D;
struct View2DGrid;
struct View2DScrollers;

struct wmWindowManager;
struct bContext;

typedef struct View2DGrid View2DGrid;
typedef struct View2DScrollers View2DScrollers;

/* ----------------------------------------- */
/* Prototypes:						    */

/* refresh and validation (of view rects) */
void UI_view2d_size_update(struct View2D *v2d, int winx, int winy);
void UI_view2d_status_enforce(struct View2D *v2d);

void UI_view2d_totRect_set(struct View2D *v2d, int width, int height);
void UI_view2d_curRect_reset(struct View2D *v2d);

/* view matrix operations */
void UI_view2d_view_ortho(const struct bContext *C, struct View2D *v2d);
void UI_view2d_view_orthoSpecial(const struct bContext *C, struct View2D *v2d, short xaxis);
void UI_view2d_view_restore(const struct bContext *C);

/* grid drawing */
View2DGrid *UI_view2d_grid_calc(const struct bContext *C, struct View2D *v2d, short unit, short clamp, int winx, int winy);
void UI_view2d_grid_draw(const struct bContext *C, struct View2D *v2d, View2DGrid *grid, int flag);
void UI_view2d_grid_free(View2DGrid *grid);

/* scrollbar drawing */
View2DScrollers *UI_view2d_scrollers_calc(const struct bContext *C, struct View2D *v2d, short xunits, short xclamp, short yunits, short yclamp);
void UI_view2d_scrollers_draw(const struct bContext *C, struct View2D *v2d, View2DScrollers *scrollers);
void UI_view2d_scrollers_free(View2DScrollers *scrollers);

/* coordinate conversion */
void UI_view2d_region_to_view(struct View2D *v2d, int x, int y, float *viewx, float *viewy);
void UI_view2d_view_to_region(struct View2D *v2d, float x, float y, short *regionx, short *regiony);
void UI_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, short *regionx, short *region_y);

/* utilities */
struct View2D *UI_view2d_fromcontext(const struct bContext *C);
void UI_view2d_getscale(struct View2D *v2d, float *x, float *y);


/* operators */
void ui_view2d_operatortypes(void);
void UI_view2d_keymap(struct wmWindowManager *wm);

#endif /* UI_VIEW2D_H */

