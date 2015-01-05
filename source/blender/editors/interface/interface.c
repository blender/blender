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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation 2002-2008, full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface.c
 *  \ingroup edinterface
 */


#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>  /* offsetof() */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_rect.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_unit.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_idprop.h"

#include "BIF_gl.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "UI_interface.h"

#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"

#include "RNA_access.h"

#include "BPY_extern.h"

#include "IMB_colormanagement.h"

#include "interface_intern.h"

/* avoid unneeded calls to ui_but_value_get */
#define UI_BUT_VALUE_UNSET DBL_MAX
#define UI_GET_BUT_VALUE_INIT(_but, _value) if (_value == DBL_MAX) {  (_value) = ui_but_value_get(_but); } (void)0

#define B_NOP -1

/* 
 * a full doc with API notes can be found in bf-blender/trunk/blender/doc/guides/interface_API.txt
 * 
 * uiBlahBlah()		external function
 * ui_blah_blah()	internal function
 */

static void ui_but_free(const bContext *C, uiBut *but);

bool ui_block_is_menu(const uiBlock *block)
{
	return (((block->flag & UI_BLOCK_LOOP) != 0) &&
	        /* non-menu popups use keep-open, so check this is off */
	        ((block->flag & UI_BLOCK_KEEP_OPEN) == 0));
}

bool ui_block_is_pie_menu(const uiBlock *block)
{
	return ((block->flag & UI_BLOCK_RADIAL) != 0);
}

static bool ui_but_is_unit_radians_ex(UnitSettings *unit, const int unit_type)
{
	return (unit->system_rotation == USER_UNIT_ROT_RADIANS && unit_type == PROP_UNIT_ROTATION);
}

static bool ui_but_is_unit_radians(const uiBut *but)
{
	UnitSettings *unit = but->block->unit;
	const int unit_type = UI_but_unit_type_get(but);

	return ui_but_is_unit_radians_ex(unit, unit_type);
}

/* ************* window matrix ************** */

void ui_block_to_window_fl(const ARegion *ar, uiBlock *block, float *x, float *y)
{
	float gx, gy;
	int sx, sy, getsizex, getsizey;

	getsizex = BLI_rcti_size_x(&ar->winrct) + 1;
	getsizey = BLI_rcti_size_y(&ar->winrct) + 1;
	sx = ar->winrct.xmin;
	sy = ar->winrct.ymin;

	gx = *x;
	gy = *y;

	if (block->panel) {
		gx += block->panel->ofsx;
		gy += block->panel->ofsy;
	}

	*x = ((float)sx) + ((float)getsizex) * (0.5f + 0.5f * (gx * block->winmat[0][0] + gy * block->winmat[1][0] + block->winmat[3][0]));
	*y = ((float)sy) + ((float)getsizey) * (0.5f + 0.5f * (gx * block->winmat[0][1] + gy * block->winmat[1][1] + block->winmat[3][1]));
}

void ui_block_to_window(const ARegion *ar, uiBlock *block, int *x, int *y)
{
	float fx, fy;

	fx = *x;
	fy = *y;

	ui_block_to_window_fl(ar, block, &fx, &fy);

	*x = (int)(fx + 0.5f);
	*y = (int)(fy + 0.5f);
}

void ui_block_to_window_rctf(const ARegion *ar, uiBlock *block, rctf *rct_dst, const rctf *rct_src)
{
	*rct_dst = *rct_src;
	ui_block_to_window_fl(ar, block, &rct_dst->xmin, &rct_dst->ymin);
	ui_block_to_window_fl(ar, block, &rct_dst->xmax, &rct_dst->ymax);
}

void ui_window_to_block_fl(const ARegion *ar, uiBlock *block, float *x, float *y)   /* for mouse cursor */
{
	float a, b, c, d, e, f, px, py;
	int sx, sy, getsizex, getsizey;

	getsizex = BLI_rcti_size_x(&ar->winrct) + 1;
	getsizey = BLI_rcti_size_y(&ar->winrct) + 1;
	sx = ar->winrct.xmin;
	sy = ar->winrct.ymin;

	a = 0.5f * ((float)getsizex) * block->winmat[0][0];
	b = 0.5f * ((float)getsizex) * block->winmat[1][0];
	c = 0.5f * ((float)getsizex) * (1.0f + block->winmat[3][0]);

	d = 0.5f * ((float)getsizey) * block->winmat[0][1];
	e = 0.5f * ((float)getsizey) * block->winmat[1][1];
	f = 0.5f * ((float)getsizey) * (1.0f + block->winmat[3][1]);

	px = *x - sx;
	py = *y - sy;

	*y =  (a * (py - f) + d * (c - px)) / (a * e - d * b);
	*x = (px - b * (*y) - c) / a;

	if (block->panel) {
		*x -= block->panel->ofsx;
		*y -= block->panel->ofsy;
	}
}

void ui_window_to_block(const ARegion *ar, uiBlock *block, int *x, int *y)
{
	float fx, fy;

	fx = *x;
	fy = *y;

	ui_window_to_block_fl(ar, block, &fx, &fy);

	*x = (int)(fx + 0.5f);
	*y = (int)(fy + 0.5f);
}

void ui_window_to_region(const ARegion *ar, int *x, int *y)
{
	*x -= ar->winrct.xmin;
	*y -= ar->winrct.ymin;
}

void ui_region_to_window(const ARegion *ar, int *x, int *y)
{
	*x += ar->winrct.xmin;
	*y += ar->winrct.ymin;
}
/* ******************* block calc ************************* */

void ui_block_translate(uiBlock *block, int x, int y)
{
	uiBut *but;

	for (but = block->buttons.first; but; but = but->next) {
		BLI_rctf_translate(&but->rect, x, y);
	}

	BLI_rctf_translate(&block->rect, x, y);
}

