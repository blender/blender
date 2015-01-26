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
 * Contributor(s): Blender Foundation, 2003-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_panel.c
 *  \ingroup edinterface
 */


/* a full doc with API notes can be found in bf-blender/trunk/blender/doc/guides/interface_API.txt */
 
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_view2d.h"
#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "interface_intern.h"

/*********************** defines and structs ************************/

#define ANIMATION_TIME      0.30
#define ANIMATION_INTERVAL  0.02

#define PNL_LAST_ADDED      1
#define PNL_ACTIVE          2
#define PNL_WAS_ACTIVE      4
#define PNL_ANIM_ALIGN      8
#define PNL_NEW_ADDED       16
#define PNL_FIRST           32

/* only show pin header button for pinned panels */
#define USE_PIN_HIDDEN

typedef enum uiHandlePanelState {
	PANEL_STATE_DRAG,
	PANEL_STATE_DRAG_SCALE,
	PANEL_STATE_WAIT_UNTAB,
	PANEL_STATE_ANIMATION,
	PANEL_STATE_EXIT
} uiHandlePanelState;

typedef struct uiHandlePanelData {
	uiHandlePanelState state;

	/* animation */
	wmTimer *animtimer;
	double starttime;

	/* dragging */
	int startx, starty;
	int startofsx, startofsy;
	int startsizex, startsizey;
} uiHandlePanelData;

static void panel_activate_state(const bContext *C, Panel *pa, uiHandlePanelState state);

/*********************** space specific code ************************/
/* temporary code to remove all sbuts stuff from panel code         */

static int panel_aligned(ScrArea *sa, ARegion *ar)
{
	if (sa->spacetype == SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts = sa->spacedata.first;
		return sbuts->align;
	}
	else if (sa->spacetype == SPACE_USERPREF && ar->regiontype == RGN_TYPE_WINDOW)
		return BUT_VERTICAL;
	else if (sa->spacetype == SPACE_FILE && ar->regiontype == RGN_TYPE_CHANNELS)
		return BUT_VERTICAL;
	else if (sa->spacetype == SPACE_IMAGE && ar->regiontype == RGN_TYPE_PREVIEW)
		return BUT_VERTICAL;
	else if (ELEM(ar->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS))
		return BUT_VERTICAL;
	
	return 0;
}

static int panels_re_align(ScrArea *sa, ARegion *ar, Panel **r_pa)
{
	Panel *pa;
	int active = 0;

	*r_pa = NULL;

	if (sa->spacetype == SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts = sa->spacedata.first;

		if (sbuts->align)
			if (sbuts->re_align || sbuts->mainbo != sbuts->mainb)
				return 1;
	}
	else if (sa->spacetype == SPACE_IMAGE && ar->regiontype == RGN_TYPE_PREVIEW)
		return 1;
	else if (sa->spacetype == SPACE_FILE && ar->regiontype == RGN_TYPE_CHANNELS)
		return 1;

	/* in case panel is added or disappears */
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if ((pa->runtime_flag & PNL_WAS_ACTIVE) && !(pa->runtime_flag & PNL_ACTIVE))
			return 1;
		if (!(pa->runtime_flag & PNL_WAS_ACTIVE) && (pa->runtime_flag & PNL_ACTIVE))
			return 1;
		if (pa->activedata)
			active = 1;
	}

	/* in case we need to do an animation (size changes) */
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if (pa->runtime_flag & PNL_ANIM_ALIGN) {
			if (!active)
				*r_pa = pa;
			return 1;
		}
	}
	
	return 0;
}

/****************************** panels ******************************/

static void panels_collapse_all(ScrArea *sa, ARegion *ar, const Panel *from_pa)
{
	const bool has_category_tabs = UI_panel_category_is_visible(ar);
	const char *category = has_category_tabs ? UI_panel_category_active_get(ar, false) : NULL;
	const int flag = ((panel_aligned(sa, ar) == BUT_HORIZONTAL) ? PNL_CLOSEDX : PNL_CLOSEDY);
	const PanelType *from_pt = from_pa->type;
	Panel *pa;

	for (pa = ar->panels.first; pa; pa = pa->next) {
		PanelType *pt = pa->type;

		/* close panels with headers in the same context */
		if (pt && from_pt && !(pt->flag & PNL_NO_HEADER)) {
			if (!pt->context[0] || !from_pt->context[0] || STREQ(pt->context, from_pt->context)) {
				if ((pa->flag & PNL_PIN) || !category || !pt->category[0] || STREQ(pt->category, category)) {
					pa->flag &= ~PNL_CLOSED;
					pa->flag |= flag;
				}
			}
		}
	}
}


static void ui_panel_copy_offset(Panel *pa, Panel *papar)
{
	/* with respect to sizes... papar is parent */

	pa->ofsx = papar->ofsx;
	pa->ofsy = papar->ofsy + papar->sizey - pa->sizey;
}


/* XXX Disabled paneltab handling for now. Old 2.4x feature, *DO NOT* confuse it with new tool tabs in 2.70. ;)
 *     See also T41704.
 */
/* #define UI_USE_PANELTAB */

Panel *UI_panel_find_by_type(ARegion *ar, PanelType *pt)
{
	Panel *pa;
	const char *idname = pt->idname;

#ifdef UI_USE_PANELTAB
	const char *tabname = pt->idname;
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if (STREQLEN(pa->panelname, idname, sizeof(pa->panelname))) {
			if (STREQLEN(pa->tabname, tabname, sizeof(pa->tabname))) {
				return pa;
			}
		}
	}
#else
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if (STREQLEN(pa->panelname, idname, sizeof(pa->panelname))) {
			return pa;
		}
	}
#endif

	return NULL;
}

/**
 * \note \a pa should be return value from #UI_panel_find_by_type and can be NULL.
 */
Panel *UI_panel_begin(ScrArea *sa, ARegion *ar, uiBlock *block, PanelType *pt, Panel *pa, bool *r_open)
{
	Panel *palast, *panext;
	const char *drawname = CTX_IFACE_(pt->translation_context, pt->label);
	const char *idname = pt->idname;
#ifdef UI_USE_PANELTAB
	const char *tabname = pt->idname;
	const char *hookname = NULL;
#endif
	const bool newpanel = (pa == NULL);
	int align = panel_aligned(sa, ar);

	if (!newpanel) {
		pa->type = pt;
	}
	else {
		/* new panel */
		pa = MEM_callocN(sizeof(Panel), "new panel");
		pa->type = pt;
		BLI_strncpy(pa->panelname, idname, sizeof(pa->panelname));

		if (pt->flag & PNL_DEFAULT_CLOSED) {
			if (align == BUT_VERTICAL)
				pa->flag |= PNL_CLOSEDY;
			else
				pa->flag |= PNL_CLOSEDX;
		}
	
		pa->ofsx = 0;
		pa->ofsy = 0;
		pa->sizex = 0;
		pa->sizey = 0;
		pa->runtime_flag |= PNL_NEW_ADDED;

		BLI_addtail(&ar->panels, pa);

#ifdef UI_USE_PANELTAB
		BLI_strncpy(pa->tabname, tabname, sizeof(pa->tabname));

		/* make new Panel tabbed? */
		if (hookname) {
			Panel *patab;
			for (patab = ar->panels.first; patab; patab = patab->next) {
				if ((patab->runtime_flag & PNL_ACTIVE) && patab->paneltab == NULL) {
					if (STREQLEN(hookname, patab->panelname, sizeof(patab->panelname))) {
						if (STREQLEN(tabname, patab->tabname, sizeof(patab->tabname))) {
							pa->paneltab = patab;
							ui_panel_copy_offset(pa, patab);
							break;
						}
					}
				}
			}
		}
#else
		BLI_strncpy(pa->tabname, idname, sizeof(pa->tabname));
#endif
	}

	/* Do not allow closed panels without headers! Else user could get "disappeared" UI! */
	if ((pt->flag & PNL_NO_HEADER) && (pa->flag & PNL_CLOSED)) {
		pa->flag &= ~PNL_CLOSED;
		/* Force update of panels' positions! */
		pa->sizex = 0;
		pa->sizey = 0;
	}

	BLI_strncpy(pa->drawname, drawname, sizeof(pa->drawname));

	/* if a new panel is added, we insert it right after the panel
	 * that was last added. this way new panels are inserted in the
	 * right place between versions */
	for (palast = ar->panels.first; palast; palast = palast->next)
		if (palast->runtime_flag & PNL_LAST_ADDED)
			break;
	
	if (newpanel) {
		pa->sortorder = (palast) ? palast->sortorder + 1 : 0;

		for (panext = ar->panels.first; panext; panext = panext->next)
			if (panext != pa && panext->sortorder >= pa->sortorder)
				panext->sortorder++;
	}

	if (palast)
		palast->runtime_flag &= ~PNL_LAST_ADDED;

	/* assign to block */
	block->panel = pa;
	pa->runtime_flag |= PNL_ACTIVE | PNL_LAST_ADDED;

	*r_open = false;

	if (pa->paneltab) return pa;
	if (pa->flag & PNL_CLOSED) return pa;

	*r_open = true;
	
	return pa;
}

