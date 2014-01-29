/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file UI_view2d.h
 *  \ingroup editorui
 */

#ifndef __UI_VIEW2D_H__
#define __UI_VIEW2D_H__

/* ------------------------------------------ */
/* Settings and Defines:                      */

/* ---- General Defines ---- */

/* generic value to use when coordinate lies out of view when converting */
#define V2D_IS_CLIPPED  12000

/* Common View2D view types 
 * NOTE: only define a type here if it completely sets all (+/- a few) of the relevant flags 
 *	    and settings for a View2D region, and that set of settings is used in more
 *	    than one specific place
 */
enum eView2D_CommonViewTypes {
	/* custom view type (region has defined all necessary flags already) */
	V2D_COMMONVIEW_CUSTOM = -1,
	/* standard (only use this when setting up a new view, as a sensible base for most settings) */
	V2D_COMMONVIEW_STANDARD,
	/* listview (i.e. Outliner) */
	V2D_COMMONVIEW_LIST,
	/* stackview (this is basically a list where new items are added at the top) */
	V2D_COMMONVIEW_STACK,
	/* headers (this is basically the same as listview, but no y-panning) */
	V2D_COMMONVIEW_HEADER,
	/* ui region containing panels */
	V2D_COMMONVIEW_PANELS_UI
};

/* ---- Defines for Scroller/Grid Arguments ----- */

/* 'dummy' argument to pass when argument is irrelevant */
#define V2D_ARG_DUMMY       -1

/* Grid units */
enum eView2D_Units {
	/* for drawing time */
	V2D_UNIT_SECONDS = 0,
	V2D_UNIT_FRAMES,
	V2D_UNIT_FRAMESCALE,
	
	/* for drawing values */
	V2D_UNIT_VALUES,
	V2D_UNIT_DEGREES,
	V2D_UNIT_TIME,
};

/* clamping of grid values to whole numbers */
enum eView2D_Clamp {
	V2D_GRID_NOCLAMP = 0,
	V2D_GRID_CLAMP
};

/* flags for grid-lines to draw */
enum eView2D_Gridlines {
	V2D_HORIZONTAL_LINES        = (1 << 0),
	V2D_VERTICAL_LINES          = (1 << 1),
	V2D_HORIZONTAL_AXIS         = (1 << 2),
	V2D_VERTICAL_AXIS           = (1 << 3),
	V2D_HORIZONTAL_FINELINES    = (1 << 4),
	
	V2D_GRIDLINES_MAJOR         = (V2D_VERTICAL_LINES | V2D_VERTICAL_AXIS | V2D_HORIZONTAL_LINES | V2D_HORIZONTAL_AXIS),
	V2D_GRIDLINES_ALL           = (V2D_GRIDLINES_MAJOR | V2D_HORIZONTAL_FINELINES),
};

/* ------ Defines for Scrollers ----- */

/* scroller area */
#define V2D_SCROLL_HEIGHT   (0.85f * U.widget_unit)
#define V2D_SCROLL_WIDTH    (0.85f * U.widget_unit)

/* scroller 'handles' hotspot radius for mouse */
#define V2D_SCROLLER_HANDLE_SIZE    (0.6f * U.widget_unit)

/* ------ Define for UI_view2d_sync ----- */

/* means copy it from another v2d */
#define V2D_LOCK_SET    0
/* means copy it to the other v2ds */
#define V2D_LOCK_COPY   1


/* ------------------------------------------ */
/* Macros:								*/

/* test if mouse in a scrollbar (assume that scroller availability has been tested) */
#define IN_2D_VERT_SCROLL(v2d, co)   (BLI_rcti_isect_pt_v(&v2d->vert, co))
#define IN_2D_HORIZ_SCROLL(v2d, co)  (BLI_rcti_isect_pt_v(&v2d->hor,  co))

/* ------------------------------------------ */
/* Type definitions:                          */

struct View2D;
struct View2DGrid;
struct View2DScrollers;

struct wmKeyConfig;
struct bScreen;
struct Scene;
struct ScrArea;
struct ARegion;
struct bContext;
struct rctf;

typedef struct View2DGrid View2DGrid;
typedef struct View2DScrollers View2DScrollers;

/* ----------------------------------------- */
/* Prototypes:                               */