static void ui_block_bounds_calc_text(uiBlock *block, float offset)
{
	uiStyle *style = UI_style_get();
	uiBut *bt, *init_col_bt, *col_bt;
	int i = 0, j, x1addval = offset;

	UI_fontstyle_set(&style->widget);

	for (init_col_bt = bt = block->buttons.first; bt; bt = bt->next) {
		if (!ELEM(bt->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
			j = BLF_width(style->widget.uifont_id, bt->drawstr, sizeof(bt->drawstr));

			if (j > i)
				i = j;
		}

		if (bt->next && bt->rect.xmin < bt->next->rect.xmin) {
			/* End of this column, and it’s not the last one. */
			for (col_bt = init_col_bt; col_bt->prev != bt; col_bt = col_bt->next) {
				col_bt->rect.xmin = x1addval;
				col_bt->rect.xmax = x1addval + i + block->bounds;

				ui_but_update(col_bt);  /* clips text again */
			}

			/* And we prepare next column. */
			x1addval += i + block->bounds;
			i = 0;
			init_col_bt = col_bt;
		}
	}

	/* Last column. */
	for (col_bt = init_col_bt; col_bt; col_bt = col_bt->next) {
		col_bt->rect.xmin = x1addval;
		col_bt->rect.xmax = max_ff(x1addval + i + block->bounds, offset + block->minbounds);

		ui_but_update(col_bt);  /* clips text again */
	}
}

void ui_block_bounds_calc(uiBlock *block)
{
	uiBut *bt;
	int xof;
	
	if (BLI_listbase_is_empty(&block->buttons)) {
		if (block->panel) {
			block->rect.xmin = 0.0; block->rect.xmax = block->panel->sizex;
			block->rect.ymin = 0.0; block->rect.ymax = block->panel->sizey;
		}
	}
	else {
	
		BLI_rctf_init_minmax(&block->rect);

		for (bt = block->buttons.first; bt; bt = bt->next) {
			BLI_rctf_union(&block->rect, &bt->rect);
		}
		
		block->rect.xmin -= block->bounds;
		block->rect.ymin -= block->bounds;
		block->rect.xmax += block->bounds;
		block->rect.ymax += block->bounds;
	}

	block->rect.xmax = block->rect.xmin + max_ff(BLI_rctf_size_x(&block->rect), block->minbounds);

	/* hardcoded exception... but that one is annoying with larger safety */ 
	bt = block->buttons.first;
	if (bt && strncmp(bt->str, "ERROR", 5) == 0) xof = 10;
	else xof = 40;

	block->safety.xmin = block->rect.xmin - xof;
	block->safety.ymin = block->rect.ymin - xof;
	block->safety.xmax = block->rect.xmax + xof;
	block->safety.ymax = block->rect.ymax + xof;
}

static void ui_block_bounds_calc_centered(wmWindow *window, uiBlock *block)
{
	int xmax, ymax;
	int startx, starty;
	int width, height;
	
	/* note: this is used for the splash where window bounds event has not been
	 * updated by ghost, get the window bounds from ghost directly */

	xmax = WM_window_pixels_x(window);
	ymax = WM_window_pixels_y(window);

	ui_block_bounds_calc(block);
	
	width  = BLI_rctf_size_x(&block->rect);
	height = BLI_rctf_size_y(&block->rect);
	
	startx = (xmax * 0.5f) - (width * 0.5f);
	starty = (ymax * 0.5f) - (height * 0.5f);
	
	ui_block_translate(block, startx - block->rect.xmin, starty - block->rect.ymin);
	
	/* now recompute bounds and safety */
	ui_block_bounds_calc(block);
	
}

static void ui_block_bounds_calc_centered_pie(uiBlock *block)
{
	const int xy[2] = {
	    block->pie_data.pie_center_spawned[0],
	    block->pie_data.pie_center_spawned[1]
	};

	ui_block_translate(block, xy[0], xy[1]);

	/* now recompute bounds and safety */
	ui_block_bounds_calc(block);
}

static void ui_block_bounds_calc_popup(
        wmWindow *window, uiBlock *block,
        eBlockBoundsCalc bounds_calc, const int xy[2])
{
	int startx, starty, endx, endy, width, height, oldwidth, oldheight;
	int oldbounds, xmax, ymax;
	const int margin = UI_SCREEN_MARGIN;

	oldbounds = block->bounds;

	/* compute mouse position with user defined offset */
	ui_block_bounds_calc(block);
	
	xmax = WM_window_pixels_x(window);
	ymax = WM_window_pixels_y(window);

	oldwidth  = BLI_rctf_size_x(&block->rect);
	oldheight = BLI_rctf_size_y(&block->rect);

	/* first we ensure wide enough text bounds */
	if (bounds_calc == UI_BLOCK_BOUNDS_POPUP_MENU) {
		if (block->flag & UI_BLOCK_LOOP) {
			block->bounds = 2.5f * UI_UNIT_X;
			ui_block_bounds_calc_text(block, block->rect.xmin);
		}
	}

	/* next we recompute bounds */
	block->bounds = oldbounds;
	ui_block_bounds_calc(block);

	/* and we adjust the position to fit within window */
	width  = BLI_rctf_size_x(&block->rect);
	height = BLI_rctf_size_y(&block->rect);

	/* avoid divide by zero below, caused by calling with no UI, but better not crash */
	oldwidth = oldwidth > 0 ? oldwidth : MAX2(1, width);
	oldheight = oldheight > 0 ? oldheight : MAX2(1, height);

	/* offset block based on mouse position, user offset is scaled
	 * along in case we resized the block in ui_block_bounds_calc_text */
	startx = xy[0] + block->rect.xmin + (block->mx * width) / oldwidth;
	starty = xy[1] + block->rect.ymin + (block->my * height) / oldheight;

	if (startx < margin)
		startx = margin;
	if (starty < margin)
		starty = margin;

	endx = startx + width;
	endy = starty + height;

	if (endx > xmax) {
		endx = xmax - margin;
		startx = endx - width;
	}
	if (endy > ymax - margin) {
		endy = ymax - margin;
		starty = endy - height;
	}

	ui_block_translate(block, startx - block->rect.xmin, starty - block->rect.ymin);

	/* now recompute bounds and safety */
	ui_block_bounds_calc(block);
}

/* used for various cases */
void UI_block_bounds_set_normal(uiBlock *block, int addval)
{
	if (block == NULL)
		return;
	
	block->bounds = addval;
	block->bounds_type = UI_BLOCK_BOUNDS;
}

/* used for pulldowns */
void UI_block_bounds_set_text(uiBlock *block, int addval)
{
	block->bounds = addval;
	block->bounds_type = UI_BLOCK_BOUNDS_TEXT;
}

/* used for block popups */
void UI_block_bounds_set_popup(uiBlock *block, int addval, int mx, int my)
{
	block->bounds = addval;
	block->bounds_type = UI_BLOCK_BOUNDS_POPUP_MOUSE;
	block->mx = mx;
	block->my = my;
}

/* used for menu popups */
void UI_block_bounds_set_menu(uiBlock *block, int addval, int mx, int my)
{
	block->bounds = addval;
	block->bounds_type = UI_BLOCK_BOUNDS_POPUP_MENU;
	block->mx = mx;
	block->my = my;
}

/* used for centered popups, i.e. splash */
void UI_block_bounds_set_centered(uiBlock *block, int addval)
{
	block->bounds = addval;
	block->bounds_type = UI_BLOCK_BOUNDS_POPUP_CENTER;
}

void UI_block_bounds_set_explicit(uiBlock *block, int minx, int miny, int maxx, int maxy)
{
	block->rect.xmin = minx;
	block->rect.ymin = miny;
	block->rect.xmax = maxx;
	block->rect.ymax = maxy;
	block->bounds_type = UI_BLOCK_BOUNDS_NONE;
}

static int ui_but_calc_float_precision(uiBut *but, double value)
{
	int prec = (int)but->a2;

	/* first check for various special cases:
	 * * If button is radians, we want additional precision (see T39861).
	 * * If prec is not set, we fallback to a simple default */
	if (ui_but_is_unit_radians(but) && prec < 5) {
		prec = 5;
	}
	else if (prec == -1) {
		prec = (but->hardmax < 10.001f) ? 3 : 2;
	}

	return UI_calc_float_precision(prec, value);
}

/* ************** LINK LINE DRAWING  ************* */

/* link line drawing is not part of buttons or theme.. so we stick with it here */

static void ui_draw_linkline(uiLinkLine *line, int highlightActiveLines, int dashInactiveLines)
{
	rcti rect;

	if (line->from == NULL || line->to == NULL) return;
	
	rect.xmin = BLI_rctf_cent_x(&line->from->rect);
	rect.ymin = BLI_rctf_cent_y(&line->from->rect);
	rect.xmax = BLI_rctf_cent_x(&line->to->rect);
	rect.ymax = BLI_rctf_cent_y(&line->to->rect);
	
	if (dashInactiveLines)
		UI_ThemeColor(TH_GRID);
	else if (line->flag & UI_SELECT)
		glColor3ub(100, 100, 100);
	else if (highlightActiveLines && ((line->from->flag & UI_ACTIVE) || (line->to->flag & UI_ACTIVE)))
		UI_ThemeColor(TH_TEXT_HI);
	else
		glColor3ub(0, 0, 0);

	ui_draw_link_bezier(&rect);
}

static void ui_draw_links(uiBlock *block)
{
	uiBut *but;
	uiLinkLine *line;

	/* Draw the grey out lines. Do this first so they appear at the
	 * bottom of inactive or active lines.
	 * As we go, remember if we see any active or selected lines. */
	bool found_selectline = false;
	bool found_activeline = false;

	for (but = block->buttons.first; but; but = but->next) {
		if (but->type == UI_BTYPE_LINK && but->link) {
			for (line = but->link->lines.first; line; line = line->next) {
				if (!(line->from->flag & UI_ACTIVE) && !(line->to->flag & UI_ACTIVE)) {
					if (line->deactive)
						ui_draw_linkline(line, 0, true);
				}
				else
					found_activeline = true;

				if ((line->from->flag & UI_SELECT) || (line->to->flag & UI_SELECT))
					found_selectline = true;
			}
		}
	}

	/* Draw the inactive lines (lines with neither button being hovered over) */
	for (but = block->buttons.first; but; but = but->next) {
		if (but->type == UI_BTYPE_LINK && but->link) {
			for (line = but->link->lines.first; line; line = line->next) {
				if (!(line->from->flag & UI_ACTIVE) && !(line->to->flag & UI_ACTIVE)) {
					if (!line->deactive)
						ui_draw_linkline(line, 0, false);
				}
			}
		}
	}

	/* Draw any active lines (lines with either button being hovered over).
	 * Do this last so they appear on top of inactive and grey out lines. */
	if (found_activeline) {
		for (but = block->buttons.first; but; but = but->next) {
			if (but->type == UI_BTYPE_LINK && but->link) {
				for (line = but->link->lines.first; line; line = line->next) {
					if ((line->from->flag & UI_ACTIVE) || (line->to->flag & UI_ACTIVE))
						ui_draw_linkline(line, !found_selectline, false);
				}
			}
		}
	}
}

/* ************** BLOCK ENDING FUNCTION ************* */

/* NOTE: if but->poin is allocated memory for every defbut, things fail... */
static bool ui_but_equals_old(const uiBut *but, const uiBut *oldbut)
{
	/* various properties are being compared here, hopefully sufficient
	 * to catch all cases, but it is simple to add more checks later */
	if (but->retval != oldbut->retval) return false;
	if (but->rnapoin.data != oldbut->rnapoin.data) return false;
	if (but->rnaprop != oldbut->rnaprop || but->rnaindex != oldbut->rnaindex) return false;
	if (but->func != oldbut->func) return false;
	if (but->funcN != oldbut->funcN) return false;
	if (oldbut->func_arg1 != oldbut && but->func_arg1 != oldbut->func_arg1) return false;
	if (oldbut->func_arg2 != oldbut && but->func_arg2 != oldbut->func_arg2) return false;
	if (!but->funcN && ((but->poin != oldbut->poin && (uiBut *)oldbut->poin != oldbut) ||
	                    (but->pointype != oldbut->pointype))) return false;
	if (but->optype != oldbut->optype) return false;

	return true;
}

uiBut *ui_but_find_old(uiBlock *block_old, const uiBut *but_new)
{
	uiBut *but_old;
	for (but_old = block_old->buttons.first; but_old; but_old = but_old->next) {
		if (ui_but_equals_old(but_new, but_old)) {
			break;
		}
	}
	return but_old;
}
uiBut *ui_but_find_new(uiBlock *block_new, const uiBut *but_old)
{
	uiBut *but_new;
	for (but_new = block_new->buttons.first; but_new; but_new = but_new->next) {
		if (ui_but_equals_old(but_new, but_old)) {
			break;
		}
	}
	return but_new;
}

/* oldbut is being inserted in new block, so we use the lines from new button, and replace button pointers */
static void ui_but_update_linklines(uiBlock *block, uiBut *oldbut, uiBut *newbut)
{
	uiLinkLine *line;
	uiBut *but;
	
	/* if active button is UI_BTYPE_LINK */
	if (newbut->type == UI_BTYPE_LINK && newbut->link) {
		
		SWAP(uiLink *, oldbut->link, newbut->link);
		
		for (line = oldbut->link->lines.first; line; line = line->next) {
			if (line->to == newbut)
				line->to = oldbut;
			if (line->from == newbut)
				line->from = oldbut;
		}
	}
	
	/* check all other button links */
	for (but = block->buttons.first; but; but = but->next) {
		if (but != newbut && but->type == UI_BTYPE_LINK && but->link) {
			for (line = but->link->lines.first; line; line = line->next) {
				if (line->to == newbut)
					line->to = oldbut;
				if (line->from == newbut)
					line->from = oldbut;
			}
		}
	}
}

/**
 * \return true when \a but_p is set (only done for active buttons).
 */
static bool ui_but_update_from_old_block(const bContext *C, uiBlock *block, uiBut **but_p, uiBut **but_old_p)
{
	const int drawflag_copy = 0;  /* None currently. */

	uiBlock *oldblock = block->oldblock;
	uiBut *oldbut = NULL, *but = *but_p;
	bool found_active = false;


#if 0
	/* simple/stupid - search every time */
	oldbut = ui_but_find_old(oldblock, but);
	(void)but_old_p;
#else
	BLI_assert(*but_old_p == NULL || BLI_findindex(&oldblock->buttons, *but_old_p) != -1);

	/* fastpath - avoid loop-in-loop, calling 'ui_but_find_old'
	 * as long as old/new buttons are aligned */
	if (LIKELY(*but_old_p && ui_but_equals_old(but, *but_old_p))) {
		oldbut = *but_old_p;
	}
	else {
		/* fallback to block search */
		oldbut = ui_but_find_old(oldblock, but);
	}
	(*but_old_p) = oldbut ? oldbut->next : NULL;
#endif


	if (!oldbut) {
		return found_active;
	}

	if (oldbut->active) {
		/* flags from the buttons we want to refresh, may want to add more here... */
		const int flag_copy = UI_BUT_REDALERT;

		found_active = true;

#if 0
		but->flag = oldbut->flag;
		but->active = oldbut->active;
		but->pos = oldbut->pos;
		but->ofs = oldbut->ofs;
		but->editstr = oldbut->editstr;
		but->editval = oldbut->editval;
		but->editvec = oldbut->editvec;
		but->editcoba = oldbut->editcoba;
		but->editcumap = oldbut->editcumap;
		but->selsta = oldbut->selsta;
		but->selend = oldbut->selend;
		but->softmin = oldbut->softmin;
		but->softmax = oldbut->softmax;
		but->linkto[0] = oldbut->linkto[0];
		but->linkto[1] = oldbut->linkto[1];
		oldbut->active = NULL;
#endif

		/* move button over from oldblock to new block */
		BLI_remlink(&oldblock->buttons, oldbut);
		BLI_insertlinkafter(&block->buttons, but, oldbut);
		oldbut->block = block;
		*but_p = oldbut;

		/* still stuff needs to be copied */
		oldbut->rect = but->rect;
		oldbut->context = but->context; /* set by Layout */

		/* drawing */
		oldbut->icon = but->icon;
		oldbut->iconadd = but->iconadd;
		oldbut->alignnr = but->alignnr;

		/* typically the same pointers, but not on undo/redo */
		/* XXX some menu buttons store button itself in but->poin. Ugly */
		if (oldbut->poin != (char *)oldbut) {
			SWAP(char *, oldbut->poin, but->poin);
			SWAP(void *, oldbut->func_argN, but->func_argN);
		}

		oldbut->flag = (oldbut->flag & ~flag_copy) | (but->flag & flag_copy);
		oldbut->drawflag = (oldbut->drawflag & ~drawflag_copy) | (but->drawflag & drawflag_copy);

		/* copy hardmin for list rows to prevent 'sticking' highlight to mouse position
		 * when scrolling without moving mouse (see [#28432]) */
		if (ELEM(oldbut->type, UI_BTYPE_ROW, UI_BTYPE_LISTROW))
			oldbut->hardmax = but->hardmax;

		ui_but_update_linklines(block, oldbut, but);

		if (!BLI_listbase_is_empty(&block->butstore)) {
			UI_butstore_register_update(block, oldbut, but);
		}

		/* move/copy string from the new button to the old */
		/* needed for alt+mouse wheel over enums */
		if (but->str != but->strdata) {
			if (oldbut->str != oldbut->strdata) {
				SWAP(char *, but->str, oldbut->str);
			}
			else {
				oldbut->str = but->str;
				but->str = but->strdata;
			}
		}
		else {
			if (oldbut->str != oldbut->strdata) {
				MEM_freeN(oldbut->str);
				oldbut->str = oldbut->strdata;
			}
			BLI_strncpy(oldbut->strdata, but->strdata, sizeof(oldbut->strdata));
		}

		BLI_remlink(&block->buttons, but);
		ui_but_free(C, but);

		/* note: if layout hasn't been applied yet, it uses old button pointers... */
	}
	else {
		const int flag_copy = UI_BUT_DRAG_MULTI;

		but->flag = (but->flag & ~flag_copy) | (oldbut->flag & flag_copy);

		/* ensures one button can get activated, and in case the buttons
		 * draw are the same this gives O(1) lookup for each button */
		BLI_remlink(&oldblock->buttons, oldbut);
		ui_but_free(C, oldbut);
	}

	return found_active;
}

/* needed for temporarily rename buttons, such as in outliner or file-select,
 * they should keep calling uiDefButs to keep them alive */
/* returns 0 when button removed */
bool UI_but_active_only(const bContext *C, ARegion *ar, uiBlock *block, uiBut *but)
{
	uiBlock *oldblock;
	uiBut *oldbut;
	bool activate = false, found = false, isactive = false;
	
	oldblock = block->oldblock;
	if (!oldblock) {
		activate = true;
	}
	else {
		oldbut = ui_but_find_old(oldblock, but);
		if (oldbut) {
			found = true;

			if (oldbut->active) {
				isactive = true;
			}
		}
	}
	if ((activate == true) || (found == false)) {
		ui_but_activate_event((bContext *)C, ar, but);
	}
	else if ((found == true) && (isactive == false)) {
		BLI_remlink(&block->buttons, but);
		ui_but_free(C, but);
		return false;
	}
	
	return true;
}

/* simulate button click */
void UI_but_execute(const bContext *C, uiBut *but)
{
	ARegion *ar = CTX_wm_region(C);
	void *active_back;
	ui_but_execute_begin((bContext *)C, ar, but, &active_back);
	/* Value is applied in begin. No further action required. */
	ui_but_execute_end((bContext *)C, ar, but, active_back);
}

/* use to check if we need to disable undo, but don't make any changes
 * returns false if undo needs to be disabled. */
static bool ui_but_is_rna_undo(const uiBut *but)
{
	if (but->rnapoin.id.data) {
		/* avoid undo push for buttons who's ID are screen or wm level
		 * we could disable undo for buttons with no ID too but may have
		 * unforeseen consequences, so best check for ID's we _know_ are not
		 * handled by undo - campbell */
		ID *id = but->rnapoin.id.data;
		if (ID_CHECK_UNDO(id) == false) {
			return false;
		}
		else {
			return true;
		}
	}
	else if (but->rnapoin.type && !RNA_struct_undo_check(but->rnapoin.type)) {
		return false;
	}

	return true;
}

/* assigns automatic keybindings to menu items for fast access
 * (underline key in menu) */
static void ui_menu_block_set_keyaccels(uiBlock *block)
{
	uiBut *but;

	unsigned int menu_key_mask = 0;
	unsigned char menu_key;
	const char *str_pt;
	int pass;
	int tot_missing = 0;

	/* only do it before bounding */
	if (block->rect.xmin != block->rect.xmax)
		return;

	for (pass = 0; pass < 2; pass++) {
		/* 2 Passes, on for first letter only, second for any letter if first fails
		 * fun first pass on all buttons so first word chars always get first priority */

		for (but = block->buttons.first; but; but = but->next) {
			if (!ELEM(but->type,
			          UI_BTYPE_BUT,
			          UI_BTYPE_BUT_MENU,
			          UI_BTYPE_MENU, UI_BTYPE_BLOCK,
			          UI_BTYPE_PULLDOWN) ||
			    (but->flag & UI_HIDDEN))
			{
				/* pass */
			}
			else if (but->menu_key == '\0') {
				if (but->str) {
					for (str_pt = but->str; *str_pt; ) {
						menu_key = tolower(*str_pt);
						if ((menu_key >= 'a' && menu_key <= 'z') && !(menu_key_mask & 1 << (menu_key - 'a'))) {
							menu_key_mask |= 1 << (menu_key - 'a');
							break;
						}

						if (pass == 0) {
							/* Skip to next delimiter on first pass (be picky) */
							while (isalpha(*str_pt))
								str_pt++;

							if (*str_pt)
								str_pt++;
						}
						else {
							/* just step over every char second pass and find first usable key */
							str_pt++;
						}
					}

					if (*str_pt) {
						but->menu_key = menu_key;
					}
					else {
						/* run second pass */
						tot_missing++;
					}

					/* if all keys have been used just exit, unlikely */
					if (menu_key_mask == (1 << 26) - 1) {
						return;
					}
				}
			}
		}

		/* check if second pass is needed */
		if (!tot_missing) {
			break;
		}
	}
}

/* XXX, this code will shorten any allocated string to 'UI_MAX_NAME_STR'
 * since this is really long its unlikely to be an issue,
 * but this could be supported */
void ui_but_add_shortcut(uiBut *but, const char *shortcut_str, const bool do_strip)
{

	if (do_strip && (but->flag & UI_BUT_HAS_SEP_CHAR)) {
		char *cpoin = strrchr(but->str, UI_SEP_CHAR);
		if (cpoin) {
			*cpoin = '\0';
		}
		but->flag &= ~UI_BUT_HAS_SEP_CHAR;
	}

	/* without this, just allow stripping of the shortcut */
	if (shortcut_str) {
		char *butstr_orig;

		if (but->str != but->strdata) {
			butstr_orig = but->str; /* free after using as source buffer */
		}
		else {
			butstr_orig = BLI_strdup(but->str);
		}
		BLI_snprintf(but->strdata,
		             sizeof(but->strdata),
		             "%s" UI_SEP_CHAR_S "%s",
		             butstr_orig, shortcut_str);
		MEM_freeN(butstr_orig);
		but->str = but->strdata;
		but->flag |= UI_BUT_HAS_SEP_CHAR;
		ui_but_update(but);
	}
}

static bool ui_but_event_operator_string(const bContext *C, uiBut *but, char *buf, const size_t buf_len)
{
	MenuType *mt;
	bool found = false;

	if (but->optype) {
		IDProperty *prop = (but->opptr) ? but->opptr->data : NULL;

		if (WM_key_event_operator_string(C, but->optype->idname, but->opcontext, prop, true,
		                                 buf, buf_len))
		{
			found = true;
		}
	}
	else if ((mt = UI_but_menutype_get(but))) {
		IDProperty *prop_menu;
		IDProperty *prop_menu_name;

		/* annoying, create a property */
		IDPropertyTemplate val = {0};
		prop_menu = IDP_New(IDP_GROUP, &val, __func__); /* dummy, name is unimportant  */
		IDP_AddToGroup(prop_menu, (prop_menu_name = IDP_NewString("", "name", sizeof(mt->idname))));

		IDP_AssignString(prop_menu_name, mt->idname, sizeof(mt->idname));

		if (WM_key_event_operator_string(C, "WM_OT_call_menu", WM_OP_INVOKE_REGION_WIN, prop_menu, true,
		                                 buf, buf_len))
		{
			found = true;
		}

		IDP_FreeProperty(prop_menu);
		MEM_freeN(prop_menu);
	}

	return found;
}

static bool ui_but_event_property_operator_string(const bContext *C, uiBut *but, char *buf, const size_t buf_len)
{	
	/* context toggle operator names to check... */
	const char *ctx_toggle_opnames[] = {
		"WM_OT_context_toggle",
		"WM_OT_context_toggle_enum",
		"WM_OT_context_cycle_int",
		"WM_OT_context_cycle_enum",
		"WM_OT_context_cycle_array",
		"WM_OT_context_menu_enum",
		NULL
	};		
	const size_t num_ops = sizeof(ctx_toggle_opnames) / sizeof(const char *);
	
	bool found = false;
	
	/* this version is only for finding hotkeys for properties (which get set via context using operators) */
	if (but->rnaprop) {
		/* to avoid massive slowdowns on property panels, for now, we only check the 
		 * hotkeys for Editor / Scene settings...
		 *
		 * TODO: userpref settings?
		 */
		// TODO: value (for enum stuff)?
		char *data_path = NULL;
		
		if (but->rnapoin.id.data) {
			ID *id = but->rnapoin.id.data;
			
			if (GS(id->name) == ID_SCR) {
				/* screen/editor property 
				 * NOTE: in most cases, there is actually no info for backwards tracing 
				 * how to get back to ID from the editor data we may be dealing with
				 */
				if (RNA_struct_is_a(but->rnapoin.type, &RNA_Space)) {
					/* data should be directly on here... */
					data_path = BLI_sprintfN("space_data.%s", RNA_property_identifier(but->rnaprop));
				}
				else {
					/* special exceptions for common nested data in editors... */
					if (RNA_struct_is_a(but->rnapoin.type, &RNA_DopeSheet)) {
						/* dopesheet filtering options... */
						data_path = BLI_sprintfN("space_data.dopesheet.%s", RNA_property_identifier(but->rnaprop));
					}
				}
			}
			else if (GS(id->name) == ID_SCE) {
				if (RNA_struct_is_a(but->rnapoin.type, &RNA_ToolSettings)) {
					/* toolsettings property 
					 * NOTE: toolsettings is usually accessed directly (i.e. not through scene)
					 */
					data_path = RNA_path_from_ID_to_property(&but->rnapoin, but->rnaprop);
				}
				else {
					/* scene property */
					char *path = RNA_path_from_ID_to_property(&but->rnapoin, but->rnaprop);
					
					if (path) {
						data_path = BLI_sprintfN("scene.%s", path);
						MEM_freeN(path);
					}
#if 0
					else {
						printf("ERROR in %s(): Couldn't get path for scene property - %s\n",
						       __func__, RNA_property_identifier(but->rnaprop));
					}
#endif
				}
			}
			else {
				//puts("other id");
			}
			
			//printf("prop shortcut: '%s' (%s)\n", RNA_property_identifier(but->rnaprop), data_path);
		}
		
		/* we have a datapath! */
		if (data_path) {
			size_t i;
			
			/* create a property to host the "datapath" property we're sending to the operators */
			IDProperty *prop_path;
			IDProperty *prop_path_value;
			
			IDPropertyTemplate val = {0};
			prop_path = IDP_New(IDP_GROUP, &val, __func__);
			prop_path_value = IDP_NewString(data_path, "data_path", strlen(data_path) + 1);
			IDP_AddToGroup(prop_path, prop_path_value);
			
			/* check each until one works... */
			for (i = 0; (i < num_ops) && (ctx_toggle_opnames[i]); i++) {
				//printf("\t%s\n", ctx_toggle_opnames[i]);
				if (WM_key_event_operator_string(C, ctx_toggle_opnames[i], WM_OP_INVOKE_REGION_WIN, prop_path, false,
				                                 buf, buf_len))
				{
					found = true;
					break;
				}
			}
			
			/* cleanup */
			IDP_FreeProperty(prop_path);
			MEM_freeN(prop_path);
			MEM_freeN(data_path);
		}
	}
	
	return found;
}

/* this goes in a seemingly weird pattern:
 *
 *     4
 *  5     6
 * 1       2
 *  7     8
 *     3
 *
 * but it's actually quite logical. It's designed to be 'upwards compatible'
 * for muscle memory so that the menu item locations are fixed and don't move
 * as new items are added to the menu later on. It also optimises efficiency -
 * a radial menu is best kept symmetrical, with as large an angle between
 * items as possible, so that the gestural mouse movements can be fast and inexact.

 * It starts off with two opposite sides for the first two items
 * then joined by the one below for the third (this way, even with three items,
 * the menu seems to still be 'in order' reading left to right). Then the fourth is
 * added to complete the compass directions. From here, it's just a matter of
 * subdividing the rest of the angles for the last 4 items.
 *
 * --Matt 07/2006
 */
const char ui_radial_dir_order[8] = {
    UI_RADIAL_W,  UI_RADIAL_E,  UI_RADIAL_S,  UI_RADIAL_N,
    UI_RADIAL_NW, UI_RADIAL_NE, UI_RADIAL_SW, UI_RADIAL_SE};

const char  ui_radial_dir_to_numpad[8] = {8, 9, 6, 3, 2, 1, 4, 7};
const short ui_radial_dir_to_angle[8] =  {90, 45, 0, 315, 270, 225, 180, 135};

static void ui_but_pie_direction_string(uiBut *but, char *buf, int size)
{
	BLI_assert(but->pie_dir < ARRAY_SIZE(ui_radial_dir_to_numpad));
	BLI_snprintf(buf, size, "%d", ui_radial_dir_to_numpad[but->pie_dir]);
}

static void ui_menu_block_set_keymaps(const bContext *C, uiBlock *block)
{
	uiBut *but;
	char buf[128];

	/* only do it before bounding */
	if (block->rect.xmin != block->rect.xmax)
		return;

	if (block->flag & UI_BLOCK_RADIAL) {
		for (but = block->buttons.first; but; but = but->next) {
			if (but->pie_dir != UI_RADIAL_NONE) {
				ui_but_pie_direction_string(but, buf, sizeof(buf));
				ui_but_add_shortcut(but, buf, false);
			}
		}
	}
	else {
		for (but = block->buttons.first; but; but = but->next) {

			if (ui_but_event_operator_string(C, but, buf, sizeof(buf))) {
				ui_but_add_shortcut(but, buf, false);
			}
			else if (ui_but_event_property_operator_string(C, but, buf, sizeof(buf))) {
				ui_but_add_shortcut(but, buf, false);
			}
		}
	}
}

void UI_block_update_from_old(const bContext *C, uiBlock *block)
{
	uiBut *but_old;
	uiBut *but;

	if (!block->oldblock)
		return;

	but_old = block->oldblock->buttons.first;

	if (BLI_listbase_is_empty(&block->oldblock->butstore) == false) {
		UI_butstore_update(block);
	}

	for (but = block->buttons.first; but; but = but->next) {
		if (ui_but_update_from_old_block(C, block, &but, &but_old)) {
			ui_but_update(but);
		}
	}

	block->auto_open = block->oldblock->auto_open;
	block->auto_open_last = block->oldblock->auto_open_last;
	block->tooltipdisabled = block->oldblock->tooltipdisabled;
	BLI_movelisttolist(&block->color_pickers.list, &block->oldblock->color_pickers.list);

	block->oldblock = NULL;
}

void UI_block_end_ex(const bContext *C, uiBlock *block, const int xy[2])
{
	wmWindow *window = CTX_wm_window(C);
	Scene *scene = CTX_data_scene(C);
	uiBut *but;

	BLI_assert(block->active);

	UI_block_update_from_old(C, block);

	/* inherit flags from 'old' buttons that was drawn here previous, based
	 * on matching buttons, we need this to make button event handling non
	 * blocking, while still allowing buttons to be remade each redraw as it
	 * is expected by blender code */
	for (but = block->buttons.first; but; but = but->next) {
		/* temp? Proper check for graying out */
		if (but->optype) {
			wmOperatorType *ot = but->optype;

			if (but->context)
				CTX_store_set((bContext *)C, but->context);

			if (ot == NULL || WM_operator_poll_context((bContext *)C, ot, but->opcontext) == 0) {
				but->flag |= UI_BUT_DISABLED;
				but->lock = true;
			}

			if (but->context)
				CTX_store_set((bContext *)C, NULL);
		}

		ui_but_anim_flag(but, (scene) ? scene->r.cfra : 0.0f);
	}



	/* handle pending stuff */
	if (block->layouts.first) {
		UI_block_layout_resolve(block, NULL, NULL);
	}
	ui_block_align_calc(block);
	if ((block->flag & UI_BLOCK_LOOP) && (block->flag & UI_BLOCK_NUMSELECT)) {
		ui_menu_block_set_keyaccels(block); /* could use a different flag to check */
	}

	if (block->flag & UI_BLOCK_LOOP) {
		ui_menu_block_set_keymaps(C, block);
	}
	
	/* after keymaps! */
	switch (block->bounds_type) {
		case UI_BLOCK_BOUNDS_NONE:
			break;
		case UI_BLOCK_BOUNDS:
			ui_block_bounds_calc(block);
			break;
		case UI_BLOCK_BOUNDS_TEXT:
			ui_block_bounds_calc_text(block, 0.0f);
			break;
		case UI_BLOCK_BOUNDS_POPUP_CENTER:
			ui_block_bounds_calc_centered(window, block);
			break;
		case UI_BLOCK_BOUNDS_PIE_CENTER:
			ui_block_bounds_calc_centered_pie(block);
			break;

			/* fallback */
		case UI_BLOCK_BOUNDS_POPUP_MOUSE:
		case UI_BLOCK_BOUNDS_POPUP_MENU:
			ui_block_bounds_calc_popup(window, block, block->bounds_type, xy);
			break;
	}

	if (block->rect.xmin == 0.0f && block->rect.xmax == 0.0f) {
		UI_block_bounds_set_normal(block, 0);
	}
	if (block->flag & UI_BUT_ALIGN) {
		UI_block_align_end(block);
	}

	block->endblock = 1;
}

void UI_block_end(const bContext *C, uiBlock *block)
{
	wmWindow *window = CTX_wm_window(C);

	UI_block_end_ex(C, block, &window->eventstate->x);
}

/* ************** BLOCK DRAWING FUNCTION ************* */

void ui_fontscale(short *points, float aspect)
{
	if (aspect < 0.9f || aspect > 1.1f) {
		float pointsf = *points;
		
		/* for some reason scaling fonts goes too fast compared to widget size */
		/* XXX not true anymore? (ton) */
		//aspect = sqrt(aspect);
		pointsf /= aspect;
		
		if (aspect > 1.0f)
			*points = ceilf(pointsf);
		else
			*points = floorf(pointsf);
	}
}

/* project button or block (but==NULL) to pixels in regionspace */
static void ui_but_to_pixelrect(rcti *rect, const ARegion *ar, uiBlock *block, uiBut *but)
{
	rctf rectf;

	ui_block_to_window_rctf(ar, block, &rectf, (but) ? &but->rect : &block->rect);

	rectf.xmin -= ar->winrct.xmin;
	rectf.ymin -= ar->winrct.ymin;
	rectf.xmax -= ar->winrct.xmin;
	rectf.ymax -= ar->winrct.ymin;

	rect->xmin = floorf(rectf.xmin);
	rect->ymin = floorf(rectf.ymin);
	rect->xmax = floorf(rectf.xmax);
	rect->ymax = floorf(rectf.ymax);
}

/* uses local copy of style, to scale things down, and allow widgets to change stuff */
void UI_block_draw(const bContext *C, uiBlock *block)
{
	uiStyle style = *UI_style_get_dpi();  /* XXX pass on as arg */
	ARegion *ar;
	uiBut *but;
	rcti rect;
	int multisample_enabled;
	
	/* get menu region or area region */
	ar = CTX_wm_menu(C);
	if (!ar)
		ar = CTX_wm_region(C);

	if (!block->endblock)
		UI_block_end(C, block);

	/* disable AA, makes widgets too blurry */
	multisample_enabled = glIsEnabled(GL_MULTISAMPLE_ARB);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE_ARB);

	/* we set this only once */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	/* scale fonts */
	ui_fontscale(&style.paneltitle.points, block->aspect);
	ui_fontscale(&style.grouplabel.points, block->aspect);
	ui_fontscale(&style.widgetlabel.points, block->aspect);
	ui_fontscale(&style.widget.points, block->aspect);
	
	/* scale block min/max to rect */
	ui_but_to_pixelrect(&rect, ar, block, NULL);
	
	/* pixel space for AA widgets */
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	wmOrtho2_region_ui(ar);
	
	/* back */
	if (block->flag & UI_BLOCK_RADIAL)
		ui_draw_pie_center(block);
	else if (block->flag & UI_BLOCK_LOOP)
		ui_draw_menu_back(&style, block, &rect);
	else if (block->panel)
		ui_draw_aligned_panel(&style, block, &rect, UI_panel_category_is_visible(ar));

	/* widgets */
	for (but = block->buttons.first; but; but = but->next) {
		if (!(but->flag & (UI_HIDDEN | UI_SCROLLED))) {
			ui_but_to_pixelrect(&rect, ar, block, but);
		
			/* XXX: figure out why invalid coordinates happen when closing render window */
			/* and material preview is redrawn in main window (temp fix for bug #23848) */
			if (rect.xmin < rect.xmax && rect.ymin < rect.ymax)
				ui_draw_but(C, ar, &style, but, &rect);
		}
	}
	
	/* restore matrix */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE_ARB);
	
	ui_draw_links(block);
}