void UI_panel_end(uiBlock *block, int width, int height)
{
	Panel *pa = block->panel;

	if (pa->runtime_flag & PNL_NEW_ADDED) {
		pa->runtime_flag &= ~PNL_NEW_ADDED;
		pa->sizex = width;
		pa->sizey = height;
	}
	else {
		/* check if we need to do an animation */
		if (!ELEM(width, 0, pa->sizex) || !ELEM(height, 0, pa->sizey)) {
			pa->runtime_flag |= PNL_ANIM_ALIGN;
			if (height != 0)
				pa->ofsy += pa->sizey - height;
		}

		/* update width/height if non-zero */
		if (width != 0)
			pa->sizex = width;
		if (height != 0)
			pa->sizey = height;
	}
}

static void ui_offset_panel_block(uiBlock *block)
{
	uiStyle *style = UI_style_get_dpi();
	uiBut *but;
	int ofsy;

	/* compute bounds and offset */
	ui_block_bounds_calc(block);

	ofsy = block->panel->sizey - style->panelspace;

	for (but = block->buttons.first; but; but = but->next) {
		but->rect.ymin += ofsy;
		but->rect.ymax += ofsy;
	}

	block->rect.xmax = block->panel->sizex;
	block->rect.ymax = block->panel->sizey;
	block->rect.xmin = block->rect.ymin = 0.0;
}

/**************************** drawing *******************************/