/* refresh and validation (of view rects) */
void UI_view2d_region_reinit(struct View2D *v2d, short type, int winx, int winy);

void UI_view2d_curRect_validate(struct View2D *v2d);
void UI_view2d_curRect_reset(struct View2D *v2d);
void UI_view2d_sync(struct bScreen *screen, struct ScrArea *sa, struct View2D *v2dcur, int flag);

void UI_view2d_totRect_set(struct View2D *v2d, int width, int height);
void UI_view2d_totRect_set_resize(struct View2D *v2d, int width, int height, int resize);

/* per tab offsets, returns 1 if tab changed */
bool UI_view2d_tab_set(struct View2D *v2d, int tab);

void UI_view2d_zoom_cache_reset(void);

/* view matrix operations */
void UI_view2d_view_ortho(struct View2D *v2d);
void UI_view2d_view_orthoSpecial(struct ARegion *ar, struct View2D *v2d, short xaxis);
void UI_view2d_view_restore(const struct bContext *C);

/* grid drawing */
View2DGrid *UI_view2d_grid_calc(struct Scene *scene, struct View2D *v2d,
                                short xunits, short xclamp, short yunits, short yclamp, int winx, int winy);
void UI_view2d_grid_draw(struct View2D *v2d, View2DGrid *grid, int flag);
void UI_view2d_constant_grid_draw(struct View2D *v2d);
void UI_view2d_multi_grid_draw(struct View2D *v2d, int colorid, float step, int level_size, int totlevels);
void UI_view2d_grid_size(View2DGrid *grid, float *r_dx, float *r_dy);
void UI_view2d_grid_free(View2DGrid *grid);

/* scrollbar drawing */
View2DScrollers *UI_view2d_scrollers_calc(const struct bContext *C, struct View2D *v2d,
                                          short xunits, short xclamp, short yunits, short yclamp);
void UI_view2d_scrollers_draw(const struct bContext *C, struct View2D *v2d, View2DScrollers *scrollers);
void UI_view2d_scrollers_free(View2DScrollers *scrollers);

/* list view tools */
void UI_view2d_listview_cell_to_view(struct View2D *v2d, float columnwidth, float rowheight,
                                     float startx, float starty, int column, int row,
                                     struct rctf *rect);
void UI_view2d_listview_view_to_cell(struct View2D *v2d, float columnwidth, float rowheight,
                                     float startx, float starty, float viewx, float viewy,
                                     int *column, int *row);
void UI_view2d_listview_visible_cells(struct View2D *v2d, float columnwidth, float rowheight,
                                      float startx, float starty, int *column_min, int *column_max,
                                      int *row_min, int *row_max);

/* coordinate conversion */
void UI_view2d_region_to_view(struct View2D *v2d, float x, float y, float *viewx, float *viewy);
void UI_view2d_view_to_region(struct View2D *v2d, float x, float y, int *regionx, int *regiony);
void UI_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, int *regionx, int *region_y);
void UI_view2d_to_region_float(struct View2D *v2d, float x, float y, float *regionx, float *regiony);

/* utilities */
struct View2D *UI_view2d_fromcontext(const struct bContext *C);
struct View2D *UI_view2d_fromcontext_rwin(const struct bContext *C);

void UI_view2d_getscale(struct View2D *v2d, float *x, float *y);
void UI_view2d_getscale_inverse(struct View2D *v2d, float *x, float *y);

void UI_view2d_getcenter(struct View2D *v2d, float *x, float *y);
void UI_view2d_setcenter(struct View2D *v2d, float x, float y);

short UI_view2d_mouse_in_scrollers(const struct bContext *C, struct View2D *v2d, int x, int y);

/* cached text drawing in v2d, to allow pixel-aligned draw as post process */
void UI_view2d_text_cache_add(struct View2D *v2d, float x, float y, const char *str, const char col[4]);
void UI_view2d_text_cache_rectf(struct View2D *v2d, const struct rctf *rect, const char *str, const char col[4]);
void UI_view2d_text_cache_draw(struct ARegion *ar);

/* operators */
void UI_view2d_operatortypes(void);
void UI_view2d_keymap(struct wmKeyConfig *keyconf);

void UI_view2d_smooth_view(struct bContext *C, struct ARegion *ar,
                           const struct rctf *cur, const int smooth_viewtx);

#endif /* __UI_VIEW2D_H__ */