/* ************* EVENTS ************* */

/**
 * Check if the button is pushed, this is only meaningful for some button types.
 *
 * \return (0 == UNSELECT), (1 == SELECT), (-1 == DO-NOTHING)
 */
int ui_but_is_pushed_ex(uiBut *but, double *value)
{
	int is_push = 0;

	if (but->bit) {
		const bool state = ELEM(but->type, UI_BTYPE_TOGGLE_N, UI_BTYPE_ICON_TOGGLE_N, UI_BTYPE_CHECKBOX_N) ? false : true;
		int lvalue;
		UI_GET_BUT_VALUE_INIT(but, *value);
		lvalue = (int)*value;
		if (UI_BITBUT_TEST(lvalue, (but->bitnr))) {
			is_push = state;
		}
		else {
			is_push = !state;
		}
	}
	else {
		switch (but->type) {
			case UI_BTYPE_BUT:
			case UI_BTYPE_HOTKEY_EVENT:
			case UI_BTYPE_KEY_EVENT:
			case UI_BTYPE_COLOR:
				is_push = -1;
				break;
			case UI_BTYPE_BUT_TOGGLE:
			case UI_BTYPE_TOGGLE:
			case UI_BTYPE_ICON_TOGGLE:
			case UI_BTYPE_CHECKBOX:
				UI_GET_BUT_VALUE_INIT(but, *value);
				if (*value != (double)but->hardmin) is_push = true;
				break;
			case UI_BTYPE_ICON_TOGGLE_N:
			case UI_BTYPE_TOGGLE_N:
			case UI_BTYPE_CHECKBOX_N:
				UI_GET_BUT_VALUE_INIT(but, *value);
				if (*value == 0.0) is_push = true;
				break;
			case UI_BTYPE_ROW:
			case UI_BTYPE_LISTROW:
				UI_GET_BUT_VALUE_INIT(but, *value);
				/* support for rna enum buts */
				if (but->rnaprop && (RNA_property_flag(but->rnaprop) & PROP_ENUM_FLAG)) {
					if ((int)*value & (int)but->hardmax) is_push = true;
				}
				else {
					if (*value == (double)but->hardmax) is_push = true;
				}
				break;
			default:
				is_push = -1;
				break;
		}
	}

	return is_push;
}
int ui_but_is_pushed(uiBut *but)
{
	double value = UI_BUT_VALUE_UNSET;
	return ui_but_is_pushed_ex(but, &value);
}

static void ui_but_update_select_flag(uiBut *but, double *value)
{
	switch (ui_but_is_pushed_ex(but, value)) {
		case true:
			but->flag |= UI_SELECT;
			break;
		case false:
			but->flag &= ~UI_SELECT;
			break;
	}
}

static uiBut *ui_linkline_find_inlink(uiBlock *block, void *poin)
{
	uiBut *but;
	
	but = block->buttons.first;
	while (but) {
		if (but->type == UI_BTYPE_INLINK) {
			if (but->poin == poin) return but;
		}
		but = but->next;
	}
	return NULL;
}

static void ui_linkline_add(ListBase *listb, uiBut *but, uiBut *bt, short deactive)
{
	uiLinkLine *line;
	
	line = MEM_callocN(sizeof(uiLinkLine), "linkline");
	BLI_addtail(listb, line);
	line->from = but;
	line->to = bt;
	line->deactive = deactive;
}

uiBut *UI_block_links_find_inlink(uiBlock *block, void *poin)
{
	return ui_linkline_find_inlink(block, poin);
}

void UI_block_links_compose(uiBlock *block)
{
	uiBut *but, *bt;
	uiLink *link;
	void ***ppoin;
	int a;
	
	but = block->buttons.first;
	while (but) {
		if (but->type == UI_BTYPE_LINK) {
			link = but->link;
			
			/* for all pointers in the array */
			if (link) {
				if (link->ppoin) {
					ppoin = link->ppoin;
					for (a = 0; a < *(link->totlink); a++) {
						bt = ui_linkline_find_inlink(block, (*ppoin)[a]);
						if (bt) {
							if ((but->flag & UI_BUT_SCA_LINK_GREY) || (bt->flag & UI_BUT_SCA_LINK_GREY)) {
								ui_linkline_add(&link->lines, but, bt, true);
							}
							else {
								ui_linkline_add(&link->lines, but, bt, false);
							}

						}
					}
				}
				else if (link->poin) {
					bt = ui_linkline_find_inlink(block, *link->poin);
					if (bt) {
						if ((but->flag & UI_BUT_SCA_LINK_GREY) || (bt->flag & UI_BUT_SCA_LINK_GREY)) {
							ui_linkline_add(&link->lines, but, bt, true);
						}
						else {
							ui_linkline_add(&link->lines, but, bt, false);
						}
					}
				}
			}
		}
		but = but->next;
	}
}


/* ************************************************ */

void UI_block_lock_set(uiBlock *block, bool val, const char *lockstr)
{
	if (val) {
		block->lock = val;
		block->lockstr = lockstr;
	}
}

void UI_block_lock_clear(uiBlock *block)
{
	block->lock = false;
	block->lockstr = NULL;
}

/* *************************************************************** */

void ui_linkline_remove(uiLinkLine *line, uiBut *but)
{
	uiLink *link;
	int a, b;
	
	BLI_remlink(&but->link->lines, line);

	link = line->from->link;

	/* are there more pointers allowed? */
	if (link->ppoin) {
		
		if (*(link->totlink) == 1) {
			*(link->totlink) = 0;
			MEM_freeN(*(link->ppoin));
			*(link->ppoin) = NULL;
		}
		else {
			b = 0;
			for (a = 0; a < (*(link->totlink)); a++) {
				if ((*(link->ppoin))[a] != line->to->poin) {
					(*(link->ppoin))[b] = (*(link->ppoin))[a];
					b++;
				}
			}
			(*(link->totlink))--;
		}
	}
	else {
		*(link->poin) = NULL;
	}

	MEM_freeN(line);
	//REDRAW
}

/* *********************** data get/set ***********************
 * this either works with the pointed to data, or can work with
 * an edit override pointer while dragging for example */

/* for buttons pointing to color for example */
void ui_but_v3_get(uiBut *but, float vec[3])
{
	PropertyRNA *prop;
	int a;

	if (but->editvec) {
		copy_v3_v3(vec, but->editvec);
	}

	if (but->rnaprop) {
		prop = but->rnaprop;

		zero_v3(vec);

		if (RNA_property_type(prop) == PROP_FLOAT) {
			int tot = RNA_property_array_length(&but->rnapoin, prop);
			BLI_assert(tot > 0);
			if (tot == 3) {
				RNA_property_float_get_array(&but->rnapoin, prop, vec);
			}
			else {
				tot = min_ii(tot, 3);
				for (a = 0; a < tot; a++) {
					vec[a] = RNA_property_float_get_index(&but->rnapoin, prop, a);
				}
			}
		}
	}
	else if (but->pointype == UI_BUT_POIN_CHAR) {
		const char *cp = (char *)but->poin;

		vec[0] = ((float)cp[0]) / 255.0f;
		vec[1] = ((float)cp[1]) / 255.0f;
		vec[2] = ((float)cp[2]) / 255.0f;
	}
	else if (but->pointype == UI_BUT_POIN_FLOAT) {
		const float *fp = (float *)but->poin;
		copy_v3_v3(vec, fp);
	}
	else {
		if (but->editvec == NULL) {
			fprintf(stderr, "%s: can't get color, should never happen\n", __func__);
			zero_v3(vec);
		}
	}

	if (but->type == UI_BTYPE_UNITVEC) {
		normalize_v3(vec);
	}
}

/* for buttons pointing to color for example */
void ui_but_v3_set(uiBut *but, const float vec[3])
{
	PropertyRNA *prop;

	if (but->editvec) {
		copy_v3_v3(but->editvec, vec);
	}

	if (but->rnaprop) {
		prop = but->rnaprop;

		if (RNA_property_type(prop) == PROP_FLOAT) {
			int tot;
			int a;

			tot = RNA_property_array_length(&but->rnapoin, prop);
			BLI_assert(tot > 0);
			if (tot == 3) {
				RNA_property_float_set_array(&but->rnapoin, prop, vec);
			}
			else {
				tot = min_ii(tot, 3);
				for (a = 0; a < tot; a++) {
					RNA_property_float_set_index(&but->rnapoin, prop, a, vec[a]);
				}
			}
		}
	}
	else if (but->pointype == UI_BUT_POIN_CHAR) {
		char *cp = (char *)but->poin;
		cp[0] = (char)(0.5f + vec[0] * 255.0f);
		cp[1] = (char)(0.5f + vec[1] * 255.0f);
		cp[2] = (char)(0.5f + vec[2] * 255.0f);
	}
	else if (but->pointype == UI_BUT_POIN_FLOAT) {
		float *fp = (float *)but->poin;
		copy_v3_v3(fp, vec);
	}
}