/* extern used by previewrender */
#if 0 /*UNUSED 2.5*/
static void uiPanelPush(uiBlock *block)
{
	glPushMatrix(); 

	if (block->panel)
		glTranslatef((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0);
}

static void uiPanelPop(uiBlock *UNUSED(block))
{
	glPopMatrix();
}
#endif

/* triangle 'icon' for panel header */
void UI_draw_icon_tri(float x, float y, char dir)
{
	float f3 = 0.15 * U.widget_unit;
	float f5 = 0.25 * U.widget_unit;
	float f7 = 0.35 * U.widget_unit;
	
	if (dir == 'h') {
		ui_draw_anti_tria(x - f3, y - f5, x - f3, y + f5, x + f7, y);
	}
	else if (dir == 't') {
		ui_draw_anti_tria(x - f5, y - f7, x + f5, y - f7, x, y + f3);
	}
	else { /* 'v' = vertical, down */
		ui_draw_anti_tria(x - f5, y + f3, x + f5, y + f3, x, y - f7);
	}
}

/* triangle 'icon' inside rect */
static void ui_draw_tria_rect(const rctf *rect, char dir)
{
	if (dir == 'h') {
		float half = 0.5f * BLI_rctf_size_y(rect);
		ui_draw_anti_tria(rect->xmin, rect->ymin, rect->xmin, rect->ymax, rect->xmax, rect->ymin + half);
	}
	else {
		float half = 0.5f * BLI_rctf_size_x(rect);
		ui_draw_anti_tria(rect->xmin, rect->ymax, rect->xmax, rect->ymax, rect->xmin + half, rect->ymin);
	}
}

static void ui_draw_anti_x(float x1, float y1, float x2, float y2)
{

	/* set antialias line */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	glLineWidth(2.0);
	
	fdrawline(x1, y1, x2, y2);
	fdrawline(x1, y2, x2, y1);
	
	glLineWidth(1.0);
	
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	
}

/* x 'icon' for panel header */
static void ui_draw_x_icon(float x, float y)
{

	ui_draw_anti_x(x, y, x + 9.375f, y + 9.375f);

}

#define PNL_ICON    UI_UNIT_X  /* could be UI_UNIT_Y too */

static void ui_draw_panel_scalewidget(const rcti *rect)
{
	float xmin, xmax, dx;
	float ymin, ymax, dy;
	
	xmin = rect->xmax - PNL_HEADER + 2;
	xmax = rect->xmax - 3;
	ymin = rect->ymin + 3;
	ymax = rect->ymin + PNL_HEADER - 2;
		
	dx = 0.5f * (xmax - xmin);
	dy = 0.5f * (ymax - ymin);
	
	glEnable(GL_BLEND);
	glColor4ub(255, 255, 255, 50);
	fdrawline(xmin, ymin, xmax, ymax);
	fdrawline(xmin + dx, ymin, xmax, ymax - dy);
	
	glColor4ub(0, 0, 0, 50);
	fdrawline(xmin, ymin + 1, xmax, ymax + 1);
	fdrawline(xmin + dx, ymin + 1, xmax, ymax - dy + 1);
	glDisable(GL_BLEND);
}
static void ui_draw_panel_dragwidget(const rctf *rect)
{
	unsigned char col_back[3], col_high[3], col_dark[3];
	const int col_tint = 84;

	const int px = (int)U.pixelsize;
	const int px_zoom = max_ii(iroundf(BLI_rctf_size_y(rect) / 22.0f), 1);

	const int box_margin = max_ii(iroundf((float)(px_zoom * 2.0f)), px);
	const int box_size = max_ii(iroundf((BLI_rctf_size_y(rect) / 8.0f) - px), px);

	const int x_min = rect->xmin;
	const int y_min = rect->ymin;
	const int y_ofs = max_ii(iroundf(BLI_rctf_size_y(rect) / 3.0f), px);
	const int x_ofs = y_ofs;
	int i_x, i_y;


	UI_GetThemeColor3ubv(UI_GetThemeValue(TH_PANEL_SHOW_HEADER) ? TH_PANEL_HEADER : TH_PANEL_BACK, col_back);
	UI_GetColorPtrShade3ubv(col_back, col_high,  col_tint);
	UI_GetColorPtrShade3ubv(col_back, col_dark, -col_tint);


	/* draw multiple boxes */
	for (i_x = 0; i_x < 4; i_x++) {
		for (i_y = 0; i_y < 2; i_y++) {
			const int x_co = (x_min + x_ofs) + (i_x * (box_size + box_margin));
			const int y_co = (y_min + y_ofs) + (i_y * (box_size + box_margin));

			glColor3ubv(col_dark);
			glRectf(x_co - box_size, y_co - px_zoom, x_co, (y_co + box_size) - px_zoom);
			glColor3ubv(col_high);
			glRectf(x_co - box_size, y_co, x_co, y_co + box_size);
		}
	}
}


static void ui_draw_aligned_panel_header(uiStyle *style, uiBlock *block, const rcti *rect, char dir)
{
	Panel *panel = block->panel;
	rcti hrect;
	int pnl_icons;
	const char *activename = panel->drawname[0] ? panel->drawname : panel->panelname;

	/* + 0.001f to avoid flirting with float inaccuracy */
	if (panel->control & UI_PNL_CLOSE)
		pnl_icons = (panel->labelofs + 2 * PNL_ICON + 5) / block->aspect + 0.001f;
	else
		pnl_icons = (panel->labelofs + PNL_ICON + 5) / block->aspect + 0.001f;
	
	/* active tab */
	/* draw text label */
	UI_ThemeColor(TH_TITLE);
	
	hrect = *rect;
	if (dir == 'h') {
		hrect.xmin = rect->xmin + pnl_icons;
		hrect.ymin += 2.0f / block->aspect;
		UI_fontstyle_draw(&style->paneltitle, &hrect, activename);
	}
	else {
		/* ignore 'pnl_icons', otherwise the text gets offset horizontally 
		 * + 0.001f to avoid flirting with float inaccuracy
		 */
		hrect.xmin = rect->xmin + (PNL_ICON + 5) / block->aspect + 0.001f;
		UI_fontstyle_draw_rotated(&style->paneltitle, &hrect, activename);
	}
}

/* panel integrated in buttonswindow, tool/property lists etc */
void ui_draw_aligned_panel(uiStyle *style, uiBlock *block, const rcti *rect, const bool show_pin)
{
	Panel *panel = block->panel;
	rcti headrect;
	rctf itemrect;
	int ofsx;

	if (panel->paneltab) return;
	if (panel->type && (panel->type->flag & PNL_NO_HEADER)) return;

	/* calculate header rect */
	/* + 0.001f to prevent flicker due to float inaccuracy */
	headrect = *rect;
	headrect.ymin = headrect.ymax;
	headrect.ymax = headrect.ymin + floor(PNL_HEADER / block->aspect + 0.001f);

	{
		float minx = rect->xmin;
		float maxx = rect->xmax;
		float y = headrect.ymax;

		glEnable(GL_BLEND);

		if (UI_GetThemeValue(TH_PANEL_SHOW_HEADER)) {
			/* draw with background color */
			UI_ThemeColor4(TH_PANEL_HEADER);
			glRectf(minx, headrect.ymin + 1, maxx, y);

			fdrawline(minx, y, maxx, y);
			fdrawline(minx, y, maxx, y);
		}
		else if (!(panel->runtime_flag & PNL_FIRST)) {
			/* draw embossed separator */
			minx += 5.0f / block->aspect;
			maxx -= 5.0f / block->aspect;

			glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
			fdrawline(minx, y, maxx, y);
			glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
			fdrawline(minx, y - 1, maxx, y - 1);
		}

		glDisable(GL_BLEND);
	}

	/* draw optional pin icon */

#ifdef USE_PIN_HIDDEN
	if (show_pin && (block->panel->flag & PNL_PIN))
#else
	if (show_pin)
#endif
	{
		glEnable(GL_BLEND);
		UI_icon_draw_aspect(headrect.xmax - ((PNL_ICON * 2.2f) / block->aspect), headrect.ymin + (5.0f / block->aspect),
		                    (panel->flag & PNL_PIN) ? ICON_PINNED : ICON_UNPINNED,
		                    (block->aspect / UI_DPI_FAC), 1.0f);
		glDisable(GL_BLEND);
	}

	/* horizontal title */
	if (!(panel->flag & PNL_CLOSEDX)) {
		ui_draw_aligned_panel_header(style, block, &headrect, 'h');

		/* itemrect smaller */
		itemrect.xmax = headrect.xmax - 5.0f / block->aspect;
		itemrect.xmin = itemrect.xmax - BLI_rcti_size_y(&headrect);
		itemrect.ymin = headrect.ymin;
		itemrect.ymax = headrect.ymax;

		BLI_rctf_scale(&itemrect, 0.7f);
		ui_draw_panel_dragwidget(&itemrect);
	}

	/* if the panel is minimized vertically:
	 * (------)
	 */
	if (panel->flag & PNL_CLOSEDY) {
	}
	else if (panel->flag & PNL_CLOSEDX) {
		/* draw vertical title */
		ui_draw_aligned_panel_header(style, block, &headrect, 'v');
	}
	/* an open panel */
	else {
		/* in some occasions, draw a border */
		if (panel->flag & PNL_SELECT) {
			if (panel->control & UI_PNL_SOLID) UI_draw_roundbox_corner_set(UI_CNR_ALL);
			else UI_draw_roundbox_corner_set(UI_CNR_NONE);

			UI_ThemeColorShade(TH_BACK, -120);
			UI_draw_roundbox_unfilled(0.5f + rect->xmin, 0.5f + rect->ymin, 0.5f + rect->xmax, 0.5f + headrect.ymax + 1, 8);
		}

		/* panel backdrop */
		if (UI_GetThemeValue(TH_PANEL_SHOW_BACK)) {
			/* draw with background color */
			glEnable(GL_BLEND);
			UI_ThemeColor4(TH_PANEL_BACK);
			glRecti(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
		}

		if (panel->control & UI_PNL_SCALE)
			ui_draw_panel_scalewidget(rect);
	}

	/* draw optional close icon */

	ofsx = 6;
	if (panel->control & UI_PNL_CLOSE) {
		UI_ThemeColor(TH_TITLE);
		ui_draw_x_icon(rect->xmin + 2 + ofsx, rect->ymax + 2);
		ofsx = 22;
	}

	/* draw collapse icon */
	UI_ThemeColor(TH_TITLE);

	/* itemrect smaller */
	itemrect.xmin = headrect.xmin + 5.0f / block->aspect;
	itemrect.xmax = itemrect.xmin + BLI_rcti_size_y(&headrect);
	itemrect.ymin = headrect.ymin;
	itemrect.ymax = headrect.ymax;

	BLI_rctf_scale(&itemrect, 0.35f);

	if (panel->flag & PNL_CLOSEDY)
		ui_draw_tria_rect(&itemrect, 'h');
	else if (panel->flag & PNL_CLOSEDX)
		ui_draw_tria_rect(&itemrect, 'h');
	else
		ui_draw_tria_rect(&itemrect, 'v');

	(void)ofsx;
}

/************************** panel alignment *************************/

static int get_panel_header(Panel *pa)
{
	if (pa->type && (pa->type->flag & PNL_NO_HEADER))
		return 0;

	return PNL_HEADER;
}

static int get_panel_size_y(Panel *pa)
{
	if (pa->type && (pa->type->flag & PNL_NO_HEADER))
		return pa->sizey;

	return PNL_HEADER + pa->sizey;
}

/* this function is needed because uiBlock and Panel itself don't
 * change sizey or location when closed */
static int get_panel_real_ofsy(Panel *pa)
{
	if (pa->flag & PNL_CLOSEDY) return pa->ofsy + pa->sizey;
	else if (pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDY)) return pa->ofsy + pa->sizey;
	else if (pa->paneltab) return pa->paneltab->ofsy;
	else return pa->ofsy;
}

static int get_panel_real_ofsx(Panel *pa)
{
	if (pa->flag & PNL_CLOSEDX) return pa->ofsx + get_panel_header(pa);
	else if (pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDX)) return pa->ofsx + get_panel_header(pa);
	else return pa->ofsx + pa->sizex;
}

typedef struct PanelSort {
	Panel *pa, *orig;
} PanelSort;

/* note about sorting;
 * the sortorder has a lower value for new panels being added.
 * however, that only works to insert a single panel, when more new panels get
 * added the coordinates of existing panels and the previously stored to-be-inserted
 * panels do not match for sorting */

static int find_leftmost_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1 = a1, *ps2 = a2;
	
	if (ps1->pa->ofsx > ps2->pa->ofsx) return 1;
	else if (ps1->pa->ofsx < ps2->pa->ofsx) return -1;
	else if (ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if (ps1->pa->sortorder < ps2->pa->sortorder) return -1;

	return 0;
}


static int find_highest_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1 = a1, *ps2 = a2;
	
	/* stick uppermost header-less panels to the top of the region -
	 * prevent them from being sorted (multiple header-less panels have to be sorted though) */
	if (ps1->pa->type->flag & PNL_NO_HEADER && ps2->pa->type->flag & PNL_NO_HEADER) {
		/* skip and check for ofs and sortorder below */
	}
	else if (ps1->pa->type->flag & PNL_NO_HEADER) return -1;
	else if (ps2->pa->type->flag & PNL_NO_HEADER) return 1;

	if (ps1->pa->ofsy + ps1->pa->sizey < ps2->pa->ofsy + ps2->pa->sizey) return 1;
	else if (ps1->pa->ofsy + ps1->pa->sizey > ps2->pa->ofsy + ps2->pa->sizey) return -1;
	else if (ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if (ps1->pa->sortorder < ps2->pa->sortorder) return -1;
	
	return 0;
}

static int compare_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1 = a1, *ps2 = a2;
	
	if (ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if (ps1->pa->sortorder < ps2->pa->sortorder) return -1;
	
	return 0;
}