bool ui_but_is_float(const uiBut *but)
{
	if (but->pointype == UI_BUT_POIN_FLOAT && but->poin)
		return true;
	
	if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_FLOAT)
		return true;
	
	return false;
}

bool ui_but_is_bool(const uiBut *but)
{
	if (ELEM(but->type, UI_BTYPE_TOGGLE, UI_BTYPE_TOGGLE_N, UI_BTYPE_ICON_TOGGLE, UI_BTYPE_ICON_TOGGLE_N))
		return true;

	if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_BOOLEAN)
		return true;

	if ((but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) && (but->type == UI_BTYPE_ROW))
		return true;

	return false;
}


bool ui_but_is_unit(const uiBut *but)
{
	UnitSettings *unit = but->block->unit;
	const int unit_type = UI_but_unit_type_get(but);

	if (unit_type == PROP_UNIT_NONE)
		return false;

#if 1 /* removed so angle buttons get correct snapping */
	if (ui_but_is_unit_radians_ex(unit, unit_type))
		return false;
#endif
	
	/* for now disable time unit conversion */
	if (unit_type == PROP_UNIT_TIME)
		return false;

	if (unit->system == USER_UNIT_NONE) {
		if (unit_type != PROP_UNIT_ROTATION) {
			return false;
		}
	}

	return true;
}

/**
 * Check if this button is similar enough to be grouped with another.
 */
bool ui_but_is_compatible(const uiBut *but_a, const uiBut *but_b)
{
	if (but_a->type != but_b->type)
		return false;
	if (but_a->pointype != but_b->pointype)
		return false;

	if (but_a->rnaprop) {
		/* skip 'rnapoin.data', 'rnapoin.id.data'
		 * allow different data to have the same props edited at once */
		if (but_a->rnapoin.type != but_b->rnapoin.type)
			return false;
		if (RNA_property_type(but_a->rnaprop) != RNA_property_type(but_b->rnaprop))
			return false;
		if (RNA_property_subtype(but_a->rnaprop) != RNA_property_subtype(but_b->rnaprop))
			return false;
	}

	return true;
}

bool ui_but_is_rna_valid(uiBut *but)
{
	if (but->rnaprop == NULL || RNA_struct_contains_property(&but->rnapoin, but->rnaprop)) {
		return true;
	}
	else {
		printf("property removed %s: %p\n", but->drawstr, but->rnaprop);
		return false;
	}
}

double ui_but_value_get(uiBut *but)
{
	PropertyRNA *prop;
	double value = 0.0;

	if (but->editval) { return *(but->editval); }
	if (but->poin == NULL && but->rnapoin.data == NULL) return 0.0;

	if (but->rnaprop) {
		prop = but->rnaprop;

		BLI_assert(but->rnaindex != -1);

		switch (RNA_property_type(prop)) {
			case PROP_BOOLEAN:
				if (RNA_property_array_check(prop))
					value = RNA_property_boolean_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value = RNA_property_boolean_get(&but->rnapoin, prop);
				break;
			case PROP_INT:
				if (RNA_property_array_check(prop))
					value = RNA_property_int_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value = RNA_property_int_get(&but->rnapoin, prop);
				break;
			case PROP_FLOAT:
				if (RNA_property_array_check(prop))
					value = RNA_property_float_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value = RNA_property_float_get(&but->rnapoin, prop);
				break;
			case PROP_ENUM:
				value = RNA_property_enum_get(&but->rnapoin, prop);
				break;
			default:
				value = 0.0;
				break;
		}
	}
	else if (but->pointype == UI_BUT_POIN_CHAR) {
		value = *(char *)but->poin;
	}
	else if (but->pointype == UI_BUT_POIN_SHORT) {
		value = *(short *)but->poin;
	}
	else if (but->pointype == UI_BUT_POIN_INT) {
		value = *(int *)but->poin;
	}
	else if (but->pointype == UI_BUT_POIN_FLOAT) {
		value = *(float *)but->poin;
	}

	return value;
}

void ui_but_value_set(uiBut *but, double value)
{
	PropertyRNA *prop;

	/* value is a hsv value: convert to rgb */
	if (but->rnaprop) {
		prop = but->rnaprop;

		if (RNA_property_editable(&but->rnapoin, prop)) {
			switch (RNA_property_type(prop)) {
				case PROP_BOOLEAN:
					if (RNA_property_array_check(prop))
						RNA_property_boolean_set_index(&but->rnapoin, prop, but->rnaindex, value);
					else
						RNA_property_boolean_set(&but->rnapoin, prop, value);
					break;
				case PROP_INT:
					if (RNA_property_array_check(prop))
						RNA_property_int_set_index(&but->rnapoin, prop, but->rnaindex, (int)value);
					else
						RNA_property_int_set(&but->rnapoin, prop, (int)value);
					break;
				case PROP_FLOAT:
					if (RNA_property_array_check(prop))
						RNA_property_float_set_index(&but->rnapoin, prop, but->rnaindex, value);
					else
						RNA_property_float_set(&but->rnapoin, prop, value);
					break;
				case PROP_ENUM:
					if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
						int ivalue = (int)value;
						ivalue ^= RNA_property_enum_get(&but->rnapoin, prop); /* toggle for enum/flag buttons */
						RNA_property_enum_set(&but->rnapoin, prop, ivalue);
					}
					else {
						RNA_property_enum_set(&but->rnapoin, prop, value);
					}
					break;
				default:
					break;
			}
		}

		/* we can't be sure what RNA set functions actually do,
		 * so leave this unset */
		value = UI_BUT_VALUE_UNSET;
	}
	else if (but->pointype == 0) {
		/* pass */
	}
	else {
		/* first do rounding */
		if (but->pointype == UI_BUT_POIN_CHAR) {
			value = (char)floor(value + 0.5);
		}
		else if (but->pointype == UI_BUT_POIN_SHORT) {
			value = (short)floor(value + 0.5);
		}
		else if (but->pointype == UI_BUT_POIN_INT)
			value = (int)floor(value + 0.5);
		else if (but->pointype == UI_BUT_POIN_FLOAT) {
			float fval = (float)value;
			if (fval >= -0.00001f && fval <= 0.00001f) fval = 0.0f;  /* prevent negative zero */
			value = fval;
		}
		
		/* then set value with possible edit override */
		if (but->editval)
			value = *but->editval = value;
		else if (but->pointype == UI_BUT_POIN_CHAR)
			value = *((char *)but->poin) = (char)value;
		else if (but->pointype == UI_BUT_POIN_SHORT)
			value = *((short *)but->poin) = (short)value;
		else if (but->pointype == UI_BUT_POIN_INT)
			value = *((int *)but->poin) = (int)value;
		else if (but->pointype == UI_BUT_POIN_FLOAT)
			value = *((float *)but->poin) = (float)value;
	}

	ui_but_update_select_flag(but, &value);
}

int ui_but_string_get_max_length(uiBut *but)
{
	if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU, UI_BTYPE_SEARCH_MENU_UNLINK))
		return but->hardmax;
	else
		return UI_MAX_DRAW_STR;
}

uiBut *ui_but_drag_multi_edit_get(uiBut *but)
{
	uiBut *but_iter;

	BLI_assert(but->flag & UI_BUT_DRAG_MULTI);

	for (but_iter = but->block->buttons.first; but_iter; but_iter = but_iter->next) {
		if (but_iter->editstr) {
			break;
		}
	}

	return but_iter;
}

static double ui_get_but_scale_unit(uiBut *but, double value)
{
	UnitSettings *unit = but->block->unit;
	int unit_type = UI_but_unit_type_get(but);

	/* Time unit is a bit special, not handled by BKE_scene_unit_scale() for now. */
	if (unit_type == PROP_UNIT_TIME) { /* WARNING - using evil_C :| */
		Scene *scene = CTX_data_scene(but->block->evil_C);
		return FRA2TIME(value);
	}
	else {
		return BKE_scene_unit_scale(unit, RNA_SUBTYPE_UNIT_VALUE(unit_type), value);
	}
}

/* str will be overwritten */
void ui_but_convert_to_unit_alt_name(uiBut *but, char *str, size_t maxlen)
{
	if (ui_but_is_unit(but)) {
		UnitSettings *unit = but->block->unit;
		int unit_type = UI_but_unit_type_get(but);
		char *orig_str;
		
		orig_str = BLI_strdup(str);
		
		bUnit_ToUnitAltName(str, maxlen, orig_str, unit->system, RNA_SUBTYPE_UNIT_VALUE(unit_type));
		
		MEM_freeN(orig_str);
	}
}

/**
 * \param float_precision  Override the button precision.
 */
static void ui_get_but_string_unit(uiBut *but, char *str, int len_max, double value, bool pad, int float_precision)
{
	UnitSettings *unit = but->block->unit;
	const bool do_split = (unit->flag & USER_UNIT_OPT_SPLIT) != 0;
	int unit_type = UI_but_unit_type_get(but);
	int precision;

	if (unit->scale_length < 0.0001f) unit->scale_length = 1.0f;  // XXX do_versions

	/* Use precision override? */
	if (float_precision == -1) {
		/* Sanity checks */
		precision = (int)but->a2;
		if      (precision > UI_PRECISION_FLOAT_MAX) precision = UI_PRECISION_FLOAT_MAX;
		else if (precision == -1)                    precision = 2;
	}
	else {
		precision = float_precision;
	}

	bUnit_AsString(str, len_max, ui_get_but_scale_unit(but, value), precision,
	               unit->system, RNA_SUBTYPE_UNIT_VALUE(unit_type), do_split, pad);
}

static float ui_get_but_step_unit(uiBut *but, float step_default)
{
	int unit_type = RNA_SUBTYPE_UNIT_VALUE(UI_but_unit_type_get(but));
	double step;

	step = bUnit_ClosestScalar(ui_get_but_scale_unit(but, step_default), but->block->unit->system, unit_type);

	/* -1 is an error value */
	if (step != -1.0) {
		BLI_assert(step > 0.0);
		return (float)(step / ui_get_but_scale_unit(but, 1.0)) * 100.0f;
	}
	else {
		return step_default;
	}
}

/**
 * \param float_precision  For number buttons the precision to use or -1 to fallback to the button default.
 */
void ui_but_string_get_ex(uiBut *but, char *str, const size_t maxlen, const int float_precision)
{
	if (but->rnaprop && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU, UI_BTYPE_SEARCH_MENU_UNLINK)) {
		PropertyType type;
		const char *buf = NULL;
		int buf_len;

		type = RNA_property_type(but->rnaprop);

		if (type == PROP_STRING) {
			/* RNA string */
			buf = RNA_property_string_get_alloc(&but->rnapoin, but->rnaprop, str, maxlen, &buf_len);
		}
		else if (type == PROP_ENUM) {
			/* RNA enum */
			int value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
			if (RNA_property_enum_name(but->block->evil_C, &but->rnapoin, but->rnaprop, value, &buf)) {
				BLI_strncpy(str, buf, maxlen);
				buf = str;
			}
		}
		else if (type == PROP_POINTER) {
			/* RNA pointer */
			PointerRNA ptr = RNA_property_pointer_get(&but->rnapoin, but->rnaprop);
			buf = RNA_struct_name_get_alloc(&ptr, str, maxlen, &buf_len);
		}
		else {
			BLI_assert(0);
		}

		if (!buf) {
			str[0] = '\0';
		}
		else if (buf && buf != str) {
			/* string was too long, we have to truncate */
			memcpy(str, buf, MIN2(maxlen, (size_t)(buf_len + 1)));
			MEM_freeN((void *)buf);
		}
	}
	else if (but->type == UI_BTYPE_TEXT) {
		/* string */
		BLI_strncpy(str, but->poin, maxlen);
		return;
	}
	else if (ELEM(but->type, UI_BTYPE_SEARCH_MENU, UI_BTYPE_SEARCH_MENU_UNLINK)) {
		/* string */
		BLI_strncpy(str, but->poin, maxlen);
		return;
	}
	else if (ui_but_anim_expression_get(but, str, maxlen)) {
		/* driver expression */
	}
	else {
		/* number editing */
		double value;

		value = ui_but_value_get(but);

		if (ui_but_is_float(but)) {
			if (ui_but_is_unit(but)) {
				ui_get_but_string_unit(but, str, maxlen, value, false, float_precision);
			}
			else {
				const int prec = (float_precision == -1) ? ui_but_calc_float_precision(but, value) : float_precision;
				BLI_snprintf(str, maxlen, "%.*f", prec, value);
			}
		}
		else
			BLI_snprintf(str, maxlen, "%d", (int)value);
	}
}
void ui_but_string_get(uiBut *but, char *str, const size_t maxlen)
{
	ui_but_string_get_ex(but, str, maxlen, -1);
}

#ifdef WITH_PYTHON

static bool ui_set_but_string_eval_num_unit(bContext *C, uiBut *but, const char *str, double *value)
{
	char str_unit_convert[256];
	const int unit_type = UI_but_unit_type_get(but);

	BLI_strncpy(str_unit_convert, str, sizeof(str_unit_convert));

	/* ugly, use the draw string to get the value,
	 * this could cause problems if it includes some text which resolves to a unit */
	bUnit_ReplaceString(str_unit_convert, sizeof(str_unit_convert), but->drawstr,
	                    ui_get_but_scale_unit(but, 1.0), but->block->unit->system, RNA_SUBTYPE_UNIT_VALUE(unit_type));

	return (BPY_button_exec(C, str_unit_convert, value, true) != -1);
}

#endif /* WITH_PYTHON */


bool ui_but_string_set_eval_num(bContext *C, uiBut *but, const char *str, double *value)
{
	bool ok = false;

#ifdef WITH_PYTHON

	if (str[0] != '\0') {
		bool is_unit_but = (ui_but_is_float(but) && ui_but_is_unit(but));
		/* only enable verbose if we won't run again with units */
		if (BPY_button_exec(C, str, value, is_unit_but == false) != -1) {
			/* if the value parsed ok without unit conversion this button may still need a unit multiplier */
			if (is_unit_but) {
				char str_new[128];

				BLI_snprintf(str_new, sizeof(str_new), "%f", *value);
				ok = ui_set_but_string_eval_num_unit(C, but, str_new, value);
			}
			else {
				ok = true; /* parse normal string via py (no unit conversion needed) */
			}
		}
		else if (is_unit_but) {
			/* parse failed, this is a unit but so run replacements and parse again */
			ok = ui_set_but_string_eval_num_unit(C, but, str, value);
		}
	}

#else /* WITH_PYTHON */

	*value = atof(str);
	ok = true;

	(void)C;
	(void)but;

#endif /* WITH_PYTHON */

	return ok;
}


bool ui_but_string_set(bContext *C, uiBut *but, const char *str)
{
	if (but->rnaprop && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU, UI_BTYPE_SEARCH_MENU_UNLINK)) {
		if (RNA_property_editable(&but->rnapoin, but->rnaprop)) {
			PropertyType type;

			type = RNA_property_type(but->rnaprop);

			if (type == PROP_STRING) {
				/* RNA string */
				RNA_property_string_set(&but->rnapoin, but->rnaprop, str);
				return true;
			}
			else if (type == PROP_POINTER) {
				/* RNA pointer */
				PointerRNA ptr, rptr;
				PropertyRNA *prop;

				if (str[0] == '\0') {
					RNA_property_pointer_set(&but->rnapoin, but->rnaprop, PointerRNA_NULL);
					return true;
				}
				else {
					ptr = but->rnasearchpoin;
					prop = but->rnasearchprop;
					
					if (prop && RNA_property_collection_lookup_string(&ptr, prop, str, &rptr))
						RNA_property_pointer_set(&but->rnapoin, but->rnaprop, rptr);

					return true;
				}

				return false;
			}
			else if (type == PROP_ENUM) {
				int value;
				if (RNA_property_enum_value(but->block->evil_C, &but->rnapoin, but->rnaprop, str, &value)) {
					RNA_property_enum_set(&but->rnapoin, but->rnaprop, value);
					return true;
				}
				return false;
			}
			else {
				BLI_assert(0);
			}
		}
	}
	else if (but->type == UI_BTYPE_TEXT) {
		/* string */
		if (ui_but_is_utf8(but)) BLI_strncpy_utf8(but->poin, str, but->hardmax);
		else BLI_strncpy(but->poin, str, but->hardmax);

		return true;
	}
	else if (ELEM(but->type, UI_BTYPE_SEARCH_MENU, UI_BTYPE_SEARCH_MENU_UNLINK)) {
		/* string */
		BLI_strncpy(but->poin, str, but->hardmax);
		return true;
	}
	else if (ui_but_anim_expression_set(but, str)) {
		/* driver expression */
		return true;
	}
	else if (str[0] == '#') {
		/* shortcut to create new driver expression (versus immediate Py-execution) */
		return ui_but_anim_expression_create(but, str + 1);
	}
	else {
		/* number editing */
		double value;

		if (ui_but_string_set_eval_num(C, but, str, &value) == false) {
			return false;
		}

		if (!ui_but_is_float(but)) value = (int)floor(value + 0.5);

		/* not that we use hard limits here */
		if (value < (double)but->hardmin) value = but->hardmin;
		if (value > (double)but->hardmax) value = but->hardmax;

		ui_but_value_set(but, value);
		return true;
	}

	return false;
}