/* this doesnt draw */
/* returns 1 when it did something */
static bool uiAlignPanelStep(ScrArea *sa, ARegion *ar, const float fac, const bool drag)
{
	Panel *pa;
	PanelSort *ps, *panelsort, *psnext;
	int a, tot = 0;
	bool done;
	int align = panel_aligned(sa, ar);
	bool has_category_tabs = UI_panel_category_is_visible(ar);
	
	/* count active, not tabbed panels */
	for (pa = ar->panels.first; pa; pa = pa->next)
		if ((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab == NULL)
			tot++;

	if (tot == 0) return 0;

	/* extra; change close direction? */
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if ((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab == NULL) {
			if ((pa->flag & PNL_CLOSEDX) && (align == BUT_VERTICAL))
				pa->flag ^= PNL_CLOSED;
			else if ((pa->flag & PNL_CLOSEDY) && (align == BUT_HORIZONTAL))
				pa->flag ^= PNL_CLOSED;
		}
	}

	/* sort panels */
	panelsort = MEM_callocN(tot * sizeof(PanelSort), "panelsort");
	
	ps = panelsort;
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if ((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab == NULL) {
			ps->pa = MEM_dupallocN(pa);
			ps->orig = pa;
			ps++;
		}
	}
	
	if (drag) {
		/* while we are dragging, we sort on location and update sortorder */
		if (align == BUT_VERTICAL)
			qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
		else
			qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);

		for (ps = panelsort, a = 0; a < tot; a++, ps++)
			ps->orig->sortorder = a;
	}
	else
		/* otherwise use sortorder */
		qsort(panelsort, tot, sizeof(PanelSort), compare_panel);
	
	/* no smart other default start loc! this keeps switching f5/f6/etc compatible */
	ps = panelsort;
	ps->pa->ofsx = 0;
	ps->pa->ofsy = -get_panel_size_y(ps->pa);

	if (has_category_tabs) {
		if (align == BUT_VERTICAL) {
			ps->pa->ofsx += UI_PANEL_CATEGORY_MARGIN_WIDTH;
		}
	}

	for (a = 0; a < tot - 1; a++, ps++) {
		psnext = ps + 1;

		if (align == BUT_VERTICAL) {
			psnext->pa->ofsx = ps->pa->ofsx;
			psnext->pa->ofsy = get_panel_real_ofsy(ps->pa) - get_panel_size_y(psnext->pa);
		}
		else {
			psnext->pa->ofsx = get_panel_real_ofsx(ps->pa);
			psnext->pa->ofsy = ps->pa->ofsy + get_panel_size_y(ps->pa) - get_panel_size_y(psnext->pa);
		}
	}
	
	/* we interpolate */
	done = false;
	ps = panelsort;
	for (a = 0; a < tot; a++, ps++) {
		if ((ps->pa->flag & PNL_SELECT) == 0) {
			if ((ps->orig->ofsx != ps->pa->ofsx) || (ps->orig->ofsy != ps->pa->ofsy)) {
				ps->orig->ofsx = iroundf(fac * (float)ps->pa->ofsx + (1.0f - fac) * (float)ps->orig->ofsx);
				ps->orig->ofsy = iroundf(fac * (float)ps->pa->ofsy + (1.0f - fac) * (float)ps->orig->ofsy);
				done = true;
			}
		}
	}

	/* copy locations to tabs */
	for (pa = ar->panels.first; pa; pa = pa->next)
		if (pa->paneltab && (pa->runtime_flag & PNL_ACTIVE))
			ui_panel_copy_offset(pa, pa->paneltab);

	/* free panelsort array */
	for (ps = panelsort, a = 0; a < tot; a++, ps++) {
		MEM_freeN(ps->pa);
	}
	MEM_freeN(panelsort);
	
	return done;
}

static void ui_panels_size(ScrArea *sa, ARegion *ar, int *x, int *y)
{
	Panel *pa;
	int align = panel_aligned(sa, ar);
	int sizex = 0;
	int sizey = 0;

	/* compute size taken up by panels, for setting in view2d */
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if (pa->runtime_flag & PNL_ACTIVE) {
			int pa_sizex, pa_sizey;

			if (align == BUT_VERTICAL) {
				pa_sizex = pa->ofsx + pa->sizex;
				pa_sizey = get_panel_real_ofsy(pa);
			}
			else {
				pa_sizex = get_panel_real_ofsx(pa) + pa->sizex;
				pa_sizey = pa->ofsy + get_panel_size_y(pa);
			}

			sizex = max_ii(sizex, pa_sizex);
			sizey = min_ii(sizey, pa_sizey);
		}
	}

	if (sizex == 0)
		sizex = UI_PANEL_WIDTH;
	if (sizey == 0)
		sizey = -UI_PANEL_WIDTH;
	
	*x = sizex;
	*y = sizey;
}

static void ui_do_animate(const bContext *C, Panel *panel)
{
	uiHandlePanelData *data = panel->activedata;
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	float fac;

	fac = (PIL_check_seconds_timer() - data->starttime) / ANIMATION_TIME;
	fac = min_ff(sqrtf(fac), 1.0f);

	/* for max 1 second, interpolate positions */
	if (uiAlignPanelStep(sa, ar, fac, false)) {
		ED_region_tag_redraw(ar);
	}
	else {
		fac = 1.0f;
	}

	if (fac >= 1.0f) {
		panel_activate_state(C, panel, PANEL_STATE_EXIT);
		return;
	}
}

void UI_panels_begin(const bContext *UNUSED(C), ARegion *ar)
{
	Panel *pa;

	/* set all panels as inactive, so that at the end we know
	 * which ones were used */
	for (pa = ar->panels.first; pa; pa = pa->next) {
		if (pa->runtime_flag & PNL_ACTIVE)
			pa->runtime_flag = PNL_WAS_ACTIVE;
		else
			pa->runtime_flag = 0;
	}
}

/* only draws blocks with panels */
void UI_panels_end(const bContext *C, ARegion *ar, int *x, int *y)
{
	ScrArea *sa = CTX_wm_area(C);
	uiBlock *block;
	Panel *panot, *panew, *patest, *pa, *firstpa;
	
	/* offset contents */
	for (block = ar->uiblocks.first; block; block = block->next)
		if (block->active && block->panel)
			ui_offset_panel_block(block);

	/* consistency; are panels not made, whilst they have tabs */
	for (panot = ar->panels.first; panot; panot = panot->next) {
		if ((panot->runtime_flag & PNL_ACTIVE) == 0) {  /* not made */

			for (panew = ar->panels.first; panew; panew = panew->next) {
				if ((panew->runtime_flag & PNL_ACTIVE)) {
					if (panew->paneltab == panot) {  /* panew is tab in notmade pa */
						break;
					}
				}
			}
			/* now panew can become the new parent, check all other tabs */
			if (panew) {
				for (patest = ar->panels.first; patest; patest = patest->next) {
					if (patest->paneltab == panot) {
						patest->paneltab = panew;
					}
				}
				panot->paneltab = panew;
				panew->paneltab = NULL;
				ED_region_tag_redraw(ar); /* the buttons panew were not made */
			}
		}
	}

	/* re-align, possibly with animation */
	if (panels_re_align(sa, ar, &pa)) {
		/* XXX code never gets here... PNL_ANIM_ALIGN flag is never set */
		if (pa)
			panel_activate_state(C, pa, PANEL_STATE_ANIMATION);
		else
			uiAlignPanelStep(sa, ar, 1.0, false);
	}

	/* tag first panel */
	firstpa = NULL;
	for (block = ar->uiblocks.first; block; block = block->next)
		if (block->active && block->panel)
			if (!firstpa || block->panel->sortorder < firstpa->sortorder)
				firstpa = block->panel;
	
	if (firstpa)
		firstpa->runtime_flag |= PNL_FIRST;
	
	/* compute size taken up by panel */
	ui_panels_size(sa, ar, x, y);
}

void UI_panels_draw(const bContext *C, ARegion *ar)
{
	uiBlock *block;

	UI_ThemeClearColor(TH_BACK);
	
	/* draw panels, selected on top */
	for (block = ar->uiblocks.first; block; block = block->next) {
		if (block->active && block->panel && !(block->panel->flag & PNL_SELECT)) {
			UI_block_draw(C, block);
		}
	}

	for (block = ar->uiblocks.first; block; block = block->next) {
		if (block->active && block->panel && (block->panel->flag & PNL_SELECT)) {
			UI_block_draw(C, block);
		}
	}
}