void ui_but_default_set(bContext *C, const bool all, const bool use_afterfunc)
{
	wmOperatorType *ot = WM_operatortype_find("UI_OT_reset_default_button", true);

	if (use_afterfunc) {
		PointerRNA *ptr;
		ptr = ui_handle_afterfunc_add_operator(ot, WM_OP_EXEC_DEFAULT, true);
		RNA_boolean_set(ptr, "all", all);
	}
	else {
		PointerRNA ptr;
		WM_operator_properties_create_ptr(&ptr, ot);
		RNA_boolean_set(&ptr, "all", all);
		WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &ptr);
		WM_operator_properties_free(&ptr);
	}
}

static double soft_range_round_up(double value, double max)
{
	/* round up to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, ..
	 * checking for 0.0 prevents floating point exceptions */
	double newmax = (value != 0.0) ? pow(10.0, ceil(log(value) / M_LN10)) : 0.0;

	if (newmax * 0.2 >= max && newmax * 0.2 >= value)
		return newmax * 0.2;
	else if (newmax * 0.5 >= max && newmax * 0.5 >= value)
		return newmax * 0.5;
	else
		return newmax;
}

static double soft_range_round_down(double value, double max)
{
	/* round down to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, ..
	 * checking for 0.0 prevents floating point exceptions */
	double newmax = (value != 0.0) ? pow(10.0, floor(log(value) / M_LN10)) : 0.0;

	if (newmax * 5.0 <= max && newmax * 5.0 <= value)
		return newmax * 5.0;
	else if (newmax * 2.0 <= max && newmax * 2.0 <= value)
		return newmax * 2.0;
	else
		return newmax;
}

/* note: this could be split up into functions which handle arrays and not */
static void ui_set_but_soft_range(uiBut *but)
{
	/* ideally we would not limit this but practically, its more than
	 * enough worst case is very long vectors wont use a smart soft-range
	 * which isn't so bad. */

	if (but->rnaprop) {
		const PropertyType type = RNA_property_type(but->rnaprop);
		double softmin, softmax /*, step, precision*/;
		double value_min;
		double value_max;

		/* clamp button range to something reasonable in case
		 * we get -inf/inf from RNA properties */
		if (type == PROP_INT) {
			const bool is_array = RNA_property_array_check(but->rnaprop);
			int imin, imax, istep;

			RNA_property_int_ui_range(&but->rnapoin, but->rnaprop, &imin, &imax, &istep);
			softmin = (imin == INT_MIN) ? -1e4 : imin;
			softmax = (imin == INT_MAX) ? 1e4 : imax;
			/*step = istep;*/ /*UNUSED*/
			/*precision = 1;*/ /*UNUSED*/

			if (is_array) {
				int value_range[2];
				RNA_property_int_get_array_range(&but->rnapoin, but->rnaprop, value_range);
				value_min = (double)value_range[0];
				value_max = (double)value_range[1];
			}
			else {
				value_min = value_max = (double)RNA_property_int_get(&but->rnapoin, but->rnaprop);
			}
		}
		else if (type == PROP_FLOAT) {
			const bool is_array = RNA_property_array_check(but->rnaprop);
			float fmin, fmax, fstep, fprecision;

			RNA_property_float_ui_range(&but->rnapoin, but->rnaprop, &fmin, &fmax, &fstep, &fprecision);
			softmin = (fmin == -FLT_MAX) ? (float)-1e4 : fmin;
			softmax = (fmax == FLT_MAX) ? (float)1e4 : fmax;
			/*step = fstep;*/ /*UNUSED*/
			/*precision = fprecision;*/ /*UNUSED*/

			if (is_array) {
				float value_range[2];
				RNA_property_float_get_array_range(&but->rnapoin, but->rnaprop, value_range);
				value_min = (double)value_range[0];
				value_max = (double)value_range[1];
			}
			else {
				value_min = value_max = (double)RNA_property_float_get(&but->rnapoin, but->rnaprop);
			}
		}
		else {
			return;
		}

		/* if the value goes out of the soft/max range, adapt the range */
		if (value_min + 1e-10 < softmin) {
			if (value_min < 0.0)
				softmin = -soft_range_round_up(-value_min, -softmin);
			else
				softmin = soft_range_round_down(value_min, softmin);

			if (softmin < (double)but->hardmin)
				softmin = (double)but->hardmin;
		}
		if (value_max - 1e-10 > softmax) {
			if (value_max < 0.0)
				softmax = -soft_range_round_down(-value_max, -softmax);
			else
				softmax = soft_range_round_up(value_max, softmax);

			if (softmax > (double)but->hardmax)
				softmax = but->hardmax;
		}

		but->softmin = softmin;
		but->softmax = softmax;
	}
	else if (but->poin && (but->pointype & UI_BUT_POIN_TYPES)) {
		float value = ui_but_value_get(but);
		CLAMP(value, but->hardmin, but->hardmax);
		but->softmin = min_ff(but->softmin, value);
		but->softmax = max_ff(but->softmax, value);
	}
	else {
		BLI_assert(0);
	}
}

/* ******************* Free ********************/

static void ui_free_link(uiLink *link)
{
	if (link) {
		BLI_freelistN(&link->lines);
		MEM_freeN(link);
	}
}

/* can be called with C==NULL */
static void ui_but_free(const bContext *C, uiBut *but)
{
	if (but->opptr) {
		WM_operator_properties_free(but->opptr);
		MEM_freeN(but->opptr);
	}

	if (but->func_argN) {
		MEM_freeN(but->func_argN);
	}

	if (but->active) {
		/* XXX solve later, buttons should be free-able without context ideally,
		 * however they may have open tooltips or popup windows, which need to
		 * be closed using a context pointer */
		if (C) {
			ui_but_active_free(C, but);
		}
		else {
			if (but->active) {
				MEM_freeN(but->active);
			}
		}
	}
	if (but->str && but->str != but->strdata) {
		MEM_freeN(but->str);
	}
	ui_free_link(but->link);

	if ((but->type == UI_BTYPE_IMAGE) && but->poin) {
		IMB_freeImBuf((struct ImBuf *)but->poin);
	}

	BLI_assert(UI_butstore_is_registered(but->block, but) == false);

	MEM_freeN(but);
}

/* can be called with C==NULL */
void UI_block_free(const bContext *C, uiBlock *block)
{
	uiBut *but;

	UI_butstore_clear(block);

	while ((but = BLI_pophead(&block->buttons))) {
		ui_but_free(C, but);
	}

	if (block->unit) {
		MEM_freeN(block->unit);
	}

	if (block->func_argN) {
		MEM_freeN(block->func_argN);
	}

	CTX_store_free_list(&block->contexts);

	BLI_freelistN(&block->saferct);
	BLI_freelistN(&block->color_pickers.list);
	
	MEM_freeN(block);
}

/* can be called with C==NULL */
void UI_blocklist_free(const bContext *C, ListBase *lb)
{
	uiBlock *block;
	
	while ((block = BLI_pophead(lb))) {
		UI_block_free(C, block);
	}
}

void UI_blocklist_free_inactive(const bContext *C, ListBase *lb)
{
	uiBlock *block, *nextblock;

	for (block = lb->first; block; block = nextblock) {
		nextblock = block->next;
	
		if (!block->handle) {
			if (!block->active) {
				BLI_remlink(lb, block);
				UI_block_free(C, block);
			}
			else
				block->active = 0;
		}
	}
}

void UI_block_region_set(uiBlock *block, ARegion *region)
{
	ListBase *lb = &region->uiblocks;
	uiBlock *oldblock = NULL;

	/* each listbase only has one block with this name, free block
	 * if is already there so it can be rebuilt from scratch */
	if (lb) {
		oldblock = BLI_findstring(lb, block->name, offsetof(uiBlock, name));

		if (oldblock) {
			oldblock->active = 0;
			oldblock->panel = NULL;
			oldblock->handle = NULL;
		}

		/* at the beginning of the list! for dynamical menus/blocks */
		BLI_addhead(lb, block);
	}

	block->oldblock = oldblock;
}

uiBlock *UI_block_begin(const bContext *C, ARegion *region, const char *name, short dt)
{
	uiBlock *block;
	wmWindow *window;
	Scene *scn;
	int getsizex, getsizey;

	window = CTX_wm_window(C);
	scn = CTX_data_scene(C);

	block = MEM_callocN(sizeof(uiBlock), "uiBlock");
	block->active = 1;
	block->dt = dt;
	block->evil_C = (void *)C;  /* XXX */

	if (scn) {
		block->color_profile = true;

		/* store display device name, don't lookup for transformations yet
		 * block could be used for non-color displays where looking up for transformation
		 * would slow down redraw, so only lookup for actual transform when it's indeed
		 * needed
		 */
		BLI_strncpy(block->display_device, scn->display_settings.display_device, sizeof(block->display_device));

		/* copy to avoid crash when scene gets deleted with ui still open */
		block->unit = MEM_mallocN(sizeof(scn->unit), "UI UnitSettings");
		memcpy(block->unit, &scn->unit, sizeof(scn->unit));
	}

	BLI_strncpy(block->name, name, sizeof(block->name));

	if (region)
		UI_block_region_set(block, region);

	/* window matrix and aspect */
	if (region && region->swinid) {
		wm_subwindow_matrix_get(window, region->swinid, block->winmat);
		wm_subwindow_size_get(window, region->swinid, &getsizex, &getsizey);

		block->aspect = 2.0f / fabsf(getsizex * block->winmat[0][0]);
	}
	else {
		/* no subwindow created yet, for menus for example, so we
		 * use the main window instead, since buttons are created
		 * there anyway */
		wm_subwindow_matrix_get(window, window->screen->mainwin, block->winmat);
		wm_subwindow_size_get(window, window->screen->mainwin, &getsizex, &getsizey);

		block->aspect = 2.0f / fabsf(getsizex * block->winmat[0][0]);
		block->auto_open = true;
		block->flag |= UI_BLOCK_LOOP; /* tag as menu */
	}

	return block;
}

uiBlock *UI_block_find_in_region(const char *name, ARegion *ar)
{
	return BLI_findstring(&ar->uiblocks, name, offsetof(uiBlock, name));
}

void UI_block_emboss_set(uiBlock *block, char dt)
{
	block->dt = dt;
}

void ui_but_update(uiBut *but)
{
	/* if something changed in the button */
	double value = UI_BUT_VALUE_UNSET;
//	float okwidth; // UNUSED
	
	ui_but_update_select_flag(but, &value);
	
	/* only update soft range while not editing */
	if (!(but->editval || but->editstr || but->editvec)) {
		if ((but->rnaprop != NULL) ||
		    (but->poin && (but->pointype & UI_BUT_POIN_TYPES)))
		{
			ui_set_but_soft_range(but);
		}
	}

	/* test for min and max, icon sliders, etc */
	switch (but->type) {
		case UI_BTYPE_NUM:
		case UI_BTYPE_SCROLL:
		case UI_BTYPE_NUM_SLIDER:
			UI_GET_BUT_VALUE_INIT(but, value);
			if      (value < (double)but->hardmin) ui_but_value_set(but, but->hardmin);
			else if (value > (double)but->hardmax) ui_but_value_set(but, but->hardmax);

			/* max must never be smaller than min! Both being equal is allowed though */
			BLI_assert(but->softmin <= but->softmax &&
			           but->hardmin <= but->hardmax);
			break;
			
		case UI_BTYPE_ICON_TOGGLE:
		case UI_BTYPE_ICON_TOGGLE_N:
			if (!but->rnaprop || (RNA_property_flag(but->rnaprop) & PROP_ICONS_CONSECUTIVE)) {
				if (but->flag & UI_SELECT) but->iconadd = 1;
				else but->iconadd = 0;
			}
			break;
			
			/* quiet warnings for unhandled types */
		default:
			break;
	}
	
	
	/* safety is 4 to enable small number buttons (like 'users') */
	// okwidth = -4 + (BLI_rcti_size_x(&but->rect)); // UNUSED
	
	/* name: */
	switch (but->type) {

		case UI_BTYPE_MENU:
			if (BLI_rctf_size_x(&but->rect) > 24.0f) {
				/* only needed for menus in popup blocks that don't recreate buttons on redraw */
				if (but->block->flag & UI_BLOCK_LOOP) {
					if (but->rnaprop && (RNA_property_type(but->rnaprop) == PROP_ENUM)) {
						int value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
						const char *buf;
						if (RNA_property_enum_name_gettexted(but->block->evil_C,
						                                     &but->rnapoin, but->rnaprop, value, &buf))
						{
							if (but->str == but->strdata) {
								if (strlen(buf) < sizeof(but->strdata)) {
									BLI_strncpy(but->str, buf, sizeof(but->strdata));
								}
								else {
									but->str = BLI_strdup(buf);
								}
							}
							else {
								MEM_SAFE_FREE(but->str);
								but->str = BLI_strdup(buf);
							}
						}
					}
				}
				BLI_strncpy(but->drawstr, but->str, sizeof(but->drawstr));
			}
			break;

		case UI_BTYPE_NUM:
		case UI_BTYPE_NUM_SLIDER:

			if (!but->editstr) {
				const char *drawstr_suffix = NULL;
				size_t slen;

				UI_GET_BUT_VALUE_INIT(but, value);

				slen = BLI_strncpy_rlen(but->drawstr, but->str, sizeof(but->drawstr));

				if (ui_but_is_float(but)) {
					if (value == (double) FLT_MAX) {
						slen += BLI_strncpy_rlen(but->drawstr + slen, "inf", sizeof(but->drawstr) - slen);
					}
					else if (value == (double) -FLT_MAX) {
						slen += BLI_strncpy_rlen(but->drawstr + slen, "-inf", sizeof(but->drawstr) - slen);
					}
					/* support length type buttons */
					else if (ui_but_is_unit(but)) {
						char new_str[sizeof(but->drawstr)];
						ui_get_but_string_unit(but, new_str, sizeof(new_str), value, true, -1);
						slen += BLI_strncpy_rlen(but->drawstr + slen, new_str, sizeof(but->drawstr) - slen);
					}
					else {
						const int prec = ui_but_calc_float_precision(but, value);
						slen += BLI_snprintf(but->drawstr + slen, sizeof(but->drawstr) - slen, "%.*f", prec, value);
					}
				}
				else {
					slen += BLI_snprintf(but->drawstr + slen, sizeof(but->drawstr) - slen, "%d", (int)value);
				}

				if (but->rnaprop) {
					PropertySubType pstype = RNA_property_subtype(but->rnaprop);

					if (pstype == PROP_PERCENTAGE) {
						drawstr_suffix = "%";
					}
					else if (pstype == PROP_PIXEL) {
						drawstr_suffix = " px";
					}
				}

				if (drawstr_suffix) {
					BLI_strncpy(but->drawstr + slen, drawstr_suffix, sizeof(but->drawstr) - slen);
				}

			}
			break;

		case UI_BTYPE_LABEL:
			if (ui_but_is_float(but)) {
				int prec;
				UI_GET_BUT_VALUE_INIT(but, value);
				prec = ui_but_calc_float_precision(but, value);
				BLI_snprintf(but->drawstr, sizeof(but->drawstr), "%s%.*f", but->str, prec, value);
			}
			else {
				BLI_strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);
			}
		
			break;

		case UI_BTYPE_TEXT:
		case UI_BTYPE_SEARCH_MENU:
		case UI_BTYPE_SEARCH_MENU_UNLINK:
			if (!but->editstr) {
				char str[UI_MAX_DRAW_STR];

				ui_but_string_get(but, str, UI_MAX_DRAW_STR);
				BLI_snprintf(but->drawstr, sizeof(but->drawstr), "%s%s", but->str, str);
			}
			break;
	
		case UI_BTYPE_KEY_EVENT:
		{
			const char *str;
			if (but->flag & UI_SELECT) {
				str = "Press a key";
			}
			else {
				UI_GET_BUT_VALUE_INIT(but, value);
				str = WM_key_event_string((short)value);
			}
			BLI_snprintf(but->drawstr, UI_MAX_DRAW_STR, "%s%s", but->str, str);
			break;
		}
		case UI_BTYPE_HOTKEY_EVENT:
			if (but->flag & UI_SELECT) {

				if (but->modifier_key) {
					char *str = but->drawstr;
					but->drawstr[0] = '\0';

					if (but->modifier_key & KM_SHIFT)
						str += BLI_strcpy_rlen(str, "Shift ");
					if (but->modifier_key & KM_CTRL)
						str += BLI_strcpy_rlen(str, "Ctrl ");
					if (but->modifier_key & KM_ALT)
						str += BLI_strcpy_rlen(str, "Alt ");
					if (but->modifier_key & KM_OSKEY)
						str += BLI_strcpy_rlen(str, "Cmd ");

					(void)str; /* UNUSED */
				}
				else {
					BLI_strncpy(but->drawstr, "Press a key", UI_MAX_DRAW_STR);
				}
			}
			else
				BLI_strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);

			break;

		case UI_BTYPE_HSVCUBE:
		case UI_BTYPE_HSVCIRCLE:
			break;
		default:
			BLI_strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);
			break;
		
	}

	/* if we are doing text editing, this will override the drawstr */
	if (but->editstr)
		but->drawstr[0] = '\0';
	
	/* text clipping moved to widget drawing code itself */
}


void UI_block_align_begin(uiBlock *block)
{
	/* if other align was active, end it */
	if (block->flag & UI_BUT_ALIGN) UI_block_align_end(block);

	block->flag |= UI_BUT_ALIGN_DOWN;
	block->alignnr++;

	/* buttons declared after this call will get this align nr */ // XXX flag?
}