void UI_panels_scale(ARegion *ar, float new_width)
{
	uiBlock *block;
	uiBut *but;
	
	for (block = ar->uiblocks.first; block; block = block->next) {
		if (block->panel) {
			float fac = new_width / (float)block->panel->sizex;
			printf("scaled %f\n", fac);
			block->panel->sizex = new_width;
			
			for (but = block->buttons.first; but; but = but->next) {
				but->rect.xmin *= fac;
				but->rect.xmax *= fac;
			}
		}
	}
}

/* ------------ panel merging ---------------- */

static void check_panel_overlap(ARegion *ar, Panel *panel)
{
	Panel *pa;

	/* also called with (panel == NULL) for clear */
	
	for (pa = ar->panels.first; pa; pa = pa->next) {
		pa->flag &= ~PNL_OVERLAP;
		if (panel && (pa != panel)) {
			if (pa->paneltab == NULL && (pa->runtime_flag & PNL_ACTIVE)) {
				float safex = 0.2, safey = 0.2;
				
				if (pa->flag & PNL_CLOSEDX) safex = 0.05;
				else if (pa->flag & PNL_CLOSEDY) safey = 0.05;
				else if (panel->flag & PNL_CLOSEDX) safex = 0.05;
				else if (panel->flag & PNL_CLOSEDY) safey = 0.05;

				if (pa->ofsx > panel->ofsx - safex * panel->sizex)
					if (pa->ofsx + pa->sizex < panel->ofsx + (1.0f + safex) * panel->sizex)
						if (pa->ofsy > panel->ofsy - safey * panel->sizey)
							if (pa->ofsy + pa->sizey < panel->ofsy + (1.0f + safey) * panel->sizey)
								pa->flag |= PNL_OVERLAP;
			}
		}
	}
}

/************************ panel dragging ****************************/

static void ui_do_drag(const bContext *C, const wmEvent *event, Panel *panel)
{
	uiHandlePanelData *data = panel->activedata;
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	short align = panel_aligned(sa, ar), dx = 0, dy = 0;
	
	/* first clip for window, no dragging outside */
	if (!BLI_rcti_isect_pt_v(&ar->winrct, &event->x))
		return;

	dx = (event->x - data->startx) & ~(PNL_GRID - 1);
	dy = (event->y - data->starty) & ~(PNL_GRID - 1);

	dx *= (float)BLI_rctf_size_x(&ar->v2d.cur) / (float)BLI_rcti_size_x(&ar->winrct);
	dy *= (float)BLI_rctf_size_y(&ar->v2d.cur) / (float)BLI_rcti_size_y(&ar->winrct);
	
	if (data->state == PANEL_STATE_DRAG_SCALE) {
		panel->sizex = MAX2(data->startsizex + dx, UI_PANEL_MINX);
		
		if (data->startsizey - dy < UI_PANEL_MINY)
			dy = -UI_PANEL_MINY + data->startsizey;

		panel->sizey = data->startsizey - dy;
		panel->ofsy = data->startofsy + dy;
	}
	else {
		/* reset the panel snapping, to allow dragging away from snapped edges */
		panel->snap = PNL_SNAP_NONE;
		
		panel->ofsx = data->startofsx + dx;
		panel->ofsy = data->startofsy + dy;
		check_panel_overlap(ar, panel);
		
		if (align) uiAlignPanelStep(sa, ar, 0.2, true);
	}

	ED_region_tag_redraw(ar);
}

/******************* region level panel interaction *****************/


/* this function is supposed to call general window drawing too */
/* also it supposes a block has panel, and isn't a menu */
static void ui_handle_panel_header(const bContext *C, uiBlock *block, int mx, int my, int event, short ctrl, short shift)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Panel *pa;
#ifdef USE_PIN_HIDDEN
	const bool show_pin = UI_panel_category_is_visible(ar) && (block->panel->flag & PNL_PIN);
#else
	const bool show_pin = UI_panel_category_is_visible(ar);
#endif

	int align = panel_aligned(sa, ar), button = 0;

	rctf rect_drag, rect_pin;
	float rect_leftmost;


	/* drag and pin rect's */
	rect_drag = block->rect;
	rect_drag.xmin = block->rect.xmax - (PNL_ICON * 1.5f);
	rect_pin = rect_drag;
	if (show_pin) {
		BLI_rctf_translate(&rect_pin, -PNL_ICON, 0.0f);
	}
	rect_leftmost = rect_pin.xmin;

	/* mouse coordinates in panel space! */
	
	/* XXX weak code, currently it assumes layout style for location of widgets */
	
	/* check open/collapsed button */
	if (event == RETKEY)
		button = 1;
	else if (event == AKEY)
		button = 1;
	else if (ELEM(event, 0, RETKEY, LEFTMOUSE) && shift) {
		block->panel->flag ^= PNL_PIN;
		button = 2;
	}
	else if (block->panel->flag & PNL_CLOSEDX) {
		if (my >= block->rect.ymax) button = 1;
	}
	else if (block->panel->control & UI_PNL_CLOSE) {
		/* whole of header can be used to collapse panel (except top-right corner) */
		if (mx <= block->rect.xmax - 8 - PNL_ICON) button = 2;
		//else if (mx <= block->rect.xmin + 10 + 2 * PNL_ICON + 2) button = 1;
	}
	else if (mx < rect_leftmost) {
		button = 1;
	}
	
	if (button) {
		if (button == 2) {  /* close */
			ED_region_tag_redraw(ar);
		}
		else {  /* collapse */
			if (ctrl)
				panels_collapse_all(sa, ar, block->panel);

			if (block->panel->flag & PNL_CLOSED) {
				block->panel->flag &= ~PNL_CLOSED;
				/* snap back up so full panel aligns with screen edge */
				if (block->panel->snap & PNL_SNAP_BOTTOM) 
					block->panel->ofsy = 0;
			}
			else if (align == BUT_HORIZONTAL) {
				block->panel->flag |= PNL_CLOSEDX;
			}
			else {
				/* snap down to bottom screen edge*/
				block->panel->flag |= PNL_CLOSEDY;
				if (block->panel->snap & PNL_SNAP_BOTTOM) 
					block->panel->ofsy = -block->panel->sizey;
			}
			
			for (pa = ar->panels.first; pa; pa = pa->next) {
				if (pa->paneltab == block->panel) {
					if (block->panel->flag & PNL_CLOSED) pa->flag |= PNL_CLOSED;
					else pa->flag &= ~PNL_CLOSED;
				}
			}
		}

		if (align)
			panel_activate_state(C, block->panel, PANEL_STATE_ANIMATION);
		else
			ED_region_tag_redraw(ar);
	}
	else if (BLI_rctf_isect_x(&rect_drag, mx)) {
		panel_activate_state(C, block->panel, PANEL_STATE_DRAG);
	}
	else if (show_pin && BLI_rctf_isect_x(&rect_pin, mx)) {
		block->panel->flag ^= PNL_PIN;
		ED_region_tag_redraw(ar);
	}
}

bool UI_panel_category_is_visible(ARegion *ar)
{
	/* more than one */
	return ar->panels_category.first && ar->panels_category.first != ar->panels_category.last;
}

PanelCategoryDyn *UI_panel_category_find(ARegion *ar, const char *idname)
{
	return BLI_findstring(&ar->panels_category, idname, offsetof(PanelCategoryDyn, idname));
}

PanelCategoryStack *UI_panel_category_active_find(ARegion *ar, const char *idname)
{
	return BLI_findstring(&ar->panels_category_active, idname, offsetof(PanelCategoryStack, idname));
}

const char *UI_panel_category_active_get(ARegion *ar, bool set_fallback)
{
	PanelCategoryStack *pc_act;

	for (pc_act = ar->panels_category_active.first; pc_act; pc_act = pc_act->next) {
		if (UI_panel_category_find(ar, pc_act->idname)) {
			return pc_act->idname;
		}
	}

	if (set_fallback) {
		PanelCategoryDyn *pc_dyn = ar->panels_category.first;
		if (pc_dyn) {
			UI_panel_category_active_set(ar, pc_dyn->idname);
			return pc_dyn->idname;
		}
	}

	return NULL;
}