static bool buts_are_horiz(uiBut *but1, uiBut *but2)
{
	float dx, dy;

	/* simple case which can fail if buttons shift apart
	 * with proportional layouts, see: [#38602] */
	if ((but1->rect.ymin == but2->rect.ymin) &&
	    (but1->rect.xmin != but2->rect.xmin))
	{
		return true;
	}

	dx = fabsf(but1->rect.xmax - but2->rect.xmin);
	dy = fabsf(but1->rect.ymin - but2->rect.ymax);
	
	return (dx <= dy);
}

void UI_block_align_end(uiBlock *block)
{
	block->flag &= ~UI_BUT_ALIGN;  /* all 4 flags */
}

bool ui_but_can_align(uiBut *but)
{
	return !ELEM(but->type, UI_BTYPE_LABEL, UI_BTYPE_CHECKBOX, UI_BTYPE_CHECKBOX_N, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE);
}

static void ui_block_align_calc_but(uiBut *first, short nr)
{
	uiBut *prev, *but = NULL, *next;
	int flag = 0, cols = 0, rows = 0;
	
	/* auto align */

	for (but = first; but && but->alignnr == nr; but = but->next) {
		if (but->next && but->next->alignnr == nr) {
			if (buts_are_horiz(but, but->next)) cols++;
			else rows++;
		}
	}

	/* rows == 0: 1 row, cols == 0: 1 column */
	
	/* note;  how it uses 'flag' in loop below (either set it, or OR it) is confusing */
	for (but = first, prev = NULL; but && but->alignnr == nr; prev = but, but = but->next) {
		next = but->next;
		if (next && next->alignnr != nr)
			next = NULL;

		/* clear old flag */
		but->drawflag &= ~UI_BUT_ALIGN;
			
		if (flag == 0) {  /* first case */
			if (next) {
				if (buts_are_horiz(but, next)) {
					if (rows == 0)
						flag = UI_BUT_ALIGN_RIGHT;
					else 
						flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT;
				}
				else {
					flag = UI_BUT_ALIGN_DOWN;
				}
			}
		}
		else if (next == NULL) {  /* last case */
			if (prev) {
				if (buts_are_horiz(prev, but)) {
					if (rows == 0)
						flag = UI_BUT_ALIGN_LEFT;
					else
						flag = UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT;
				}
				else {
					flag = UI_BUT_ALIGN_TOP;
				}
			}
		}
		else if (buts_are_horiz(but, next)) {
			/* check if this is already second row */
			if (prev && buts_are_horiz(prev, but) == 0) {
				flag &= ~UI_BUT_ALIGN_LEFT;
				flag |= UI_BUT_ALIGN_TOP;
				/* exception case: bottom row */
				if (rows > 0) {
					uiBut *bt = but;
					while (bt && bt->alignnr == nr) {
						if (bt->next && bt->next->alignnr == nr && buts_are_horiz(bt, bt->next) == 0) {
							break;
						}
						bt = bt->next;
					}
					if (bt == NULL || bt->alignnr != nr) flag = UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT;
				}
			}
			else {
				flag |= UI_BUT_ALIGN_LEFT;
			}
		}
		else {
			if (cols == 0) {
				flag |= UI_BUT_ALIGN_TOP;
			}
			else {  /* next button switches to new row */
				
				if (prev && buts_are_horiz(prev, but))
					flag |= UI_BUT_ALIGN_LEFT;
				else {
					flag &= ~UI_BUT_ALIGN_LEFT;
					flag |= UI_BUT_ALIGN_TOP;
				}
				
				if ((flag & UI_BUT_ALIGN_TOP) == 0) {  /* stil top row */
					if (prev) {
						if (next && buts_are_horiz(but, next))
							flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT | UI_BUT_ALIGN_RIGHT;
						else {
							/* last button in top row */
							flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT;
						}
					}
					else 
						flag |= UI_BUT_ALIGN_DOWN;
				}
				else 
					flag |= UI_BUT_ALIGN_TOP;
			}
		}
		
		but->drawflag |= flag;
		
		/* merge coordinates */
		if (prev) {
			/* simple cases */
			if (rows == 0) {
				but->rect.xmin = (prev->rect.xmax + but->rect.xmin) / 2.0f;
				prev->rect.xmax = but->rect.xmin;
			}
			else if (cols == 0) {
				but->rect.ymax = (prev->rect.ymin + but->rect.ymax) / 2.0f;
				prev->rect.ymin = but->rect.ymax;
			}
			else {
				if (buts_are_horiz(prev, but)) {
					but->rect.xmin = (prev->rect.xmax + but->rect.xmin) / 2.0f;
					prev->rect.xmax = but->rect.xmin;
					/* copy height too */
					but->rect.ymax = prev->rect.ymax;
				}
				else if (prev->prev && buts_are_horiz(prev->prev, prev) == 0) {
					/* the previous button is a single one in its row */
					but->rect.ymax = (prev->rect.ymin + but->rect.ymax) / 2.0f;
					prev->rect.ymin = but->rect.ymax;
					
					but->rect.xmin = prev->rect.xmin;
					if (next && buts_are_horiz(but, next) == 0)
						but->rect.xmax = prev->rect.xmax;
				}
				else {
					/* the previous button is not a single one in its row */
					but->rect.ymax = prev->rect.ymin;
				}
			}
		}
	}
}

void ui_block_align_calc(uiBlock *block)
{
	uiBut *but;
	short nr;

	/* align buttons with same align nr */
	for (but = block->buttons.first; but; ) {
		if (but->alignnr) {
			nr = but->alignnr;
			ui_block_align_calc_but(but, nr);

			/* skip with same number */
			for (; but && but->alignnr == nr; but = but->next) {
				/* pass */
			}

			if (!but) {
				break;
			}
		}
		else {
			but = but->next;
		}
	}
}

struct ColorManagedDisplay *ui_block_cm_display_get(uiBlock *block)
{
	return IMB_colormanagement_display_get_named(block->display_device);
}

void ui_block_cm_to_display_space_v3(uiBlock *block, float pixel[3])
{
	struct ColorManagedDisplay *display = ui_block_cm_display_get(block);

	IMB_colormanagement_scene_linear_to_display_v3(pixel, display);
}

void ui_block_cm_to_scene_linear_v3(uiBlock *block, float pixel[3])
{
	struct ColorManagedDisplay *display = ui_block_cm_display_get(block);

	IMB_colormanagement_display_to_scene_linear_v3(pixel, display);
}

/**
 * \brief ui_def_but is the function that draws many button types
 *
 * \param x,y The lower left hand corner of the button (X axis)
 * \param width,height The size of the button.
 *
 * for float buttons:
 * - \a a1 Click Step (how much to change the value each click)
 * - \a a2 Number of decimal point values to display. 0 defaults to 3 (0.000)
 *      1,2,3, and a maximum of 4, all greater values will be clamped to 4.
 */
static uiBut *ui_def_but(uiBlock *block, int type, int retval, const char *str,
                         int x, int y, short width, short height,
                         void *poin, float min, float max, float a1, float a2, const char *tip)
{
	uiBut *but;
	int slen;

	BLI_assert(width >= 0 && height >= 0);
	
	/* we could do some more error checks here */
	if ((type & BUTTYPE) == UI_BTYPE_LABEL) {
		BLI_assert((poin != NULL || min != 0.0f || max != 0.0f || (a1 == 0.0f && a2 != 0.0f) || (a1 != 0.0f && a1 != 1.0f)) == false);
	}

	if (type & UI_BUT_POIN_TYPES) {  /* a pointer is required */
		if (poin == NULL) {
			BLI_assert(0);
			return NULL;
		}
	}

	but = MEM_callocN(sizeof(uiBut), "uiBut");

	but->type = type & BUTTYPE;
	but->pointype = type & UI_BUT_POIN_TYPES;
	but->bit = type & UI_BUT_POIN_BIT;
	but->bitnr = type & 31;
	but->icon = ICON_NONE;
	but->iconadd = 0;

	but->retval = retval;

	slen = strlen(str);
	if (slen >= UI_MAX_NAME_STR) {
		but->str = MEM_mallocN(slen + 1, "ui_def_but str");
	}
	else {
		but->str = but->strdata;
	}
	memcpy(but->str, str, slen + 1);

	but->rect.xmin = x;
	but->rect.ymin = y;
	but->rect.xmax = but->rect.xmin + width;
	but->rect.ymax = but->rect.ymin + height;

	but->poin = poin;
	but->hardmin = but->softmin = min;
	but->hardmax = but->softmax = max;
	but->a1 = a1;
	but->a2 = a2;
	but->tip = tip;

	but->lock = block->lock;
	but->lockstr = block->lockstr;
	but->dt = block->dt;
	but->pie_dir = UI_RADIAL_NONE;

	but->block = block;  /* pointer back, used for frontbuffer status, and picker */

	if ((block->flag & UI_BUT_ALIGN) && ui_but_can_align(but))
		but->alignnr = block->alignnr;

	but->func = block->func;
	but->func_arg1 = block->func_arg1;
	but->func_arg2 = block->func_arg2;

	but->funcN = block->funcN;
	if (block->func_argN)
		but->func_argN = MEM_dupallocN(block->func_argN);
	
	but->pos = -1;   /* cursor invisible */

	if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) {    /* add a space to name */
		/* slen remains unchanged from previous assignment, ensure this stays true */
		if (slen > 0 && slen < UI_MAX_NAME_STR - 2) {
			if (but->str[slen - 1] != ' ') {
				but->str[slen] = ' ';
				but->str[slen + 1] = 0;
			}
		}
	}

	if (block->flag & UI_BLOCK_RADIAL) {
		but->drawflag |= (UI_BUT_TEXT_LEFT | UI_BUT_ICON_LEFT);
	}
	else if ((block->flag & UI_BLOCK_LOOP) ||
	         ELEM(but->type,
	              UI_BTYPE_MENU, UI_BTYPE_TEXT, UI_BTYPE_LABEL,
	              UI_BTYPE_BLOCK, UI_BTYPE_BUT_MENU, UI_BTYPE_SEARCH_MENU,
	              UI_BTYPE_PROGRESS_BAR, UI_BTYPE_SEARCH_MENU_UNLINK))
	{
		but->drawflag |= (UI_BUT_TEXT_LEFT | UI_BUT_ICON_LEFT);
	}
#ifdef USE_NUMBUTS_LR_ALIGN
	else if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) {
		but->drawflag |= UI_BUT_TEXT_LEFT;
	}
#endif

	but->drawflag |= (block->flag & UI_BUT_ALIGN);

	if (but->lock == true) {
		if (but->lockstr) {
			but->flag |= UI_BUT_DISABLED;
		}
	}

	/* keep track of UI_interface.h */
	if (ELEM(but->type,
	         UI_BTYPE_BLOCK, UI_BTYPE_BUT, UI_BTYPE_LABEL,
	         UI_BTYPE_PULLDOWN, UI_BTYPE_ROUNDBOX, UI_BTYPE_LISTBOX,
	         UI_BTYPE_BUT_MENU, UI_BTYPE_SCROLL, UI_BTYPE_GRIP,
	         UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE) ||
	    (but->type >= UI_BTYPE_SEARCH_MENU))
	{
		/* pass */
	}
	else {
		but->flag |= UI_BUT_UNDO;
	}

	BLI_addtail(&block->buttons, but);
	
	if (block->curlayout)
		ui_layout_add_but(block->curlayout, but);

#ifdef WITH_PYTHON
	/* if the 'UI_OT_editsource' is running, extract the source info from the button  */
	if (UI_editsource_enable_check()) {
		UI_editsource_active_but_test(but);
	}
#endif

	return but;
}

static void ui_def_but_rna__disable(uiBut *but)
{
	but->flag |= UI_BUT_DISABLED;
	but->lock = true;
	but->lockstr = "";
}

static void ui_def_but_rna__menu(bContext *UNUSED(C), uiLayout *layout, void *but_p)
{
	uiBlock *block = uiLayoutGetBlock(layout);
	uiPopupBlockHandle *handle = block->handle;
	uiBut *but = (uiBut *)but_p;

	/* see comment in ui_item_enum_expand, re: uiname  */
	EnumPropertyItem *item, *item_array;
	bool free;

	uiLayout *split, *column = NULL;

	int totitems = 0;
	int columns, rows, a, b;
	int column_start = 0, column_end = 0;
	int nbr_entries_nosepr = 0;

	UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);

	RNA_property_enum_items_gettexted(block->evil_C, &but->rnapoin, but->rnaprop, &item_array, NULL, &free);


	/* we dont want nested rows, cols in menus */
	UI_block_layout_set_current(block, layout);

	for (item = item_array; item->identifier; item++, totitems++) {
		if (!item->identifier[0]) {
			/* inconsistent, but menus with labels do not look good flipped */
			if (item->name) {
				block->flag |= UI_BLOCK_NO_FLIP;
				nbr_entries_nosepr++;
			}
			/* We do not want simple separators in nbr_entries_nosepr count */
			continue;
		}
		nbr_entries_nosepr++;
	}

	/* Columns and row estimation. Ignore simple separators here. */
	columns = (nbr_entries_nosepr + 20) / 20;
	if (columns < 1)
		columns = 1;
	if (columns > 8)
		columns = (nbr_entries_nosepr + 25) / 25;

	rows = totitems / columns;
	if (rows < 1)
		rows = 1;
	while (rows * columns < totitems)
		rows++;

	/* Title */
	uiDefBut(block, UI_BTYPE_LABEL, 0, RNA_property_ui_name(but->rnaprop),
	         0, 0, UI_UNIT_X * 5, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
	uiItemS(layout);

	/* note, item_array[...] is reversed on access */

	/* create items */
	split = uiLayoutSplit(layout, 0.0f, false);

	for (a = 0; a < totitems; a++) {
		if (a == column_end) {
			/* start new column, and find out where it ends in advance, so we
			 * can flip the order of items properly per column */
			column_start = a;
			column_end = totitems;

			for (b = a + 1; b < totitems; b++) {
				item = &item_array[b];

				/* new column on N rows or on separation label */
				if (((b - a) % rows == 0) || (!item->identifier[0] && item->name)) {
					column_end = b;
					break;
				}
			}

			column = uiLayoutColumn(split, false);
		}

		if (block->flag & UI_BLOCK_NO_FLIP) {
			item = &item_array[a];
		}
		else {
			item = &item_array[(column_start + column_end - 1 - a)];
		}

		if (!item->identifier[0]) {
			if (item->name) {
				if (item->icon) {
					uiItemL(column, item->name, item->icon);
				}
				else {
					/* Do not use uiItemL here, as our root layout is a menu one, it will add a fake blank icon! */
					uiDefBut(block, UI_BTYPE_LABEL, 0, item->name, 0, 0, UI_UNIT_X * 5, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				}
			}
			else {
				uiItemS(column);
			}
		}
		else {
			if (item->icon) {
				uiDefIconTextButI(block, UI_BTYPE_BUT_MENU, B_NOP, item->icon, item->name, 0, 0,
				                  UI_UNIT_X * 5, UI_UNIT_Y, &handle->retvalue, item->value, 0.0, 0, -1, item->description);
			}
			else {
				uiDefButI(block, UI_BTYPE_BUT_MENU, B_NOP, item->name, 0, 0,
				          UI_UNIT_X * 5, UI_UNIT_X, &handle->retvalue, item->value, 0.0, 0, -1, item->description);
			}
		}
	}

	UI_block_layout_set_current(block, layout);

	if (free) {
		MEM_freeN(item_array);
	}

	BLI_assert((block->flag & UI_BLOCK_IS_FLIP) == 0);
	block->flag |= UI_BLOCK_IS_FLIP;
}

/**
 * ui_def_but_rna_propname and ui_def_but_rna
 * both take the same args except for propname vs prop, this is done so we can
 * avoid an extra lookup on 'prop' when its already available.
 *
 * When this kind of change won't disrupt branches, best look into making more
 * of our UI functions take prop rather then propname.
 */
static uiBut *ui_def_but_rna(uiBlock *block, int type, int retval, const char *str,
                             int x, int y, short width, short height,
                             PointerRNA *ptr, PropertyRNA *prop, int index,
                             float min, float max, float a1, float a2,  const char *tip)
{
	const PropertyType proptype = RNA_property_type(prop);
	uiBut *but;
	int icon = 0;
	uiMenuCreateFunc func = NULL;

	if (ELEM(type, UI_BTYPE_COLOR, UI_BTYPE_HSVCIRCLE, UI_BTYPE_HSVCUBE)) {
		BLI_assert(index == -1);
	}

	/* use rna values if parameters are not specified */
	if ((proptype == PROP_ENUM) && ELEM(type, UI_BTYPE_MENU, UI_BTYPE_ROW, UI_BTYPE_LISTROW)) {
		/* UI_BTYPE_MENU is handled a little differently here */
		EnumPropertyItem *item;
		int value;
		bool free;
		int i;

		RNA_property_enum_items(block->evil_C, ptr, prop, &item, NULL, &free);

		if (type == UI_BTYPE_MENU) {
			value = RNA_property_enum_get(ptr, prop);
		}
		else {
			value = (int)max;
		}

		i = RNA_enum_from_value(item, value);
		if (i != -1) {

			if (!str) {
				str = item[i].name;
#ifdef WITH_INTERNATIONAL
				str = CTX_IFACE_(RNA_property_translation_context(prop), str);
#endif
			}

			icon = item[i].icon;
		}
		else {
			if (!str) {
				if (type == UI_BTYPE_MENU) {
					str = "";
				}
				else {
					str = RNA_property_ui_name(prop);
				}
			}
		}

		if (type == UI_BTYPE_MENU) {
			func = ui_def_but_rna__menu;
		}

		if (free) {
			MEM_freeN(item);
		}
	}
	else {
		if (!str) {
			str = RNA_property_ui_name(prop);
		}
		icon = RNA_property_ui_icon(prop);
	}

	if (!tip && proptype != PROP_ENUM)
		tip = RNA_property_ui_description(prop);

	if (min == max || a1 == -1 || a2 == -1) {
		if (proptype == PROP_INT) {
			int hardmin, hardmax, softmin, softmax, step;

			RNA_property_int_range(ptr, prop, &hardmin, &hardmax);
			RNA_property_int_ui_range(ptr, prop, &softmin, &softmax, &step);

			if (!ELEM(type, UI_BTYPE_ROW, UI_BTYPE_LISTROW) && min == max) {
				min = hardmin;
				max = hardmax;
			}
			if (a1 == -1)
				a1 = step;
			if (a2 == -1)
				a2 = 0;
		}
		else if (proptype == PROP_FLOAT) {
			float hardmin, hardmax, softmin, softmax, step, precision;

			RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
			RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);

			if (!ELEM(type, UI_BTYPE_ROW, UI_BTYPE_LISTROW) && min == max) {
				min = hardmin;
				max = hardmax;
			}
			if (a1 == -1)
				a1 = step;
			if (a2 == -1)
				a2 = precision;
		}
		else if (proptype == PROP_STRING) {
			min = 0;
			max = RNA_property_string_maxlength(prop);
			if (max == 0) /* interface code should ideally support unlimited length */
				max = UI_MAX_DRAW_STR;
		}
	}

	/* now create button */
	but = ui_def_but(block, type, retval, str, x, y, width, height, NULL, min, max, a1, a2, tip);

	but->rnapoin = *ptr;
	but->rnaprop = prop;

	if (RNA_property_array_check(but->rnaprop))
		but->rnaindex = index;
	else
		but->rnaindex = 0;

	if (icon) {
		but->icon = (BIFIconID)icon;
		but->flag |= UI_HAS_ICON;
		if (str[0]) {
			but->drawflag |= UI_BUT_ICON_LEFT;
		}
	}
	
	if ((type == UI_BTYPE_MENU) && (but->dt == UI_EMBOSS_PULLDOWN)) {
		but->flag |= UI_BUT_ICON_SUBMENU;
	}

	if (!RNA_property_editable(&but->rnapoin, prop)) {
		ui_def_but_rna__disable(but);
	}

	if (but->flag & UI_BUT_UNDO && (ui_but_is_rna_undo(but) == false)) {
		but->flag &= ~UI_BUT_UNDO;
	}

	/* If this button uses units, calculate the step from this */
	if ((proptype == PROP_FLOAT) && ui_but_is_unit(but)) {
		but->a1 = ui_get_but_step_unit(but, but->a1);
	}

	if (func) {
		but->menu_create_func = func;
		but->poin = (char *)but;
	}

	return but;
}

static uiBut *ui_def_but_rna_propname(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  const char *tip)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	uiBut *but;

	if (prop) {
		but = ui_def_but_rna(block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, a1, a2,  tip);
	}
	else {
		but = ui_def_but(block, type, retval, propname, x, y, width, height, NULL, min, max, a1, a2, tip);

		ui_def_but_rna__disable(but);
	}

	return but;
}

static uiBut *ui_def_but_operator_ptr(uiBlock *block, int type, wmOperatorType *ot, int opcontext, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but;

	if (!str) {
		if (ot && ot->srna)
			str = RNA_struct_ui_name(ot->srna);
		else
			str = "";
	}

	if ((!tip || tip[0] == '\0') && ot && ot->srna) {
		tip = RNA_struct_ui_description(ot->srna);
	}

	but = ui_def_but(block, type, -1, str, x, y, width, height, NULL, 0, 0, 0, 0, tip);
	but->optype = ot;
	but->opcontext = opcontext;
	but->flag &= ~UI_BUT_UNDO; /* no need for ui_but_is_rna_undo(), we never need undo here */

	if (!ot) {
		but->flag |= UI_BUT_DISABLED;
		but->lock = true;
		but->lockstr = "";
	}

	return but;
}

uiBut *uiDefBut(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2, const char *tip)
{
	uiBut *but = ui_def_but(block, type, retval, str, x, y, width, height, poin, min, max, a1, a2, tip);

	ui_but_update(but);
	
	return but;
}

/**
 * if \a _x_ is a power of two (only one bit) return the power,
 * otherwise return -1.
 *
 * for powers of two:
 * \code{.c}
 *     ((1 << findBitIndex(x)) == x);
 * \endcode
 */
static int findBitIndex(unsigned int x)
{
	if (!x || !is_power_of_2_i(x)) { /* is_power_of_2_i(x) strips lowest bit */
		return -1;
	}
	else {
		int idx = 0;

		if (x & 0xFFFF0000) idx += 16, x >>= 16;
		if (x & 0xFF00) idx += 8, x >>= 8;
		if (x & 0xF0) idx += 4, x >>= 4;
		if (x & 0xC) idx += 2, x >>= 2;
		if (x & 0x2) idx += 1;

		return idx;
	}
}

/* autocomplete helper functions */
struct AutoComplete {
	size_t maxlen;
	int matches;
	char *truncate;
	const char *startname;
};

AutoComplete *UI_autocomplete_begin(const char *startname, size_t maxlen)
{
	AutoComplete *autocpl;
	
	autocpl = MEM_callocN(sizeof(AutoComplete), "AutoComplete");
	autocpl->maxlen = maxlen;
	autocpl->matches = 0;
	autocpl->truncate = MEM_callocN(sizeof(char) * maxlen, "AutoCompleteTruncate");
	autocpl->startname = startname;

	return autocpl;
}

void UI_autocomplete_update_name(AutoComplete *autocpl, const char *name)
{
	char *truncate = autocpl->truncate;
	const char *startname = autocpl->startname;
	int a;

	for (a = 0; a < autocpl->maxlen - 1; a++) {
		if (startname[a] == 0 || startname[a] != name[a])
			break;
	}
	/* found a match */
	if (startname[a] == 0) {
		autocpl->matches++;
		/* first match */
		if (truncate[0] == 0)
			BLI_strncpy(truncate, name, autocpl->maxlen);
		else {
			/* remove from truncate what is not in bone->name */
			for (a = 0; a < autocpl->maxlen - 1; a++) {
				if (name[a] == 0) {
					truncate[a] = 0;
					break;
				}
				else if (truncate[a] != name[a])
					truncate[a] = 0;
			}
		}
	}
}

int UI_autocomplete_end(AutoComplete *autocpl, char *autoname)
{	
	int match = AUTOCOMPLETE_NO_MATCH;
	if (autocpl->truncate[0]) {
		if (autocpl->matches == 1) {
			match = AUTOCOMPLETE_FULL_MATCH;
		}
		else {
			match = AUTOCOMPLETE_PARTIAL_MATCH;
		}
		BLI_strncpy(autoname, autocpl->truncate, autocpl->maxlen);
	}
	else {
		if (autoname != autocpl->startname) {  /* don't copy a string over its self */
			BLI_strncpy(autoname, autocpl->startname, autocpl->maxlen);
		}
	}

	MEM_freeN(autocpl->truncate);
	MEM_freeN(autocpl);
	return match;
}

static void ui_but_update_and_icon_set(uiBut *but, int icon)
{
	if (icon) {
		but->icon = (BIFIconID) icon;
		but->flag |= UI_HAS_ICON;
	}

	ui_but_update(but);
}