void UI_panel_category_active_set(ARegion *ar, const char *idname)
{
	ListBase *lb = &ar->panels_category_active;
	PanelCategoryStack *pc_act = UI_panel_category_active_find(ar, idname);

	if (pc_act) {
		BLI_remlink(lb, pc_act);
	}
	else {
		pc_act = MEM_callocN(sizeof(PanelCategoryStack), __func__);
		BLI_strncpy(pc_act->idname, idname, sizeof(pc_act->idname));
	}

	BLI_addhead(lb, pc_act);


	/* validate all active panels, we could do this on load,
	 * they are harmless - but we should remove somewhere.
	 * (addons could define own and gather cruft over time) */
	{
		PanelCategoryStack *pc_act_next;
		/* intentionally skip first */
		pc_act_next = pc_act->next;
		while ((pc_act = pc_act_next)) {
			pc_act_next = pc_act->next;
			if (!BLI_findstring(&ar->type->paneltypes, pc_act->idname, offsetof(PanelType, category))) {
				BLI_remlink(lb, pc_act);
			}
		}
	}
}

PanelCategoryDyn *UI_panel_category_find_mouse_over_ex(ARegion *ar, const int x, const int y)
{
	PanelCategoryDyn *ptd;

	for (ptd = ar->panels_category.first; ptd; ptd = ptd->next) {
		if (BLI_rcti_isect_pt(&ptd->rect, x, y)) {
			return ptd;
		}
	}

	return NULL;
}

PanelCategoryDyn *UI_panel_category_find_mouse_over(ARegion *ar, const wmEvent *event)
{
	return UI_panel_category_find_mouse_over_ex(ar, event->mval[0], event->mval[1]);
}


void UI_panel_category_add(ARegion *ar, const char *name)
{
	PanelCategoryDyn *pc_dyn = MEM_callocN(sizeof(*pc_dyn), __func__);
	BLI_addtail(&ar->panels_category, pc_dyn);

	BLI_strncpy(pc_dyn->idname, name, sizeof(pc_dyn->idname));

	/* 'pc_dyn->rect' must be set on draw */
}

void UI_panel_category_clear_all(ARegion *ar)
{
	BLI_freelistN(&ar->panels_category);
}

/* based on UI_draw_roundbox_gl_mode, check on making a version which allows us to skip some sides */
static void ui_panel_category_draw_tab(int mode, float minx, float miny, float maxx, float maxy, float rad,
                                       int roundboxtype,
                                       const bool use_highlight, const bool use_shadow,
                                       const unsigned char highlight_fade[3])
{
	float vec[4][2] = {
	    {0.195, 0.02},
	    {0.55, 0.169},
	    {0.831, 0.45},
	    {0.98, 0.805}};
	int a;

	/* mult */
	for (a = 0; a < 4; a++) {
		mul_v2_fl(vec[a], rad);
	}

	glBegin(mode);

	/* start with corner right-top */
	if (use_highlight) {
		if (roundboxtype & UI_CNR_TOP_RIGHT) {
			glVertex2f(maxx, maxy - rad);
			for (a = 0; a < 4; a++) {
				glVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
			}
			glVertex2f(maxx - rad, maxy);
		}
		else {
			glVertex2f(maxx, maxy);
		}

		/* corner left-top */
		if (roundboxtype & UI_CNR_TOP_LEFT) {
			glVertex2f(minx + rad, maxy);
			for (a = 0; a < 4; a++) {
				glVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
			}
			glVertex2f(minx, maxy - rad);
		}
		else {
			glVertex2f(minx, maxy);
		}
	}

	if (use_highlight && !use_shadow) {
		if (highlight_fade) {
			glColor3ubv(highlight_fade);
		}
		glVertex2f(minx, miny + rad);
		glEnd();
		return;
	}

	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		glVertex2f(minx, miny + rad);
		for (a = 0; a < 4; a++) {
			glVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		glVertex2f(minx + rad, miny);
	}
	else {
		glVertex2f(minx, miny);
	}

	/* corner right-bottom */

	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		glVertex2f(maxx - rad, miny);
		for (a = 0; a < 4; a++) {
			glVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		glVertex2f(maxx, miny + rad);
	}
	else {
		glVertex2f(maxx, miny);
	}

	glEnd();
}


/**
 * Draw vertical tabs on the left side of the region,
 * one tab per category.
 */