static uiBut *uiDefButBit(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2,  const char *tip)
{
	int bitIdx = findBitIndex(bit);
	if (bitIdx == -1) {
		return NULL;
	}
	else {
		return uiDefBut(block, type | UI_BUT_POIN_BIT | bitIdx, retval, str, x, y, width, height, poin, min, max, a1, a2, tip);
	}
}
uiBut *uiDefButF(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefBut(block, type | UI_BUT_POIN_FLOAT, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitF(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefButBit(block, type | UI_BUT_POIN_FLOAT, bit, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButI(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefBut(block, type | UI_BUT_POIN_INT, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitI(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefButBit(block, type | UI_BUT_POIN_INT, bit, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButS(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefBut(block, type | UI_BUT_POIN_SHORT, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitS(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefButBit(block, type | UI_BUT_POIN_SHORT, bit, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButC(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefBut(block, type | UI_BUT_POIN_CHAR, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitC(uiBlock *block, int type, int bit, int retval, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefButBit(block, type | UI_BUT_POIN_CHAR, bit, retval, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButR(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna_propname(block, type, retval, str, x, y, width, height, ptr, propname, index, min, max, a1, a2, tip);
	ui_but_update(but);
	return but;
}
uiBut *uiDefButR_prop(uiBlock *block, int type, int retval, const char *str, int x, int y, short width, short height, PointerRNA *ptr, PropertyRNA *prop, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna(block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, a1, a2, tip);
	ui_but_update(but);
	return but;
}

uiBut *uiDefButO_ptr(uiBlock *block, int type, wmOperatorType *ot, int opcontext, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but;
	but = ui_def_but_operator_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
	ui_but_update(but);
	return but;
}
uiBut *uiDefButO(uiBlock *block, int type, const char *opname, int opcontext, const char *str, int x, int y, short width, short height, const char *tip)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);
	if (str == NULL && ot == NULL) str = opname;
	return uiDefButO_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
}

/* if a1==1.0 then a2 is an extra icon blending factor (alpha 0.0 - 1.0) */
uiBut *uiDefIconBut(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but = ui_def_but(block, type, retval, "", x, y, width, height, poin, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	return but;
}
static uiBut *uiDefIconButBit(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2, const char *tip)
{
	int bitIdx = findBitIndex(bit);
	if (bitIdx == -1) {
		return NULL;
	}
	else {
		return uiDefIconBut(block, type | UI_BUT_POIN_BIT | bitIdx, retval, icon, x, y, width, height, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconBut(block, type | UI_BUT_POIN_FLOAT, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitF(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconButBit(block, type | UI_BUT_POIN_FLOAT, bit, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconBut(block, type | UI_BUT_POIN_INT, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitI(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconButBit(block, type | UI_BUT_POIN_INT, bit, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconBut(block, type | UI_BUT_POIN_SHORT, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitS(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconButBit(block, type | UI_BUT_POIN_SHORT, bit, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconBut(block, type | UI_BUT_POIN_CHAR, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitC(uiBlock *block, int type, int bit, int retval, int icon, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconButBit(block, type | UI_BUT_POIN_CHAR, bit, retval, icon, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButR(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna_propname(block, type, retval, "", x, y, width, height, ptr, propname, index, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	return but;
}
uiBut *uiDefIconButR_prop(uiBlock *block, int type, int retval, int icon, int x, int y, short width, short height, PointerRNA *ptr, PropertyRNA *prop, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna(block, type, retval, "", x, y, width, height, ptr, prop, index, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	return but;
}

uiBut *uiDefIconButO_ptr(uiBlock *block, int type, wmOperatorType *ot, int opcontext, int icon, int x, int y, short width, short height, const char *tip)
{
	uiBut *but;
	but = ui_def_but_operator_ptr(block, type, ot, opcontext, "", x, y, width, height, tip);
	ui_but_update_and_icon_set(but, icon);
	return but;
}
uiBut *uiDefIconButO(uiBlock *block, int type, const char *opname, int opcontext, int icon, int x, int y, short width, short height, const char *tip)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);
	return uiDefIconButO_ptr(block, type, ot, opcontext, icon, x, y, width, height, tip);
}

/* Button containing both string label and icon */
uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but = ui_def_but(block, type, retval, str, x, y, width, height, poin, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	but->drawflag |= UI_BUT_ICON_LEFT;
	return but;
}
static uiBut *uiDefIconTextButBit(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, void *poin, float min, float max, float a1, float a2,  const char *tip)
{
	int bitIdx = findBitIndex(bit);
	if (bitIdx == -1) {
		return NULL;
	}
	else {
		return uiDefIconTextBut(block, type | UI_BUT_POIN_BIT | bitIdx, retval, icon, str, x, y, width, height, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextBut(block, type | UI_BUT_POIN_FLOAT, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, float *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextButBit(block, type | UI_BUT_POIN_FLOAT, bit, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextBut(block, type | UI_BUT_POIN_INT, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, int *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextButBit(block, type | UI_BUT_POIN_INT, bit, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextBut(block, type | UI_BUT_POIN_SHORT, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, short *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextButBit(block, type | UI_BUT_POIN_SHORT, bit, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextBut(block, type | UI_BUT_POIN_CHAR, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, const char *str, int x, int y, short width, short height, char *poin, float min, float max, float a1, float a2,  const char *tip)
{
	return uiDefIconTextButBit(block, type | UI_BUT_POIN_CHAR, bit, retval, icon, str, x, y, width, height, (void *) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButR(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna_propname(block, type, retval, str, x, y, width, height, ptr, propname, index, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	but->drawflag |= UI_BUT_ICON_LEFT;
	return but;
}
uiBut *uiDefIconTextButR_prop(uiBlock *block, int type, int retval, int icon, const char *str, int x, int y, short width, short height, PointerRNA *ptr, PropertyRNA *prop, int index, float min, float max, float a1, float a2,  const char *tip)
{
	uiBut *but;
	but = ui_def_but_rna(block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, a1, a2, tip);
	ui_but_update_and_icon_set(but, icon);
	but->drawflag |= UI_BUT_ICON_LEFT;
	return but;
}
uiBut *uiDefIconTextButO_ptr(uiBlock *block, int type, wmOperatorType *ot, int opcontext, int icon, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but;
	but = ui_def_but_operator_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
	ui_but_update_and_icon_set(but, icon);
	but->drawflag |= UI_BUT_ICON_LEFT;
	return but;
}
uiBut *uiDefIconTextButO(uiBlock *block, int type, const char *opname, int opcontext, int icon, const char *str, int x, int y, short width, short height, const char *tip)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);
	return uiDefIconTextButO_ptr(block, type, ot, opcontext, icon, str, x, y, width, height, tip);
}

/* END Button containing both string label and icon */

void UI_but_link_set(uiBut *but, void **poin, void ***ppoin, short *tot, int from, int to)
{
	uiLink *link;
	
	link = but->link = MEM_callocN(sizeof(uiLink), "new uilink");
	
	link->poin = poin;
	link->ppoin = ppoin;
	link->totlink = tot;
	link->fromcode = from;
	link->tocode = to;
}

/* cruft to make uiBlock and uiBut private */

int UI_blocklist_min_y_get(ListBase *lb)
{
	uiBlock *block;
	int min = 0;
	
	for (block = lb->first; block; block = block->next)
		if (block == lb->first || block->rect.ymin < min)
			min = block->rect.ymin;
			
	return min;
}

void UI_block_direction_set(uiBlock *block, char direction)
{
	block->direction = direction;
}

/* this call escapes if there's alignment flags */
void UI_block_order_flip(uiBlock *block)
{
	uiBut *but;
	float centy, miny = 10000, maxy = -10000;

	if (U.uiflag & USER_MENUFIXEDORDER)
		return;
	else if (block->flag & UI_BLOCK_NO_FLIP)
		return;
	
	for (but = block->buttons.first; but; but = but->next) {
		if (but->drawflag & UI_BUT_ALIGN) return;
		if (but->rect.ymin < miny) miny = but->rect.ymin;
		if (but->rect.ymax > maxy) maxy = but->rect.ymax;
	}
	/* mirror trick */
	centy = (miny + maxy) / 2.0f;
	for (but = block->buttons.first; but; but = but->next) {
		but->rect.ymin = centy - (but->rect.ymin - centy);
		but->rect.ymax = centy - (but->rect.ymax - centy);
		SWAP(float, but->rect.ymin, but->rect.ymax);
	}

	block->flag ^= UI_BLOCK_IS_FLIP;
}


void UI_block_flag_enable(uiBlock *block, int flag)
{
	block->flag |= flag;
}

void UI_block_flag_disable(uiBlock *block, int flag)
{
	block->flag &= ~flag;
}

void UI_but_flag_enable(uiBut *but, int flag)
{
	but->flag |= flag;
}

void UI_but_flag_disable(uiBut *but, int flag)
{
	but->flag &= ~flag;
}

void UI_but_drawflag_enable(uiBut *but, int flag)
{
	but->drawflag |= flag;
}

void UI_but_drawflag_disable(uiBut *but, int flag)
{
	but->drawflag &= ~flag;
}

void UI_but_type_set_menu_from_pulldown(uiBut *but)
{
	BLI_assert(but->type == UI_BTYPE_PULLDOWN);
	but->type = UI_BTYPE_MENU;
	UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
	UI_but_drawflag_enable(but, UI_BUT_TEXT_LEFT);
}

int UI_but_return_value_get(uiBut *but)
{
	return but->retval;
}

void UI_but_drag_set_id(uiBut *but, ID *id)
{
	but->dragtype = WM_DRAG_ID;
	but->dragpoin = (void *)id;
}

void UI_but_drag_set_rna(uiBut *but, PointerRNA *ptr)
{
	but->dragtype = WM_DRAG_RNA;
	but->dragpoin = (void *)ptr;
}

void UI_but_drag_set_path(uiBut *but, const char *path)
{
	but->dragtype = WM_DRAG_PATH;
	but->dragpoin = (void *)path;
}

void UI_but_drag_set_name(uiBut *but, const char *name)
{
	but->dragtype = WM_DRAG_NAME;
	but->dragpoin = (void *)name;
}

/* value from button itself */
void UI_but_drag_set_value(uiBut *but)
{
	but->dragtype = WM_DRAG_VALUE;
}

void UI_but_drag_set_image(uiBut *but, const char *path, int icon, struct ImBuf *imb, float scale)
{
	but->dragtype = WM_DRAG_PATH;
	but->icon = icon; /* no flag UI_HAS_ICON, so icon doesnt draw in button */
	but->dragpoin = (void *)path;
	but->imb = imb;
	but->imb_scale = scale;
}

PointerRNA *UI_but_operator_ptr_get(uiBut *but)
{
	if (but->optype && !but->opptr) {
		but->opptr = MEM_callocN(sizeof(PointerRNA), "uiButOpPtr");
		WM_operator_properties_create_ptr(but->opptr, but->optype);
	}

	return but->opptr;
}

void UI_but_unit_type_set(uiBut *but, const int unit_type)
{
	but->unit_type = (unsigned char)(RNA_SUBTYPE_UNIT_VALUE(unit_type));
}

int UI_but_unit_type_get(const uiBut *but)
{
	int ownUnit = (int)but->unit_type;
	
	/* own unit define always takes precedence over RNA provided, allowing for overriding 
	 * default value provided in RNA in a few special cases (i.e. Active Keyframe in Graph Edit)
	 */
	/* XXX: this doesn't allow clearing unit completely, though the same could be said for icons */
	if ((ownUnit != 0) || (but->rnaprop == NULL)) {
		return ownUnit << 16;
	}
	else {
		return RNA_SUBTYPE_UNIT(RNA_property_subtype(but->rnaprop));
	}
}

void UI_block_func_handle_set(uiBlock *block, uiBlockHandleFunc func, void *arg)
{
	block->handle_func = func;
	block->handle_func_arg = arg;
}

void UI_block_func_butmenu_set(uiBlock *block, uiMenuHandleFunc func, void *arg)
{
	block->butm_func = func;
	block->butm_func_arg = arg;
}

void UI_block_func_set(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2)
{
	block->func = func;
	block->func_arg1 = arg1;
	block->func_arg2 = arg2;
}

void UI_block_funcN_set(uiBlock *block, uiButHandleNFunc funcN, void *argN, void *arg2)
{
	if (block->func_argN) {
		MEM_freeN(block->func_argN);
	}

	block->funcN = funcN;
	block->func_argN = argN;
	block->func_arg2 = arg2;
}

void UI_but_func_rename_set(uiBut *but, uiButHandleRenameFunc func, void *arg1)
{
	but->rename_func = func;
	but->rename_arg1 = arg1;
}

void UI_but_func_drawextra_set(uiBlock *block, void (*func)(const bContext *C, void *idv, void *arg1, void *arg2, rcti *rect), void *arg1, void *arg2)
{
	block->drawextra = func;
	block->drawextra_arg1 = arg1;
	block->drawextra_arg2 = arg2;
}

void UI_but_func_set(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2)
{
	but->func = func;
	but->func_arg1 = arg1;
	but->func_arg2 = arg2;
}

void UI_but_funcN_set(uiBut *but, uiButHandleNFunc funcN, void *argN, void *arg2)
{
	if (but->func_argN) {
		MEM_freeN(but->func_argN);
	}

	but->funcN = funcN;
	but->func_argN = argN;
	but->func_arg2 = arg2;
}

void UI_but_func_complete_set(uiBut *but, uiButCompleteFunc func, void *arg)
{
	but->autocomplete_func = func;
	but->autofunc_arg = arg;
}

uiBut *uiDefBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_BLOCK, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func = func;
	ui_but_update(but);
	return but;
}

uiBut *uiDefBlockButN(uiBlock *block, uiBlockCreateFunc func, void *argN, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_BLOCK, 0, str, x, y, width, height, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func = func;
	if (but->func_argN) {
		MEM_freeN(but->func_argN);
	}
	but->func_argN = argN;
	ui_but_update(but);
	return but;
}


uiBut *uiDefPulldownBut(uiBlock *block, uiBlockCreateFunc func, void *arg, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_PULLDOWN, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func = func;
	ui_but_update(but);
	return but;
}

uiBut *uiDefMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_PULLDOWN, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->menu_create_func = func;
	ui_but_update(but);
	return but;
}

uiBut *uiDefIconTextMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_PULLDOWN, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);

	but->icon = (BIFIconID) icon;
	but->flag |= UI_HAS_ICON;

	but->drawflag |= UI_BUT_ICON_LEFT;
	but->flag |= UI_BUT_ICON_SUBMENU;

	but->menu_create_func = func;
	ui_but_update(but);

	return but;
}

uiBut *uiDefIconMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_PULLDOWN, 0, "", x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);

	but->icon = (BIFIconID) icon;
	but->flag |= UI_HAS_ICON;
	but->drawflag &= ~UI_BUT_ICON_LEFT;

	but->menu_create_func = func;
	ui_but_update(but);

	return but;
}

/* Block button containing both string label and icon */
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int icon, const char *str, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_BLOCK, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	/* XXX temp, old menu calls pass on icon arrow, which is now UI_BUT_ICON_SUBMENU flag */
	if (icon != ICON_RIGHTARROW_THIN) {
		but->icon = (BIFIconID) icon;
		but->drawflag |= UI_BUT_ICON_LEFT;
	}
	but->flag |= UI_HAS_ICON;
	but->flag |= UI_BUT_ICON_SUBMENU;

	but->block_create_func = func;
	ui_but_update(but);
	
	return but;
}

/* Block button containing icon */
uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int retval, int icon, int x, int y, short width, short height, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_BLOCK, retval, "", x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	but->icon = (BIFIconID) icon;
	but->flag |= UI_HAS_ICON;
	
	but->drawflag |= UI_BUT_ICON_LEFT;
	
	but->block_create_func = func;
	ui_but_update(but);
	
	return but;
}

uiBut *uiDefKeyevtButS(uiBlock *block, int retval, const char *str, int x, int y, short width, short height, short *spoin, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_KEY_EVENT | UI_BUT_POIN_SHORT, retval, str, x, y, width, height, spoin, 0.0, 0.0, 0.0, 0.0, tip);
	ui_but_update(but);
	return but;
}

/* short pointers hardcoded */
/* modkeypoin will be set to KM_SHIFT, KM_ALT, KM_CTRL, KM_OSKEY bits */
uiBut *uiDefHotKeyevtButS(uiBlock *block, int retval, const char *str, int x, int y, short width, short height, short *keypoin, short *modkeypoin, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_HOTKEY_EVENT | UI_BUT_POIN_SHORT, retval, str, x, y, width, height, keypoin, 0.0, 0.0, 0.0, 0.0, tip);
	but->modifier_key = *modkeypoin;
	ui_but_update(but);
	return but;
}


/* arg is pointer to string/name, use UI_but_func_search_set() below to make this work */
/* here a1 and a2, if set, control thumbnail preview rows/cols */
uiBut *uiDefSearchBut(uiBlock *block, void *arg, int retval, int icon, int maxlen, int x, int y, short width, short height, float a1, float a2, const char *tip)
{
	uiBut *but = ui_def_but(block, UI_BTYPE_SEARCH_MENU, retval, "", x, y, width, height, arg, 0.0, maxlen, a1, a2, tip);
	
	but->icon = (BIFIconID) icon;
	but->flag |= UI_HAS_ICON;
	
	but->drawflag |= UI_BUT_ICON_LEFT | UI_BUT_TEXT_LEFT;
	
	ui_but_update(but);
	
	return but;
}


/* arg is user value, searchfunc and handlefunc both get it as arg */
/* if active set, button opens with this item visible and selected */
void UI_but_func_search_set(uiBut *but, uiButSearchFunc sfunc, void *arg, uiButHandleFunc bfunc, void *active)
{
	but->search_func = sfunc;
	but->search_arg = arg;
	
	UI_but_func_set(but, bfunc, arg, active);
	
	/* search buttons show red-alert if item doesn't exist, not for menus */
	if (0 == (but->block->flag & UI_BLOCK_LOOP)) {
		/* skip empty buttons, not all buttons need input, we only show invalid */
		if (but->drawstr[0])
			ui_but_search_refresh(but);
	}
}

/* Callbacks for operator search button. */
static void operator_enum_search_cb(const struct bContext *C, void *but, const char *str, uiSearchItems *items)
{
	wmOperatorType *ot = ((uiBut *)but)->optype;
	PropertyRNA *prop = ot->prop;

	if (prop == NULL) {
		printf("%s: %s has no enum property set\n",
		       __func__, ot->idname);
	}
	else if (RNA_property_type(prop) != PROP_ENUM) {
		printf("%s: %s \"%s\" is not an enum property\n",
		       __func__, ot->idname, RNA_property_identifier(prop));
	}
	else {
		PointerRNA *ptr = UI_but_operator_ptr_get(but);  /* Will create it if needed! */
		EnumPropertyItem *item, *item_array;
		bool do_free;

		RNA_property_enum_items((bContext *)C, ptr, prop, &item_array, NULL, &do_free);

		for (item = item_array; item->identifier; item++) {
			/* note: need to give the index rather than the identifier because the enum can be freed */
			if (BLI_strcasestr(item->name, str)) {
				if (false == UI_search_item_add(items, item->name, SET_INT_IN_POINTER(item->value), 0))
					break;
			}
		}

		if (do_free)
			MEM_freeN(item_array);
	}
}

static void operator_enum_call_cb(struct bContext *UNUSED(C), void *but, void *arg2)
{
	wmOperatorType *ot = ((uiBut *)but)->optype;
	PointerRNA *opptr = UI_but_operator_ptr_get(but);  /* Will create it if needed! */

	if (ot) {
		if (ot->prop) {
			RNA_property_enum_set(opptr, ot->prop, GET_INT_FROM_POINTER(arg2));
			/* We do not call op from here, will be called by button code.
			 * ui_apply_but_funcs_after() (in interface_handlers.c) called this func before checking operators,
			 * because one of its parameters is the button itself!
			 */
		}
		else {
			printf("%s: op->prop for '%s' is NULL\n", __func__, ot->idname);
		}
	}
}

/* Same parameters as for uiDefSearchBut, with additional operator type and properties, used by callback
 * to call again the right op with the right options (properties values). */
uiBut *uiDefSearchButO_ptr(uiBlock *block, wmOperatorType *ot, IDProperty *properties,
                           void *arg, int retval, int icon, int maxlen, int x, int y,
                           short width, short height, float a1, float a2, const char *tip)
{
	uiBut *but;

	but = uiDefSearchBut(block, arg, retval, icon, maxlen, x, y, width, height, a1, a2, tip);
	UI_but_func_search_set(but, operator_enum_search_cb, but, operator_enum_call_cb, NULL);

	but->optype = ot;
	but->opcontext = WM_OP_EXEC_DEFAULT;

	if (properties) {
		PointerRNA *ptr = UI_but_operator_ptr_get(but);
		/* Copy idproperties. */
		ptr->data = IDP_CopyProperty(properties);
	}

	return but;
}

/* push a new event onto event queue to activate the given button 
 * (usually a text-field) upon entering a popup
 */
void UI_but_focus_on_enter_event(wmWindow *win, uiBut *but)
{
	wmEvent event;
	
	wm_event_init_from_window(win, &event);

	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = false;
	
	wm_event_add(win, &event);
}

void UI_but_string_info_get(bContext *C, uiBut *but, ...)
{
	va_list args;
	uiStringInfo *si;

	EnumPropertyItem *items = NULL, *item = NULL;
	int totitems;
	bool free_items = false;

	va_start(args, but);
	while ((si = (uiStringInfo *) va_arg(args, void *))) {
		int type = si->type;
		char *tmp = NULL;

		if (type == BUT_GET_LABEL) {
			if (but->str) {
				tmp = BLI_strdup(but->str);
			}
			else {
				type = BUT_GET_RNA_LABEL;  /* Fail-safe solution... */
			}
		}
		else if (type == BUT_GET_TIP) {
			if (but->tip && but->tip[0])
				tmp = BLI_strdup(but->tip);
			else
				type = BUT_GET_RNA_TIP;  /* Fail-safe solution... */
		}

		if (type == BUT_GET_RNAPROP_IDENTIFIER) {
			if (but->rnaprop)
				tmp = BLI_strdup(RNA_property_identifier(but->rnaprop));
		}
		else if (type == BUT_GET_RNASTRUCT_IDENTIFIER) {
			if (but->rnaprop)
				tmp = BLI_strdup(RNA_struct_identifier(but->rnapoin.type));
			else if (but->optype)
				tmp = BLI_strdup(but->optype->idname);
			else if (ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN)) {
				MenuType *mt = UI_but_menutype_get(but);
				if (mt)
					tmp = BLI_strdup(mt->idname);
			}
		}
		else if (ELEM(type, BUT_GET_RNA_LABEL, BUT_GET_RNA_TIP)) {
			if (but->rnaprop) {
				if (type == BUT_GET_RNA_LABEL)
					tmp = BLI_strdup(RNA_property_ui_name(but->rnaprop));
				else {
					const char *t = RNA_property_ui_description(but->rnaprop);
					if (t && t[0])
						tmp = BLI_strdup(t);
				}
			}
			else if (but->optype) {
				if (type == BUT_GET_RNA_LABEL)
					tmp = BLI_strdup(RNA_struct_ui_name(but->optype->srna));
				else {
					const char *t = RNA_struct_ui_description(but->optype->srna);
					if (t && t[0])
						tmp = BLI_strdup(t);
				}
			}
			else if (ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN)) {
				MenuType *mt = UI_but_menutype_get(but);
				if (mt) {
					/* not all menus are from python */
					if (mt->ext.srna) {
						if (type == BUT_GET_RNA_LABEL)
							tmp = BLI_strdup(RNA_struct_ui_name(mt->ext.srna));
						else {
							const char *t = RNA_struct_ui_description(mt->ext.srna);
							if (t && t[0])
								tmp = BLI_strdup(t);
						}
					}
				}
			}
		}
		else if (type == BUT_GET_RNA_LABEL_CONTEXT) {
			const char *_tmp = BLF_I18NCONTEXT_DEFAULT;
			if (but->rnaprop)
				_tmp = RNA_property_translation_context(but->rnaprop);
			else if (but->optype)
				_tmp = RNA_struct_translation_context(but->optype->srna);
			else if (ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN)) {
				MenuType *mt = UI_but_menutype_get(but);
				if (mt)
					_tmp = RNA_struct_translation_context(mt->ext.srna);
			}
			if (BLF_is_default_context(_tmp)) {
				_tmp = BLF_I18NCONTEXT_DEFAULT_BPYRNA;
			}
			tmp = BLI_strdup(_tmp);
		}
		else if (ELEM(type, BUT_GET_RNAENUM_IDENTIFIER, BUT_GET_RNAENUM_LABEL, BUT_GET_RNAENUM_TIP)) {
			PointerRNA *ptr = NULL;
			PropertyRNA *prop = NULL;
			int value = 0;
			
			/* get the enum property... */
			if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) {
				/* enum property */
				ptr = &but->rnapoin;
				prop = but->rnaprop;
				value = (but->type == UI_BTYPE_ROW) ? (int)but->hardmax : (int)ui_but_value_get(but);
			}
			else if (but->optype) {
				PointerRNA *opptr = UI_but_operator_ptr_get(but);
				wmOperatorType *ot = but->optype;
				
				/* if the default property of the operator is enum and it is set, 
				 * fetch the tooltip of the selected value so that "Snap" and "Mirror"
				 * operator menus in the Anim Editors will show tooltips for the different
				 * operations instead of the meaningless generic operator tooltip
				 */
				if (ot->prop && RNA_property_type(ot->prop) == PROP_ENUM) {
					if (RNA_struct_contains_property(opptr, ot->prop)) {
						ptr = opptr;
						prop = ot->prop;
						value = RNA_property_enum_get(opptr, ot->prop);
					}
				}
			}
			
			/* get strings from matching enum item */
			if (ptr && prop) {
				if (!item) {
					int i;
					
					RNA_property_enum_items_gettexted(C, ptr, prop, &items, &totitems, &free_items);
					for (i = 0, item = items; i < totitems; i++, item++) {
						if (item->identifier[0] && item->value == value)
							break;
					}
				}
				if (item && item->identifier) {
					if (type == BUT_GET_RNAENUM_IDENTIFIER)
						tmp = BLI_strdup(item->identifier);
					else if (type == BUT_GET_RNAENUM_LABEL)
						tmp = BLI_strdup(item->name);
					else if (item->description && item->description[0])
						tmp = BLI_strdup(item->description);
				}
			}
		}
		else if (type == BUT_GET_OP_KEYMAP) {
			if (!ui_block_is_menu(but->block)) {
				char buf[128];
				if (ui_but_event_operator_string(C, but, buf, sizeof(buf))) {
					tmp = BLI_strdup(buf);
				}
			}
		}
		else if (type == BUT_GET_PROP_KEYMAP) {
			/* for properties that are bound to one of the context cycle, etc. keys... */
			char buf[128];
			if (ui_but_event_property_operator_string(C, but, buf, sizeof(buf))) {
				tmp = BLI_strdup(buf);
			}
		}

		si->strinfo = tmp;
	}
	va_end(args);

	if (free_items && items)
		MEM_freeN(items);
}

/* Program Init/Exit */

void UI_init(void)
{
	ui_resources_init();
}

/* after reading userdef file */
void UI_init_userdef(void)
{
	/* fix saved themes */
	init_userdef_do_versions();
	uiStyleInit();
}

void UI_reinit_font(void)
{
	uiStyleInit();
}

void UI_exit(void)
{
	ui_resources_free();
	ui_but_clipboard_free();
}