void UI_panel_category_draw_all(ARegion *ar, const char *category_id_active)
{
	/* no tab outlines for */
// #define USE_FLAT_INACTIVE
	View2D *v2d = &ar->v2d;
	uiStyle *style = UI_style_get();
	const uiFontStyle *fstyle = &style->widget;
	const int fontid = fstyle->uifont_id;
	short fstyle_points = fstyle->points;

	PanelCategoryDyn *pc_dyn;
	const float aspect = ((uiBlock *)ar->uiblocks.first)->aspect;
	const float zoom = 1.0f / aspect;
	const int px = max_ii(1, iroundf(U.pixelsize));
	const int category_tabs_width = iroundf(UI_PANEL_CATEGORY_MARGIN_WIDTH * zoom);
	const float dpi_fac = UI_DPI_FAC;
	const int tab_v_pad_text = iroundf((2 + ((px * 3) * dpi_fac)) * zoom);  /* pading of tabs around text */
	const int tab_v_pad = iroundf((4 + (2 * px * dpi_fac)) * zoom);  /* padding between tabs */
	const float tab_curve_radius = ((px * 3) * dpi_fac) * zoom;
	const int roundboxtype = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
	bool is_alpha;
	bool do_scaletabs = false;
#ifdef USE_FLAT_INACTIVE
	bool is_active_prev = false;
#endif
	float scaletabs = 1.0f;
	/* same for all tabs */
	const int rct_xmin = v2d->mask.xmin + 3;  /* intentionally dont scale by 'px' */
	const int rct_xmax = v2d->mask.xmin + category_tabs_width;
	const int text_v_ofs = (rct_xmax - rct_xmin) * 0.3f;

	int y_ofs = tab_v_pad;

	/* Primary theme colors */
	unsigned char theme_col_back[4];
	unsigned char theme_col_text[3];
	unsigned char theme_col_text_hi[3];

	/* Tab colors */
	unsigned char theme_col_tab_bg[4];
	unsigned char theme_col_tab_active[3];
	unsigned char theme_col_tab_inactive[3];

	/* Secondary theme colors */
	unsigned char theme_col_tab_outline[3];
	unsigned char theme_col_tab_divider[3];  /* line that divides tabs from the main area */
	unsigned char theme_col_tab_highlight[3];
	unsigned char theme_col_tab_highlight_inactive[3];



	UI_GetThemeColor4ubv(TH_BACK, theme_col_back);
	UI_GetThemeColor3ubv(TH_TEXT, theme_col_text);
	UI_GetThemeColor3ubv(TH_TEXT_HI, theme_col_text_hi);

	UI_GetThemeColor4ubv(TH_TAB_BACK, theme_col_tab_bg);
	UI_GetThemeColor3ubv(TH_TAB_ACTIVE, theme_col_tab_active);
	UI_GetThemeColor3ubv(TH_TAB_INACTIVE, theme_col_tab_inactive);
	UI_GetThemeColor3ubv(TH_TAB_OUTLINE, theme_col_tab_outline);

	interp_v3_v3v3_uchar(theme_col_tab_divider, theme_col_back, theme_col_tab_outline, 0.3f);
	interp_v3_v3v3_uchar(theme_col_tab_highlight, theme_col_back, theme_col_text_hi, 0.2f);
	interp_v3_v3v3_uchar(theme_col_tab_highlight_inactive, theme_col_tab_inactive, theme_col_text_hi, 0.12f);

	is_alpha = (ar->overlap && (theme_col_back[3] != 255));

	if (fstyle->kerning == 1) {
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

	BLF_enable(fontid, BLF_ROTATION);
	BLF_rotation(fontid, M_PI / 2);
	//UI_fontstyle_set(&style->widget);
	ui_fontscale(&fstyle_points, aspect / (U.pixelsize * 1.1f));
	BLF_size(fontid, fstyle_points, U.dpi);

	BLF_enable(fontid, BLF_SHADOW);
	BLF_shadow(fontid, 3, 1.0f, 1.0f, 1.0f, 0.25f);
	BLF_shadow_offset(fontid, -1, -1);

	BLI_assert(UI_panel_category_is_visible(ar));


	/* calculate tab rect's and check if we need to scale down */
	for (pc_dyn = ar->panels_category.first; pc_dyn; pc_dyn = pc_dyn->next) {
		rcti *rct = &pc_dyn->rect;
		const char *category_id = pc_dyn->idname;
		const char *category_id_draw = IFACE_(category_id);
		const int category_width = BLF_width(fontid, category_id_draw, BLF_DRAW_STR_DUMMY_MAX);

		rct->xmin = rct_xmin;
		rct->xmax = rct_xmax;

		rct->ymin = v2d->mask.ymax - (y_ofs + category_width + (tab_v_pad_text * 2));
		rct->ymax = v2d->mask.ymax - (y_ofs);

		y_ofs += category_width + tab_v_pad + (tab_v_pad_text * 2);
	}

	if (y_ofs > BLI_rcti_size_y(&v2d->mask)) {
		scaletabs = (float)BLI_rcti_size_y(&v2d->mask) / (float)y_ofs;

		for (pc_dyn = ar->panels_category.first; pc_dyn; pc_dyn = pc_dyn->next) {
			rcti *rct = &pc_dyn->rect;
			rct->ymin = ((rct->ymin - v2d->mask.ymax) * scaletabs) + v2d->mask.ymax;
			rct->ymax = ((rct->ymax - v2d->mask.ymax) * scaletabs) + v2d->mask.ymax;
		}

		do_scaletabs = true;
	}


	/* begin drawing */
	glEnable(GL_LINE_SMOOTH);

	/* draw the background */
	if (is_alpha) {
		glEnable(GL_BLEND);
		glColor4ubv(theme_col_tab_bg);
	}
	else {
		glColor3ubv(theme_col_tab_bg);
	}

	glRecti(v2d->mask.xmin, v2d->mask.ymin, v2d->mask.xmin + category_tabs_width, v2d->mask.ymax);

	if (is_alpha) {
		glDisable(GL_BLEND);
	}

	for (pc_dyn = ar->panels_category.first; pc_dyn; pc_dyn = pc_dyn->next) {
		const rcti *rct = &pc_dyn->rect;
		const char *category_id = pc_dyn->idname;
		const char *category_id_draw = IFACE_(category_id);
		int category_width = BLI_rcti_size_y(rct) - (tab_v_pad_text * 2);
		size_t category_draw_len = BLF_DRAW_STR_DUMMY_MAX;
		// int category_width = BLF_width(fontid, category_id_draw, BLF_DRAW_STR_DUMMY_MAX);

		const bool is_active = STREQ(category_id, category_id_active);

#ifdef DEBUG
		if (STREQ(category_id, PNL_CATEGORY_FALLBACK)) {
			printf("WARNING: Panel has no 'bl_category', script needs updating!\n");
		}
#endif

		glEnable(GL_BLEND);

#ifdef USE_FLAT_INACTIVE
		if (is_active)
#endif
		{
			glColor3ubv(is_active ? theme_col_tab_active : theme_col_tab_inactive);
			ui_panel_category_draw_tab(GL_POLYGON, rct->xmin, rct->ymin, rct->xmax, rct->ymax,
			                           tab_curve_radius - px, roundboxtype, true, true, NULL);

			/* tab outline */
			glColor3ubv(theme_col_tab_outline);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			ui_panel_category_draw_tab(GL_LINE_STRIP, rct->xmin - px, rct->ymin - px, rct->xmax - px, rct->ymax + px,
			                           tab_curve_radius, roundboxtype, true, true, NULL);
			/* tab highlight (3d look) */
			glShadeModel(GL_SMOOTH);
			glColor3ubv(is_active ? theme_col_tab_highlight : theme_col_tab_highlight_inactive);
			ui_panel_category_draw_tab(GL_LINE_STRIP, rct->xmin, rct->ymin, rct->xmax, rct->ymax,
			                           tab_curve_radius, roundboxtype, true, false,
			                           is_active ? theme_col_back : theme_col_tab_inactive);
			glShadeModel(GL_FLAT);

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		/* tab blackline */
		if (!is_active) {
			glColor3ubv(theme_col_tab_divider);
			glRecti(v2d->mask.xmin + category_tabs_width - px,
			        rct->ymin - tab_v_pad,
			        v2d->mask.xmin + category_tabs_width,
			        rct->ymax + tab_v_pad);
		}

		if (do_scaletabs) {
			category_draw_len = BLF_width_to_strlen(fontid, category_id_draw, category_draw_len,
			                                        category_width, NULL);
		}

		BLF_position(fontid, rct->xmax - text_v_ofs, rct->ymin + tab_v_pad_text, 0.0f);

		/* tab titles */

		/* draw white shadow to give text more depth */
		glColor3ubv(theme_col_text);

		/* main tab title */
		BLF_draw(fontid, category_id_draw, category_draw_len);

		glDisable(GL_BLEND);

		/* tab blackline remaining (last tab) */
		if (pc_dyn->prev == NULL) {
			glColor3ubv(theme_col_tab_divider);
			glRecti(v2d->mask.xmin + category_tabs_width - px,
			        rct->ymax + px,
			        v2d->mask.xmin + category_tabs_width,
			        v2d->mask.ymax);
		}
		if (pc_dyn->next == NULL) {
			glColor3ubv(theme_col_tab_divider);
			glRecti(v2d->mask.xmin + category_tabs_width - px,
			        0,
			        v2d->mask.xmin + category_tabs_width,
			        rct->ymin);
		}

#ifdef USE_FLAT_INACTIVE
		/* draw line between inactive tabs */
		if (is_active == false && is_active_prev == false && pc_dyn->prev) {
			glColor3ubv(theme_col_tab_divider);
			glRecti(v2d->mask.xmin + (category_tabs_width / 5),
			        rct->ymax + px,
			        (v2d->mask.xmin + category_tabs_width) - (category_tabs_width / 5),
			        rct->ymax + (px * 3));
		}

		is_active_prev = is_active;
#endif

		/* not essential, but allows events to be handled right up until the region edge [#38171] */
		pc_dyn->rect.xmin = v2d->mask.xmin;
	}

	glDisable(GL_LINE_SMOOTH);

	BLF_disable(fontid, BLF_ROTATION);

	BLF_disable(fontid, BLF_SHADOW);

	if (fstyle->kerning == 1) {
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

#undef USE_FLAT_INACTIVE
}

/* XXX should become modal keymap */
/* AKey is opening/closing panels, independent of button state now */

int ui_handler_panel_region(bContext *C, const wmEvent *event, ARegion *ar)
{
	uiBlock *block;
	Panel *pa;
	int retval, mx, my;
	bool has_category_tabs = UI_panel_category_is_visible(ar);

	retval = WM_UI_HANDLER_CONTINUE;

	if (has_category_tabs) {
		if (event->val == KM_PRESS) {
			if (event->type == LEFTMOUSE) {
				PanelCategoryDyn *pc_dyn = UI_panel_category_find_mouse_over(ar, event);
				if (pc_dyn) {
					UI_panel_category_active_set(ar, pc_dyn->idname);
					ED_region_tag_redraw(ar);

					/* reset scroll to the top [#38348] */
					UI_view2d_offset(&ar->v2d, -1.0f, 1.0f);

					retval = WM_UI_HANDLER_BREAK;
				}
			}
			else if (ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
				/* mouse wheel cycle tabs */

				/* first check if the mouse is in the tab region */
				if (event->ctrl || (event->mval[0] < ((PanelCategoryDyn *)ar->panels_category.first)->rect.xmax)) {
					const char *category = UI_panel_category_active_get(ar, false);
					if (LIKELY(category)) {
						PanelCategoryDyn *pc_dyn = UI_panel_category_find(ar, category);
						if (LIKELY(pc_dyn)) {
							pc_dyn = (event->type == WHEELDOWNMOUSE) ? pc_dyn->next : pc_dyn->prev;
							if (pc_dyn) {
								/* intentionally don't reset scroll in this case,
								 * this allows for quick browsing between tabs */
								UI_panel_category_active_set(ar, pc_dyn->idname);
								ED_region_tag_redraw(ar);
							}
						}
					}
					retval = WM_UI_HANDLER_BREAK;
				}
			}
		}
	}

	if (retval == WM_UI_HANDLER_BREAK) {
		return retval;
	}

	for (block = ar->uiblocks.last; block; block = block->prev) {
		bool inside = false, inside_header = false, inside_scale = false;
		
		mx = event->x;
		my = event->y;
		ui_window_to_block(ar, block, &mx, &my);

		/* checks for mouse position inside */
		pa = block->panel;

		if (!pa || pa->paneltab != NULL)
			continue;
		if (pa->type && pa->type->flag & PNL_NO_HEADER)  /* XXX - accessed freed panels when scripts reload, need to fix. */
			continue;
		
		/* clicked at panel header? */
		if (pa->flag & PNL_CLOSEDX) {
			if (block->rect.xmin <= mx && block->rect.xmin + PNL_HEADER >= mx)
				inside_header = true;
		}
		else if (block->rect.xmin > mx || block->rect.xmax < mx) {
			/* outside left/right side */
		}
		else if ((block->rect.ymax <= my) && (block->rect.ymax + PNL_HEADER >= my)) {
			inside_header = true;
		}
		else if (!(pa->flag & PNL_CLOSEDY)) {
			/* open panel */
			if (pa->control & UI_PNL_SCALE) {
				if (block->rect.xmax - PNL_HEADER <= mx)
					if (block->rect.ymin + PNL_HEADER >= my)
						inside_scale = true;
			}
			if (block->rect.xmin <= mx && block->rect.xmax >= mx)
				if (block->rect.ymin <= my && block->rect.ymax + PNL_HEADER >= my)
					inside = true;
		}
		
		/* XXX hardcoded key warning */
		if ((inside || inside_header) && event->val == KM_PRESS) {
			if (event->type == AKEY && ((event->ctrl + event->oskey + event->shift + event->alt) == 0)) {
				
				if (pa->flag & PNL_CLOSEDY) {
					if ((block->rect.ymax <= my) && (block->rect.ymax + PNL_HEADER >= my))
						ui_handle_panel_header(C, block, mx, my, event->type, event->ctrl, event->shift);
				}
				else
					ui_handle_panel_header(C, block, mx, my, event->type, event->ctrl, event->shift);
				
				retval = WM_UI_HANDLER_BREAK;
				continue;
			}
		}
		
		/* on active button, do not handle panels */
		if (ui_but_is_active(ar))
			continue;
		
		if (inside || inside_header) {

			if (event->val == KM_PRESS) {
				
				/* open close on header */
				if (ELEM(event->type, RETKEY, PADENTER)) {
					if (inside_header) {
						ui_handle_panel_header(C, block, mx, my, RETKEY, event->ctrl, event->shift);
						retval = WM_UI_HANDLER_BREAK;
						break;
					}
				}
				else if (event->type == LEFTMOUSE) {
					/* all inside clicks should return in break - overlapping/float panels */
					retval = WM_UI_HANDLER_BREAK;
					
					if (inside_header) {
						ui_handle_panel_header(C, block, mx, my, 0, event->ctrl, event->shift);
						retval = WM_UI_HANDLER_BREAK;
						break;
					}
					else if (inside_scale && !(pa->flag & PNL_CLOSED)) {
						panel_activate_state(C, pa, PANEL_STATE_DRAG_SCALE);
						retval = WM_UI_HANDLER_BREAK;
						break;
					}

				}
				else if (event->type == RIGHTMOUSE) {
					if (inside_header) {
						ui_panel_menu(C, ar, block->panel);
						retval = WM_UI_HANDLER_BREAK;
						break;
					}
				}
				else if (event->type == ESCKEY) {
					/*XXX 2.50*/
#if 0
					if (block->handler) {
						rem_blockhandler(sa, block->handler);
						ED_region_tag_redraw(ar);
						retval = WM_UI_HANDLER_BREAK;
					}
#endif
				}
				else if (event->type == PADPLUSKEY || event->type == PADMINUS) {
#if 0 /* XXX make float panel exception? */
					int zoom = 0;
				
					/* if panel is closed, only zoom if mouse is over the header */
					if (pa->flag & (PNL_CLOSEDX | PNL_CLOSEDY)) {
						if (inside_header)
							zoom = 1;
					}
					else
						zoom = 1;

					if (zoom) {
						ScrArea *sa = CTX_wm_area(C);
						SpaceLink *sl = sa->spacedata.first;

						if (sa->spacetype != SPACE_BUTS) {
							if (!(pa->control & UI_PNL_SCALE)) {
								if (event->type == PADPLUSKEY) sl->blockscale += 0.1;
								else sl->blockscale -= 0.1;
								CLAMP(sl->blockscale, 0.6, 1.0);

								ED_region_tag_redraw(ar);
								retval = WM_UI_HANDLER_BREAK;
							}
						}
					}
#endif
				}
			}
		}
	}

	return retval;
}

/**************** window level modal panel interaction **************/

/* note, this is modal handler and should not swallow events for animation */
static int ui_handler_panel(bContext *C, const wmEvent *event, void *userdata)
{
	Panel *panel = userdata;
	uiHandlePanelData *data = panel->activedata;

	/* verify if we can stop */
	if (event->type == LEFTMOUSE && event->val != KM_PRESS) {
		ScrArea *sa = CTX_wm_area(C);
		ARegion *ar = CTX_wm_region(C);
		int align = panel_aligned(sa, ar);

		if (align)
			panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
		else
			panel_activate_state(C, panel, PANEL_STATE_EXIT);
	}
	else if (event->type == MOUSEMOVE) {
		if (data->state == PANEL_STATE_DRAG)
			ui_do_drag(C, event, panel);
	}
	else if (event->type == TIMER && event->customdata == data->animtimer) {
		if (data->state == PANEL_STATE_ANIMATION)
			ui_do_animate(C, panel);
		else if (data->state == PANEL_STATE_DRAG)
			ui_do_drag(C, event, panel);
	}

	data = panel->activedata;

	if (data && data->state == PANEL_STATE_ANIMATION)
		return WM_UI_HANDLER_CONTINUE;
	else
		return WM_UI_HANDLER_BREAK;
}

static void ui_handler_remove_panel(bContext *C, void *userdata)
{
	Panel *pa = userdata;

	panel_activate_state(C, pa, PANEL_STATE_EXIT);
}

static void panel_activate_state(const bContext *C, Panel *pa, uiHandlePanelState state)
{
	uiHandlePanelData *data = pa->activedata;
	wmWindow *win = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (data && data->state == state)
		return;

	if (state == PANEL_STATE_EXIT || state == PANEL_STATE_ANIMATION) {
		if (data && data->state != PANEL_STATE_ANIMATION) {
			/* XXX:
			 *	- the panel tabbing function call below (test_add_new_tabs()) has been commented out
			 *	  "It is too easy to do by accident when reordering panels,
			 *     is very hard to control and use, and has no real benefit." - BillRey
			 * Aligorith, 2009Sep
			 */
			//test_add_new_tabs(ar);   // also copies locations of tabs in dragged panel
			check_panel_overlap(ar, NULL);  /* clears */
		}

		pa->flag &= ~PNL_SELECT;
	}
	else
		pa->flag |= PNL_SELECT;

	if (data && data->animtimer) {
		WM_event_remove_timer(CTX_wm_manager(C), win, data->animtimer);
		data->animtimer = NULL;
	}

	if (state == PANEL_STATE_EXIT) {
		MEM_freeN(data);
		pa->activedata = NULL;

		WM_event_remove_ui_handler(&win->modalhandlers, ui_handler_panel, ui_handler_remove_panel, pa, false);
	}
	else {
		if (!data) {
			data = MEM_callocN(sizeof(uiHandlePanelData), "uiHandlePanelData");
			pa->activedata = data;

			WM_event_add_ui_handler(C, &win->modalhandlers, ui_handler_panel, ui_handler_remove_panel, pa, false);
		}

		if (ELEM(state, PANEL_STATE_ANIMATION, PANEL_STATE_DRAG))
			data->animtimer = WM_event_add_timer(CTX_wm_manager(C), win, TIMER, ANIMATION_INTERVAL);

		data->state = state;
		data->startx = win->eventstate->x;
		data->starty = win->eventstate->y;
		data->startofsx = pa->ofsx;
		data->startofsy = pa->ofsy;
		data->startsizex = pa->sizex;
		data->startsizey = pa->sizey;
		data->starttime = PIL_check_seconds_timer();
	}

	ED_region_tag_redraw(ar);
}

