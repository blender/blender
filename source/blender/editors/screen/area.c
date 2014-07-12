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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/area.c
 *  \ingroup edscr
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_linklist_stack.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"

#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_space_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "screen_intern.h"

extern void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3); /* xxx temp */

/* general area and region code */

static void region_draw_emboss(ARegion *ar, rcti *scirct)
{
	rcti rect;
	
	/* translate scissor rect to region space */
	rect.xmin = scirct->xmin - ar->winrct.xmin;
	rect.ymin = scirct->ymin - ar->winrct.ymin;
	rect.xmax = scirct->xmax - ar->winrct.xmin;
	rect.ymax = scirct->ymax - ar->winrct.ymin;
	
	/* set transp line */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	/* right  */
	glColor4ub(0, 0, 0, 30);
	sdrawline(rect.xmax, rect.ymin, rect.xmax, rect.ymax);
	
	/* bottom  */
	glColor4ub(0, 0, 0, 30);
	sdrawline(rect.xmin, rect.ymin, rect.xmax, rect.ymin);
	
	/* top  */
	glColor4ub(255, 255, 255, 30);
	sdrawline(rect.xmin, rect.ymax, rect.xmax, rect.ymax);

	/* left  */
	glColor4ub(255, 255, 255, 30);
	sdrawline(rect.xmin, rect.ymin, rect.xmin, rect.ymax);
	
	glDisable(GL_BLEND);
}

void ED_region_pixelspace(ARegion *ar)
{
	int width  = BLI_rcti_size_x(&ar->winrct) + 1;
	int height = BLI_rcti_size_y(&ar->winrct) + 1;
	
	wmOrtho2(-GLA_PIXEL_OFS, (float)width - GLA_PIXEL_OFS, -GLA_PIXEL_OFS, (float)height - GLA_PIXEL_OFS);
	glLoadIdentity();
}

/* only exported for WM */
void ED_region_do_listen(bScreen *sc, ScrArea *sa, ARegion *ar, wmNotifier *note)
{
	/* generic notes first */
	switch (note->category) {
		case NC_WM:
			if (note->data == ND_FILEREAD)
				ED_region_tag_redraw(ar);
			break;
		case NC_WINDOW:
			ED_region_tag_redraw(ar);
			break;
	}

	if (ar->type && ar->type->listener)
		ar->type->listener(sc, sa, ar, note);
}

/* only exported for WM */
void ED_area_do_listen(bScreen *sc, ScrArea *sa, wmNotifier *note)
{
	/* no generic notes? */
	if (sa->type && sa->type->listener) {
		sa->type->listener(sc, sa, note);
	}
}

/* only exported for WM */
void ED_area_do_refresh(bContext *C, ScrArea *sa)
{
	/* no generic notes? */
	if (sa->type && sa->type->refresh) {
		sa->type->refresh(C, sa);
	}
	sa->do_refresh = false;
}

/**
 * \brief Corner widgets use for dragging and splitting the view.
 */
static void area_draw_azone(short x1, short y1, short x2, short y2)
{
	int dx = x2 - x1;
	int dy = y2 - y1;

	dx = copysign(ceilf(0.3f * abs(dx)), dx);
	dy = copysign(ceilf(0.3f * abs(dy)), dy);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	
	glColor4ub(255, 255, 255, 180);
	fdrawline(x1, y2, x2, y1);
	glColor4ub(255, 255, 255, 130);
	fdrawline(x1, y2 - dy, x2 - dx, y1);
	glColor4ub(255, 255, 255, 80);
	fdrawline(x1, y2 - 2 * dy, x2 - 2 * dx, y1);
	
	glColor4ub(0, 0, 0, 210);
	fdrawline(x1, y2 + 1, x2 + 1, y1);
	glColor4ub(0, 0, 0, 180);
	fdrawline(x1, y2 - dy + 1, x2 - dx + 1, y1);
	glColor4ub(0, 0, 0, 150);
	fdrawline(x1, y2 - 2 * dy + 1, x2 - 2 * dx + 1, y1);

	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

static void region_draw_azone_icon(AZone *az)
{
	GLUquadricObj *qobj = NULL; 
	short midx = az->x1 + (az->x2 - az->x1) / 2;
	short midy = az->y1 + (az->y2 - az->y1) / 2;
		
	qobj = gluNewQuadric();
	
	glPushMatrix();
	glTranslatef(midx, midy, 0.0);
	
	/* outlined circle */
	glEnable(GL_LINE_SMOOTH);

	glColor4f(1.f, 1.f, 1.f, 0.8f);

	gluQuadricDrawStyle(qobj, GLU_FILL); 
	gluDisk(qobj, 0.0,  4.25f, 16, 1);

	glColor4f(0.2f, 0.2f, 0.2f, 0.9f);
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	gluDisk(qobj, 0.0,  4.25f, 16, 1);
	
	glDisable(GL_LINE_SMOOTH);
	
	glPopMatrix();
	gluDeleteQuadric(qobj);
	
	/* + */
	sdrawline(midx, midy - 2, midx, midy + 3);
	sdrawline(midx - 2, midy, midx + 3, midy);
}

static void draw_azone_plus(float x1, float y1, float x2, float y2)
{
	float width = 0.1f * U.widget_unit;
	float pad = 0.2f * U.widget_unit;
	
	glRectf((x1 + x2 - width) * 0.5f, y1 + pad, (x1 + x2 + width) * 0.5f, y2 - pad);
	glRectf(x1 + pad, (y1 + y2 - width) * 0.5f, (x1 + x2 - width) * 0.5f, (y1 + y2 + width) * 0.5f);
	glRectf((x1 + x2 + width) * 0.5f, (y1 + y2 - width) * 0.5f, x2 - pad, (y1 + y2 + width) * 0.5f);
}

static void region_draw_azone_tab_plus(AZone *az)
{
	glEnable(GL_BLEND);
	
	/* add code to draw region hidden as 'too small' */
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			uiSetRoundBox(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
			break;
		case AE_LEFT_TO_TOPRIGHT:
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
			break;
		case AE_RIGHT_TO_TOPLEFT:
			uiSetRoundBox(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
			break;
	}

	glColor4f(0.05f, 0.05f, 0.05f, 0.4f);
	uiRoundBox((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f);

	glEnable(GL_BLEND);

	glColor4f(0.8f, 0.8f, 0.8f, 0.4f);
	draw_azone_plus((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2);

	glDisable(GL_BLEND);
}

static void region_draw_azone_tab(AZone *az)
{
	float col[3];
	
	glEnable(GL_BLEND);
	UI_GetThemeColor3fv(TH_HEADER, col);
	glColor4f(col[0], col[1], col[2], 0.5f);
	
	/* add code to draw region hidden as 'too small' */
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, 0.3f + (float)az->y1, (float)az->x2, 0.3f + (float)az->y2, 4.0f);
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			uiSetRoundBox(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, 0.3f + (float)az->y1, (float)az->x2, 0.3f + (float)az->y2, 4.0f);
			break;
		case AE_LEFT_TO_TOPRIGHT:
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f);
			break;
		case AE_RIGHT_TO_TOPLEFT:
			uiSetRoundBox(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f);
			break;
	}
	
	glDisable(GL_BLEND);
}

static void region_draw_azone_tria(AZone *az)
{
	glEnable(GL_BLEND);
	//UI_GetThemeColor3fv(TH_HEADER, col);
	glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
	
	/* add code to draw region hidden as 'too small' */
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			ui_draw_anti_tria((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y1, (float)(az->x1 + az->x2) / 2, (float)az->y2);
			break;
			
		case AE_BOTTOM_TO_TOPLEFT:
			ui_draw_anti_tria((float)az->x1, (float)az->y2, (float)az->x2, (float)az->y2, (float)(az->x1 + az->x2) / 2, (float)az->y1);
			break;

		case AE_LEFT_TO_TOPRIGHT:
			ui_draw_anti_tria((float)az->x2, (float)az->y1, (float)az->x2, (float)az->y2, (float)az->x1, (float)(az->y1 + az->y2) / 2);
			break;
			
		case AE_RIGHT_TO_TOPLEFT:
			ui_draw_anti_tria((float)az->x1, (float)az->y1, (float)az->x1, (float)az->y2, (float)az->x2, (float)(az->y1 + az->y2) / 2);
			break;
			
	}
	
	glDisable(GL_BLEND);
}

static void region_draw_azones(ScrArea *sa, ARegion *ar)
{
	AZone *az;

	if (!sa)
		return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();
	glTranslatef(-ar->winrct.xmin, -ar->winrct.ymin, 0.0f);
	
	for (az = sa->actionzones.first; az; az = az->next) {
		/* test if action zone is over this region */
		rcti azrct;
		BLI_rcti_init(&azrct, az->x1, az->x2, az->y1, az->y2);

		if (BLI_rcti_isect(&ar->drawrct, &azrct, NULL)) {
			if (az->type == AZONE_AREA) {
				area_draw_azone(az->x1, az->y1, az->x2, az->y2);
			}
			else if (az->type == AZONE_REGION) {
				
				if (az->ar) {
					/* only display tab or icons when the region is hidden */
					if (az->ar->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) {
						if (G.debug_value == 3)
							region_draw_azone_icon(az);
						else if (G.debug_value == 2)
							region_draw_azone_tria(az);
						else if (G.debug_value == 1)
							region_draw_azone_tab(az);
						else
							region_draw_azone_tab_plus(az);
					}
				}
			}
		}
	}

	glPopMatrix();

	glDisable(GL_BLEND);
}

/* only exported for WM */
/* makes region ready for drawing, sets pixelspace */
void ED_region_set(const bContext *C, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	
	ar->drawrct = ar->winrct;
	
	/* note; this sets state, so we can use wmOrtho and friends */
	wmSubWindowScissorSet(win, ar->swinid, &ar->drawrct, true);
	
	UI_SetTheme(sa ? sa->spacetype : 0, ar->type ? ar->type->regionid : 0);
	
	ED_region_pixelspace(ar);
}


/* only exported for WM */
void ED_region_do_draw(bContext *C, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegionType *at = ar->type;
	bool scissor_pad;

	/* see BKE_spacedata_draw_locks() */
	if (at->do_lock)
		return;

	/* if no partial draw rect set, full rect */
	if (ar->drawrct.xmin == ar->drawrct.xmax) {
		ar->drawrct = ar->winrct;
		scissor_pad = true;
	}
	else {
		/* extra clip for safety */
		BLI_rcti_isect(&ar->winrct, &ar->drawrct, &ar->drawrct);
		scissor_pad = false;
	}

	ar->do_draw |= RGN_DRAWING;
	
	/* note; this sets state, so we can use wmOrtho and friends */
	wmSubWindowScissorSet(win, ar->swinid, &ar->drawrct, scissor_pad);
	
	UI_SetTheme(sa ? sa->spacetype : 0, at->regionid);
	
	/* optional header info instead? */
	if (ar->headerstr) {
		UI_ThemeClearColor(TH_HEADER);
		glClear(GL_COLOR_BUFFER_BIT);
		
		UI_ThemeColor(TH_TEXT);
		BLF_draw_default(UI_UNIT_X, 0.4f * UI_UNIT_Y, 0.0f, ar->headerstr, BLF_DRAW_STR_DUMMY_MAX);
	}
	else if (at->draw) {
		at->draw(C, ar);
	}

	/* XXX test: add convention to end regions always in pixel space, for drawing of borders/gestures etc */
	ED_region_pixelspace(ar);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_PIXEL);

	region_draw_azones(sa, ar);

	/* for debugging unneeded area redraws and partial redraw */
#if 0
	glEnable(GL_BLEND);
	glColor4f(drand48(), drand48(), drand48(), 0.1f);
	glRectf(ar->drawrct.xmin - ar->winrct.xmin, ar->drawrct.ymin - ar->winrct.ymin,
	        ar->drawrct.xmax - ar->winrct.xmin, ar->drawrct.ymax - ar->winrct.ymin);
	glDisable(GL_BLEND);
#endif

	ar->do_draw = 0;
	memset(&ar->drawrct, 0, sizeof(ar->drawrct));
	
	uiFreeInactiveBlocks(C, &ar->uiblocks);

	if (sa)
		region_draw_emboss(ar, &ar->winrct);
}

/* **********************************
 * maybe silly, but let's try for now
 * to keep these tags protected
 * ********************************** */

void ED_region_tag_redraw(ARegion *ar)
{
	/* don't tag redraw while drawing, it shouldn't happen normally
	 * but python scripts can cause this to happen indirectly */
	if (ar && !(ar->do_draw & RGN_DRAWING)) {
		/* zero region means full region redraw */
		ar->do_draw &= ~RGN_DRAW_PARTIAL;  /* just incase */
		ar->do_draw = RGN_DRAW;
		memset(&ar->drawrct, 0, sizeof(ar->drawrct));
	}
}

void ED_region_tag_redraw_overlay(ARegion *ar)
{
	if (ar)
		ar->do_draw_overlay = RGN_DRAW;
}

void ED_region_tag_refresh_ui(ARegion *ar)
{
	if (ar) {
		ar->do_draw |= RGN_DRAW_REFRESH_UI;
	}
}

void ED_region_tag_redraw_partial(ARegion *ar, rcti *rct)
{
	if (ar && !(ar->do_draw & RGN_DRAWING)) {
		if (!ar->do_draw) {
			/* no redraw set yet, set partial region */
			ar->do_draw = RGN_DRAW_PARTIAL;
			ar->drawrct = *rct;
		}
		else if (ar->drawrct.xmin != ar->drawrct.xmax) {
			/* partial redraw already set, expand region */
			BLI_rcti_union(&ar->drawrct, rct);
		}
	}
}

void ED_area_tag_redraw(ScrArea *sa)
{
	ARegion *ar;
	
	if (sa)
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			ED_region_tag_redraw(ar);
}

void ED_area_tag_redraw_regiontype(ScrArea *sa, int regiontype)
{
	ARegion *ar;
	
	if (sa) {
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiontype == regiontype) {
				ED_region_tag_redraw(ar);
			}
		}
	}
}

void ED_area_tag_refresh(ScrArea *sa)
{
	if (sa)
		sa->do_refresh = true;
}

/* *************************************************************** */

/* use NULL to disable it */
void ED_area_headerprint(ScrArea *sa, const char *str)
{
	ARegion *ar;

	/* happens when running transform operators in backround mode */
	if (sa == NULL)
		return;

	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_HEADER) {
			if (str) {
				if (ar->headerstr == NULL)
					ar->headerstr = MEM_mallocN(256, "headerprint");
				BLI_strncpy(ar->headerstr, str, 256);
			}
			else if (ar->headerstr) {
				MEM_freeN(ar->headerstr);
				ar->headerstr = NULL;
			}
			ED_region_tag_redraw(ar);
		}
	}
}

/* ************************************************************ */


static void area_azone_initialize(wmWindow *win, bScreen *screen, ScrArea *sa)
{
	AZone *az;
	
	/* reinitalize entirely, regions add azones too */
	BLI_freelistN(&sa->actionzones);

	if (screen->full != SCREENNORMAL) {
		return;
	}

	/* can't click on bottom corners on OS X, already used for resizing */
#ifdef __APPLE__
	if (!(sa->totrct.xmin == 0 && sa->totrct.ymin == 0) || WM_window_is_fullscreen(win))
#else
	(void)win;
#endif
	{
		/* set area action zones */
		az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&(sa->actionzones), az);
		az->type = AZONE_AREA;
		az->x1 = sa->totrct.xmin;
		az->y1 = sa->totrct.ymin;
		az->x2 = sa->totrct.xmin + (AZONESPOT - 1);
		az->y2 = sa->totrct.ymin + (AZONESPOT - 1);
		BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
	}
	
	az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
	BLI_addtail(&(sa->actionzones), az);
	az->type = AZONE_AREA;
	az->x1 = sa->totrct.xmax;
	az->y1 = sa->totrct.ymax;
	az->x2 = sa->totrct.xmax - (AZONESPOT - 1);
	az->y2 = sa->totrct.ymax - (AZONESPOT - 1);
	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

#define AZONEPAD_EDGE   (0.1f * U.widget_unit)
#define AZONEPAD_ICON   (0.45f * U.widget_unit)
static void region_azone_edge(AZone *az, ARegion *ar)
{
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			az->x1 = ar->winrct.xmin;
			az->y1 = ar->winrct.ymax - AZONEPAD_EDGE;
			az->x2 = ar->winrct.xmax;
			az->y2 = ar->winrct.ymax + AZONEPAD_EDGE;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1 = ar->winrct.xmin;
			az->y1 = ar->winrct.ymin + AZONEPAD_EDGE;
			az->x2 = ar->winrct.xmax;
			az->y2 = ar->winrct.ymin - AZONEPAD_EDGE;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1 = ar->winrct.xmin - AZONEPAD_EDGE;
			az->y1 = ar->winrct.ymin;
			az->x2 = ar->winrct.xmin + AZONEPAD_EDGE;
			az->y2 = ar->winrct.ymax;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax + AZONEPAD_EDGE;
			az->y1 = ar->winrct.ymin;
			az->x2 = ar->winrct.xmax - AZONEPAD_EDGE;
			az->y2 = ar->winrct.ymax;
			break;
	}

	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

static void region_azone_icon(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot = 0;
	
	/* count how many actionzones with along same edge are available.
	 * This allows for adding more action zones in the future without
	 * having to worry about correct offset */
	for (azt = sa->actionzones.first; azt; azt = azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			az->x1 = ar->winrct.xmax - tot * 2 * AZONEPAD_ICON;
			az->y1 = ar->winrct.ymax + AZONEPAD_ICON;
			az->x2 = ar->winrct.xmax - tot * AZONEPAD_ICON;
			az->y2 = ar->winrct.ymax + 2 * AZONEPAD_ICON;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1 = ar->winrct.xmin + AZONEPAD_ICON;
			az->y1 = ar->winrct.ymin - 2 * AZONEPAD_ICON;
			az->x2 = ar->winrct.xmin + 2 * AZONEPAD_ICON;
			az->y2 = ar->winrct.ymin - AZONEPAD_ICON;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1 = ar->winrct.xmin - 2 * AZONEPAD_ICON;
			az->y1 = ar->winrct.ymax - tot * 2 * AZONEPAD_ICON;
			az->x2 = ar->winrct.xmin - AZONEPAD_ICON;
			az->y2 = ar->winrct.ymax - tot * AZONEPAD_ICON;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax + AZONEPAD_ICON;
			az->y1 = ar->winrct.ymax - tot * 2 * AZONEPAD_ICON;
			az->x2 = ar->winrct.xmax + 2 * AZONEPAD_ICON;
			az->y2 = ar->winrct.ymax - tot * AZONEPAD_ICON;
			break;
	}

	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
	
	/* if more azones on 1 spot, set offset */
	for (azt = sa->actionzones.first; azt; azt = azt->next) {
		if (az != azt) {
			if (ABS(az->x1 - azt->x1) < 2 && ABS(az->y1 - azt->y1) < 2) {
				if (az->edge == AE_TOP_TO_BOTTOMRIGHT || az->edge == AE_BOTTOM_TO_TOPLEFT) {
					az->x1 += AZONESPOT;
					az->x2 += AZONESPOT;
				}
				else {
					az->y1 -= AZONESPOT;
					az->y2 -= AZONESPOT;
				}
				BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
			}
		}
	}
}

#define AZONEPAD_TAB_PLUSW  (0.7f * U.widget_unit)
#define AZONEPAD_TAB_PLUSH  (0.7f * U.widget_unit)

/* region already made zero sized, in shape of edge */
static void region_azone_tab_plus(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot = 0, add;
	
	for (azt = sa->actionzones.first; azt; azt = azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add = 1; else add = 0;
			az->x1 = ar->winrct.xmax - 2.5f * AZONEPAD_TAB_PLUSW;
			az->y1 = ar->winrct.ymax - add;
			az->x2 = ar->winrct.xmax - 1.5f * AZONEPAD_TAB_PLUSW;
			az->y2 = ar->winrct.ymax - add + AZONEPAD_TAB_PLUSH;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax - 2.5f * AZONEPAD_TAB_PLUSW;
			az->y1 = ar->winrct.ymin - AZONEPAD_TAB_PLUSH;
			az->x2 = ar->winrct.xmax - 1.5f * AZONEPAD_TAB_PLUSW;
			az->y2 = ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1 = ar->winrct.xmin - AZONEPAD_TAB_PLUSH;
			az->y1 = ar->winrct.ymax - 2.5f * AZONEPAD_TAB_PLUSW;
			az->x2 = ar->winrct.xmin;
			az->y2 = ar->winrct.ymax - 1.5f * AZONEPAD_TAB_PLUSW;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax - 1;
			az->y1 = ar->winrct.ymax - 2.5f * AZONEPAD_TAB_PLUSW;
			az->x2 = ar->winrct.xmax - 1 + AZONEPAD_TAB_PLUSH;
			az->y2 = ar->winrct.ymax - 1.5f * AZONEPAD_TAB_PLUSW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	


#define AZONEPAD_TABW   (0.9f * U.widget_unit)
#define AZONEPAD_TABH   (0.35f * U.widget_unit)

/* region already made zero sized, in shape of edge */
static void region_azone_tab(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot = 0, add;
	
	for (azt = sa->actionzones.first; azt; azt = azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add = 1; else add = 0;
			az->x1 = ar->winrct.xmax - 2 * AZONEPAD_TABW;
			az->y1 = ar->winrct.ymax - add;
			az->x2 = ar->winrct.xmax - AZONEPAD_TABW;
			az->y2 = ar->winrct.ymax - add + AZONEPAD_TABH;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1 = ar->winrct.xmin + AZONEPAD_TABW;
			az->y1 = ar->winrct.ymin - AZONEPAD_TABH;
			az->x2 = ar->winrct.xmin + 2 * AZONEPAD_TABW;
			az->y2 = ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1 = ar->winrct.xmin + 1 - AZONEPAD_TABH;
			az->y1 = ar->winrct.ymax - 2 * AZONEPAD_TABW;
			az->x2 = ar->winrct.xmin + 1;
			az->y2 = ar->winrct.ymax - AZONEPAD_TABW;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax - 1;
			az->y1 = ar->winrct.ymax - 2 * AZONEPAD_TABW;
			az->x2 = ar->winrct.xmax - 1 + AZONEPAD_TABH;
			az->y2 = ar->winrct.ymax - AZONEPAD_TABW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	

#define AZONEPAD_TRIAW  (0.8f * U.widget_unit)
#define AZONEPAD_TRIAH  (0.45f * U.widget_unit)


/* region already made zero sized, in shape of edge */
static void region_azone_tria(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot = 0, add;
	
	for (azt = sa->actionzones.first; azt; azt = azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch (az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add = 1; else add = 0;
			az->x1 = ar->winrct.xmax - 2 * AZONEPAD_TRIAW;
			az->y1 = ar->winrct.ymax - add;
			az->x2 = ar->winrct.xmax - AZONEPAD_TRIAW;
			az->y2 = ar->winrct.ymax - add + AZONEPAD_TRIAH;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1 = ar->winrct.xmin + AZONEPAD_TRIAW;
			az->y1 = ar->winrct.ymin - AZONEPAD_TRIAH;
			az->x2 = ar->winrct.xmin + 2 * AZONEPAD_TRIAW;
			az->y2 = ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1 = ar->winrct.xmin + 1 - AZONEPAD_TRIAH;
			az->y1 = ar->winrct.ymax - 2 * AZONEPAD_TRIAW;
			az->x2 = ar->winrct.xmin + 1;
			az->y2 = ar->winrct.ymax - AZONEPAD_TRIAW;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1 = ar->winrct.xmax - 1;
			az->y1 = ar->winrct.ymax - 2 * AZONEPAD_TRIAW;
			az->x2 = ar->winrct.xmax - 1 + AZONEPAD_TRIAH;
			az->y2 = ar->winrct.ymax - AZONEPAD_TRIAW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	


static void region_azone_initialize(ScrArea *sa, ARegion *ar, AZEdge edge) 
{
	AZone *az;
	
	az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
	BLI_addtail(&(sa->actionzones), az);
	az->type = AZONE_REGION;
	az->ar = ar;
	az->edge = edge;
	
	if (ar->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) {
		if (G.debug_value == 3)
			region_azone_icon(sa, az, ar);
		else if (G.debug_value == 2)
			region_azone_tria(sa, az, ar);
		else if (G.debug_value == 1)
			region_azone_tab(sa, az, ar);
		else
			region_azone_tab_plus(sa, az, ar);
	}
	else {
		region_azone_edge(az, ar);
	}
	
}


/* *************************************************************** */

static void region_azone_add(ScrArea *sa, ARegion *ar, int alignment)
{
	/* edge code (t b l r) is along which area edge azone will be drawn */
	
	if (alignment == RGN_ALIGN_TOP)
		region_azone_initialize(sa, ar, AE_BOTTOM_TO_TOPLEFT);
	else if (alignment == RGN_ALIGN_BOTTOM)
		region_azone_initialize(sa, ar, AE_TOP_TO_BOTTOMRIGHT);
	else if (alignment == RGN_ALIGN_RIGHT)
		region_azone_initialize(sa, ar, AE_LEFT_TO_TOPRIGHT);
	else if (alignment == RGN_ALIGN_LEFT)
		region_azone_initialize(sa, ar, AE_RIGHT_TO_TOPLEFT);
}

/* dir is direction to check, not the splitting edge direction! */
static int rct_fits(rcti *rect, char dir, int size)
{
	if (dir == 'h') {
		return BLI_rcti_size_x(rect) + 1 - size;
	}
	else {  /* 'v' */
		return BLI_rcti_size_y(rect) + 1 - size;
	}
}

/* *************************************************************** */

/* ar should be overlapping */
/* function checks if some overlapping region was defined before - on same place */
static void region_overlap_fix(ScrArea *sa, ARegion *ar)
{
	ARegion *ar1 = ar->prev;
	
	/* find overlapping previous region on same place */
	while (ar1) {
		if (ar1->overlap) {
			if ((ar1->alignment & RGN_SPLIT_PREV) == 0)
				if (BLI_rcti_isect(&ar1->winrct, &ar->winrct, NULL))
					break;
		}
		ar1 = ar1->prev;
	}
	
	/* translate or close */
	if (ar1) {
		int align1 = ar1->alignment & ~RGN_SPLIT_PREV;

		if (align1 == RGN_ALIGN_LEFT) {
			if (ar->winrct.xmax + ar1->winx > sa->winx - U.widget_unit)
				ar->flag |= RGN_FLAG_TOO_SMALL;
			else
				BLI_rcti_translate(&ar->winrct, ar1->winx, 0);
		}
		else if (align1 == RGN_ALIGN_RIGHT) {
			if (ar->winrct.xmin - ar1->winx < U.widget_unit)
				ar->flag |= RGN_FLAG_TOO_SMALL;
			else
				BLI_rcti_translate(&ar->winrct, -ar1->winx, 0);
		}
	}

	
	
}

/* overlapping regions only in the following restricted cases */
static bool region_is_overlap(wmWindow *win, ScrArea *sa, ARegion *ar)
{
	if (U.uiflag2 & USER_REGION_OVERLAP) {
		if (WM_is_draw_triple(win)) {
			if (ELEM(sa->spacetype, SPACE_VIEW3D, SPACE_SEQ)) {
				if (ELEM3(ar->regiontype, RGN_TYPE_TOOLS, RGN_TYPE_UI, RGN_TYPE_TOOL_PROPS))
					return 1;
			}
			else if (sa->spacetype == SPACE_IMAGE) {
				if (ELEM4(ar->regiontype, RGN_TYPE_TOOLS, RGN_TYPE_UI, RGN_TYPE_TOOL_PROPS, RGN_TYPE_PREVIEW))
					return 1;
			}
		}
	}

	return 0;
}

static void region_rect_recursive(wmWindow *win, ScrArea *sa, ARegion *ar, rcti *remainder, int quad)
{
	rcti *remainder_prev = remainder;
	int prefsizex, prefsizey;
	int alignment;
	
	if (ar == NULL)
		return;
	
	/* no returns in function, winrct gets set in the end again */
	BLI_rcti_init(&ar->winrct, 0, 0, 0, 0);
	
	/* for test; allow split of previously defined region */
	if (ar->alignment & RGN_SPLIT_PREV)
		if (ar->prev)
			remainder = &ar->prev->winrct;
	
	alignment = ar->alignment & ~RGN_SPLIT_PREV;
	
	/* set here, assuming userpref switching forces to call this again */
	ar->overlap = region_is_overlap(win, sa, ar);

	/* clear state flags first */
	ar->flag &= ~RGN_FLAG_TOO_SMALL;
	/* user errors */
	if (ar->next == NULL && alignment != RGN_ALIGN_QSPLIT)
		alignment = RGN_ALIGN_NONE;
	
	/* prefsize, for header we stick to exception (prevent dpi rounding error) */
	prefsizex = UI_DPI_FAC * (ar->sizex > 1 ? ar->sizex + 0.5f : ar->type->prefsizex);
	
	if (ar->regiontype == RGN_TYPE_HEADER) {
		prefsizey = ED_area_headersize();
	}
	else if (ar->regiontype == RGN_TYPE_UI && sa->spacetype == SPACE_FILE) {
		prefsizey = UI_UNIT_Y * 2 + (UI_UNIT_Y / 2);
	}
	else {
		prefsizey = UI_DPI_FAC * (ar->sizey > 1 ? ar->sizey + 0.5f : ar->type->prefsizey);
	}


	if (ar->flag & RGN_FLAG_HIDDEN) {
		/* hidden is user flag */
	}
	else if (alignment == RGN_ALIGN_FLOAT) {
		/* XXX floating area region, not handled yet here */
	}
	else if (rct_fits(remainder, 'v', 1) < 0 || rct_fits(remainder, 'h', 1) < 0) {
		/* remainder is too small for any usage */
		ar->flag |= RGN_FLAG_TOO_SMALL;
	}
	else if (alignment == RGN_ALIGN_NONE) {
		/* typically last region */
		ar->winrct = *remainder;
		BLI_rcti_init(remainder, 0, 0, 0, 0);
	}
	else if (alignment == RGN_ALIGN_TOP || alignment == RGN_ALIGN_BOTTOM) {
		
		if (rct_fits(remainder, 'v', prefsizey) < 0) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac = rct_fits(remainder, 'v', prefsizey);

			if (fac < 0)
				prefsizey += fac;
			
			ar->winrct = *remainder;
			
			if (alignment == RGN_ALIGN_TOP) {
				ar->winrct.ymin = ar->winrct.ymax - prefsizey + 1;
				remainder->ymax = ar->winrct.ymin - 1;
			}
			else {
				ar->winrct.ymax = ar->winrct.ymin + prefsizey - 1;
				remainder->ymin = ar->winrct.ymax + 1;
			}
		}
	}
	else if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
		
		if (rct_fits(remainder, 'h', prefsizex) < 0) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac = rct_fits(remainder, 'h', prefsizex);
			
			if (fac < 0)
				prefsizex += fac;
			
			ar->winrct = *remainder;
			
			if (alignment == RGN_ALIGN_RIGHT) {
				ar->winrct.xmin = ar->winrct.xmax - prefsizex + 1;
				if (ar->overlap == 0)
					remainder->xmax = ar->winrct.xmin - 1;
			}
			else {
				ar->winrct.xmax = ar->winrct.xmin + prefsizex - 1;
				if (ar->overlap == 0)
					remainder->xmin = ar->winrct.xmax + 1;
			}
		}
	}
	else if (alignment == RGN_ALIGN_VSPLIT || alignment == RGN_ALIGN_HSPLIT) {
		/* percentage subdiv*/
		ar->winrct = *remainder;
		
		if (alignment == RGN_ALIGN_HSPLIT) {
			if (rct_fits(remainder, 'h', prefsizex) > 4) {
				ar->winrct.xmax = BLI_rcti_cent_x(remainder);
				remainder->xmin = ar->winrct.xmax + 1;
			}
			else {
				BLI_rcti_init(remainder, 0, 0, 0, 0);
			}
		}
		else {
			if (rct_fits(remainder, 'v', prefsizey) > 4) {
				ar->winrct.ymax = BLI_rcti_cent_y(remainder);
				remainder->ymin = ar->winrct.ymax + 1;
			}
			else {
				BLI_rcti_init(remainder, 0, 0, 0, 0);
			}
		}
	}
	else if (alignment == RGN_ALIGN_QSPLIT) {
		ar->winrct = *remainder;
		
		/* test if there's still 4 regions left */
		if (quad == 0) {
			ARegion *artest = ar->next;
			int count = 1;
			
			while (artest) {
				artest->alignment = RGN_ALIGN_QSPLIT;
				artest = artest->next;
				count++;
			}
			
			if (count != 4) {
				/* let's stop adding regions */
				BLI_rcti_init(remainder, 0, 0, 0, 0);
				if (G.debug & G_DEBUG)
					printf("region quadsplit failed\n");
			}
			else {
				quad = 1;
			}
		}
		if (quad) {
			if (quad == 1) { /* left bottom */
				ar->winrct.xmax = BLI_rcti_cent_x(remainder);
				ar->winrct.ymax = BLI_rcti_cent_y(remainder);
			}
			else if (quad == 2) { /* left top */
				ar->winrct.xmax = BLI_rcti_cent_x(remainder);
				ar->winrct.ymin = BLI_rcti_cent_y(remainder) + 1;
			}
			else if (quad == 3) { /* right bottom */
				ar->winrct.xmin = BLI_rcti_cent_x(remainder) + 1;
				ar->winrct.ymax = BLI_rcti_cent_y(remainder);
			}
			else {  /* right top */
				ar->winrct.xmin = BLI_rcti_cent_x(remainder) + 1;
				ar->winrct.ymin = BLI_rcti_cent_y(remainder) + 1;
				BLI_rcti_init(remainder, 0, 0, 0, 0);
			}

			quad++;
		}
	}
	
	/* for speedup */
	ar->winx = BLI_rcti_size_x(&ar->winrct) + 1;
	ar->winy = BLI_rcti_size_y(&ar->winrct) + 1;
	
	/* if region opened normally, we store this for hide/reveal usage */
	/* prevent rounding errors for UI_DPI_FAC mult and divide */
	if (ar->winx > 1) ar->sizex = (ar->winx + 0.5f) /  UI_DPI_FAC;
	if (ar->winy > 1) ar->sizey = (ar->winy + 0.5f) /  UI_DPI_FAC;
		
	/* exception for multiple overlapping regions on same spot */
	if (ar->overlap)
		region_overlap_fix(sa, ar);

	/* set winrect for azones */
	if (ar->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) {
		ar->winrct = *remainder;
		
		if (alignment == RGN_ALIGN_TOP)
			ar->winrct.ymin = ar->winrct.ymax;
		else if (alignment == RGN_ALIGN_BOTTOM)
			ar->winrct.ymax = ar->winrct.ymin;
		else if (alignment == RGN_ALIGN_RIGHT)
			ar->winrct.xmin = ar->winrct.xmax;
		else if (alignment == RGN_ALIGN_LEFT)
			ar->winrct.xmax = ar->winrct.xmin;
		else /* prevent winrct to be valid */
			ar->winrct.xmax = ar->winrct.xmin;
	}

	/* restore prev-split exception */
	if (ar->alignment & RGN_SPLIT_PREV) {
		if (ar->prev) {
			remainder = remainder_prev;
			ar->prev->winx = BLI_rcti_size_x(&ar->prev->winrct) + 1;
			ar->prev->winy = BLI_rcti_size_y(&ar->prev->winrct) + 1;
		}
	}
	
	/* in end, add azones, where appropriate */
	if (ar->regiontype == RGN_TYPE_HEADER && ar->winy + 6 > sa->winy) {
		/* The logic for this is: when the header takes up the full area,
		 * disallow hiding it to view the main window.
		 *
		 * Without this, you can drag down the file selectors header and hide it
		 * by accident very easily (highly annoying!), the value 6 is arbitrary
		 * but accounts for small common rounding problems when scaling the UI,
		 * must be minimum '4' */
	}
	else {
		region_azone_add(sa, ar, alignment);
	}

	region_rect_recursive(win, sa, ar->next, remainder, quad);
}

static void area_calc_totrct(ScrArea *sa, int sizex, int sizey)
{
	short rt = (short) U.pixelsize;

	if (sa->v1->vec.x > 0) sa->totrct.xmin = sa->v1->vec.x + rt;
	else sa->totrct.xmin = sa->v1->vec.x;
	if (sa->v4->vec.x < sizex - 1) sa->totrct.xmax = sa->v4->vec.x - rt;
	else sa->totrct.xmax = sa->v4->vec.x;
	
	if (sa->v1->vec.y > 0) sa->totrct.ymin = sa->v1->vec.y + rt;
	else sa->totrct.ymin = sa->v1->vec.y;
	if (sa->v2->vec.y < sizey - 1) sa->totrct.ymax = sa->v2->vec.y - rt;
	else sa->totrct.ymax = sa->v2->vec.y;
	
	/* for speedup */
	sa->winx = BLI_rcti_size_x(&sa->totrct) + 1;
	sa->winy = BLI_rcti_size_y(&sa->totrct) + 1;
}


/* used for area initialize below */
static void region_subwindow(wmWindow *win, ARegion *ar)
{
	bool hidden = (ar->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) != 0;

	if ((ar->alignment & RGN_SPLIT_PREV) && ar->prev)
		hidden = hidden || (ar->prev->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));

	if (hidden) {
		if (ar->swinid)
			wm_subwindow_close(win, ar->swinid);
		ar->swinid = 0;
	}
	else if (ar->swinid == 0)
		ar->swinid = wm_subwindow_open(win, &ar->winrct);
	else 
		wm_subwindow_position(win, ar->swinid, &ar->winrct);
}

static void ed_default_handlers(wmWindowManager *wm, ScrArea *sa, ListBase *handlers, int flag)
{
	/* note, add-handler checks if it already exists */
	
	/* XXX it would be good to have boundbox checks for some of these... */
	if (flag & ED_KEYMAP_UI) {
		/* user interface widgets */
		UI_add_region_handlers(handlers);
	}
	if (flag & ED_KEYMAP_VIEW2D) {
		/* 2d-viewport handling+manipulation */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "View2D", 0, 0);
		WM_event_add_keymap_handler(handlers, keymap);
	}
	if (flag & ED_KEYMAP_MARKERS) {
		/* time-markers */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Markers", 0, 0);
		
		/* time space only has this keymap, the others get a boundbox restricted map */
		if (sa->spacetype != SPACE_TIME) {
			ARegion *ar;
			static rcti rect = {0, 10000, 0, 30};    /* same local check for all areas */
			ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
			if (ar) {
				WM_event_add_keymap_handler_bb(handlers, keymap, &rect, &ar->winrct);
			}
		}
		else
			WM_event_add_keymap_handler(handlers, keymap);
	}
	if (flag & ED_KEYMAP_ANIMATION) {
		/* frame changing and timeline operators (for time spaces) */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Animation", 0, 0);
		WM_event_add_keymap_handler(handlers, keymap);
	}
	if (flag & ED_KEYMAP_FRAMES) {
		/* frame changing/jumping (for all spaces) */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Frames", 0, 0);
		WM_event_add_keymap_handler(handlers, keymap);
	}
	if (flag & ED_KEYMAP_GPENCIL) {
		/* grease pencil */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Grease Pencil", 0, 0);
		WM_event_add_keymap_handler(handlers, keymap);
	}
	if (flag & ED_KEYMAP_HEADER) {
		/* standard keymap for headers regions */
		wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Header", 0, 0);
		WM_event_add_keymap_handler(handlers, keymap);
	}
}


/* called in screen_refresh, or screens_init, also area size changes */
void ED_area_initialize(wmWindowManager *wm, wmWindow *win, ScrArea *sa)
{
	ARegion *ar;
	rcti rect;
	
	/* set typedefinitions */
	sa->type = BKE_spacetype_from_id(sa->spacetype);
	
	if (sa->type == NULL) {
		sa->butspacetype = sa->spacetype = SPACE_VIEW3D;
		sa->type = BKE_spacetype_from_id(sa->spacetype);
	}
	
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		ar->type = BKE_regiontype_from_id(sa->type, ar->regiontype);
	
	/* area sizes */
	area_calc_totrct(sa, WM_window_pixels_x(win), WM_window_pixels_y(win));
	
	/* clear all azones, add the area triange widgets */
	area_azone_initialize(win, win->screen, sa);

	/* region rect sizes */
	rect = sa->totrct;
	region_rect_recursive(win, sa, sa->regionbase.first, &rect, 0);
	
	/* default area handlers */
	ed_default_handlers(wm, sa, &sa->handlers, sa->type->keymapflag);
	/* checks spacedata, adds own handlers */
	if (sa->type->init)
		sa->type->init(wm, sa);
	
	/* region windows, default and own handlers */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		region_subwindow(win, ar);
		
		if (ar->swinid) {
			/* default region handlers */
			ed_default_handlers(wm, sa, &ar->handlers, ar->type->keymapflag);
			/* own handlers */
			if (ar->type->init)
				ar->type->init(wm, ar);
		}
		else {
			/* prevent uiblocks to run */
			uiFreeBlocks(NULL, &ar->uiblocks);
		}
	}
}

static void region_update_rect(ARegion *ar)
{
	ar->winx = BLI_rcti_size_x(&ar->winrct) + 1;
	ar->winy = BLI_rcti_size_y(&ar->winrct) + 1;

	/* v2d mask is used to subtract scrollbars from a 2d view. Needs initialize here. */
	BLI_rcti_init(&ar->v2d.mask, 0, ar->winx - 1, 0, ar->winy -1);
}

/**
 * Call to move a popup window (keep OpenGL context free!)
 */
void ED_region_update_rect(bContext *C, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);

	wm_subwindow_rect_set(win, ar->swinid, &ar->winrct);

	region_update_rect(ar);
}

/* externally called for floating regions like menus */
void ED_region_init(bContext *C, ARegion *ar)
{
//	ARegionType *at = ar->type;
	
	/* refresh can be called before window opened */
	region_subwindow(CTX_wm_window(C), ar);
	
	region_update_rect(ar);
}

/* for quick toggle, can skip fades */
void region_toggle_hidden(bContext *C, ARegion *ar, const bool do_fade)
{
	ScrArea *sa = CTX_wm_area(C);
	
	ar->flag ^= RGN_FLAG_HIDDEN;
	
	if (do_fade && ar->overlap) {
		/* starts a timer, and in end calls the stuff below itself (region_sblend_invoke()) */
		region_blend_start(C, sa, ar);
	}
	else {
		if (ar->flag & RGN_FLAG_HIDDEN)
			WM_event_remove_handlers(C, &ar->handlers);
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
}

/* exported to all editors, uses fading default */
void ED_region_toggle_hidden(bContext *C, ARegion *ar)
{
	region_toggle_hidden(C, ar, 1);
}

/**
 * we swap spaces for fullscreen to keep all allocated data area vertices were set
 */
void ED_area_data_copy(ScrArea *sa_dst, ScrArea *sa_src, const bool do_free)
{
	SpaceType *st;
	ARegion *ar;
	const char spacetype = sa_dst->spacetype;
	const short flag_copy = HEADER_NO_PULLDOWN;
	
	sa_dst->headertype = sa_src->headertype;
	sa_dst->spacetype = sa_src->spacetype;
	sa_dst->type = sa_src->type;
	sa_dst->butspacetype = sa_src->butspacetype;

	sa_dst->flag = (sa_dst->flag & ~flag_copy) | (sa_src->flag & flag_copy);

	/* area */
	if (do_free) {
		BKE_spacedata_freelist(&sa_dst->spacedata);
	}
	BKE_spacedata_copylist(&sa_dst->spacedata, &sa_src->spacedata);

	/* Note; SPACE_EMPTY is possible on new screens */

	/* regions */
	if (do_free) {
		st = BKE_spacetype_from_id(spacetype);
		for (ar = sa_dst->regionbase.first; ar; ar = ar->next)
			BKE_area_region_free(st, ar);
		BLI_freelistN(&sa_dst->regionbase);
	}
	st = BKE_spacetype_from_id(sa_src->spacetype);
	for (ar = sa_src->regionbase.first; ar; ar = ar->next) {
		ARegion *newar = BKE_area_region_copy(st, ar);
		BLI_addtail(&sa_dst->regionbase, newar);
	}
}

void ED_area_data_swap(ScrArea *sa_dst, ScrArea *sa_src)
{
	sa_dst->headertype = sa_src->headertype;
	sa_dst->spacetype = sa_src->spacetype;
	sa_dst->type = sa_src->type;
	sa_dst->butspacetype = sa_src->butspacetype;


	SWAP(ListBase, sa_dst->spacedata, sa_src->spacedata);
	SWAP(ListBase, sa_dst->regionbase, sa_src->regionbase);
}

/* *********** Space switching code *********** */

void ED_area_swapspace(bContext *C, ScrArea *sa1, ScrArea *sa2)
{
	ScrArea *tmp = MEM_callocN(sizeof(ScrArea), "addscrarea");

	ED_area_exit(C, sa1);
	ED_area_exit(C, sa2);

	ED_area_data_copy(tmp, sa1, false);
	ED_area_data_copy(sa1, sa2, true);
	ED_area_data_copy(sa2, tmp, true);
	ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa1);
	ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa2);

	BKE_screen_area_free(tmp);
	MEM_freeN(tmp);

	/* tell WM to refresh, cursor types etc */
	WM_event_add_mousemove(C);
	
	ED_area_tag_redraw(sa1);
	ED_area_tag_refresh(sa1);
	ED_area_tag_redraw(sa2);
	ED_area_tag_refresh(sa2);
}

void ED_area_newspace(bContext *C, ScrArea *sa, int type)
{
	if (sa->spacetype != type) {
		SpaceType *st;
		SpaceLink *slold;
		SpaceLink *sl;

		ED_area_exit(C, sa);

		st = BKE_spacetype_from_id(type);
		slold = sa->spacedata.first;

		sa->spacetype = type;
		sa->butspacetype = type;
		sa->type = st;
		
		/* check previously stored space */
		for (sl = sa->spacedata.first; sl; sl = sl->next)
			if (sl->spacetype == type)
				break;
		
		/* old spacedata... happened during work on 2.50, remove */
		if (sl && BLI_listbase_is_empty(&sl->regionbase)) {
			st->free(sl);
			BLI_freelinkN(&sa->spacedata, sl);
			if (slold == sl) {
				slold = NULL;
			}
			sl = NULL;
		}
		
		if (sl) {
			/* swap regions */
			slold->regionbase = sa->regionbase;
			sa->regionbase = sl->regionbase;
			BLI_listbase_clear(&sl->regionbase);
			
			/* put in front of list */
			BLI_remlink(&sa->spacedata, sl);
			BLI_addhead(&sa->spacedata, sl);
		}
		else {
			/* new space */
			if (st) {
				sl = st->new(C);
				BLI_addhead(&sa->spacedata, sl);
				
				/* swap regions */
				if (slold)
					slold->regionbase = sa->regionbase;
				sa->regionbase = sl->regionbase;
				BLI_listbase_clear(&sl->regionbase);
			}
		}
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		
		/* tell WM to refresh, cursor types etc */
		WM_event_add_mousemove(C);
				
		/* send space change notifier */
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, sa);
		
		ED_area_tag_refresh(sa);
	}
	
	/* also redraw when re-used */
	ED_area_tag_redraw(sa);
}

void ED_area_prevspace(bContext *C, ScrArea *sa)
{
	SpaceLink *sl = (sa) ? sa->spacedata.first : CTX_wm_space_data(C);

	if (sl && sl->next) {
		/* workaround for case of double prevspace, render window
		 * with a file browser on top of it */
		if (sl->next->spacetype == SPACE_FILE && sl->next->next)
			ED_area_newspace(C, sa, sl->next->next->spacetype);
		else
			ED_area_newspace(C, sa, sl->next->spacetype);
	}
	else {
		/* no change */
		return;
	}
	ED_area_tag_redraw(sa);

	/* send space change notifier */
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, sa);
}

/* returns offset for next button in header */
int ED_area_header_switchbutton(const bContext *C, uiBlock *block, int yco)
{
	ScrArea *sa = CTX_wm_area(C);
	bScreen *scr = CTX_wm_screen(C);
	PointerRNA areaptr;
	int xco = 0.4 * U.widget_unit;

	RNA_pointer_create(&(scr->id), &RNA_Area, sa, &areaptr);

	uiDefButR(block, MENU, 0, "", xco, yco, 1.5 * U.widget_unit, U.widget_unit,
	          &areaptr, "type", 0, 0.0f, 0.0f, 0.0f, 0.0f, "");

	return xco + 1.7 * U.widget_unit;
}

/************************ standard UI regions ************************/

void ED_region_panels(const bContext *C, ARegion *ar, int vertical, const char *context, int contextnr)
{
	ScrArea *sa = CTX_wm_area(C);
	uiStyle *style = UI_GetStyleDraw();
	uiBlock *block;
	PanelType *pt;
	Panel *panel;
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;
	int x, y, xco, yco, w, em, triangle;
	bool is_context_new = 0;
	int redo;
	int scroll;

	bool use_category_tabs = (ar->regiontype == RGN_TYPE_TOOLS);  /* XXX, should use some better check? */
	/* offset panels for small vertical tab area */
	const char *category = NULL;
	const int category_tabs_width = UI_PANEL_CATEGORY_MARGIN_WIDTH;
	int margin_x = 0;

	BLI_SMALLSTACK_DECLARE(pt_stack, PanelType *);

	if (contextnr != -1)
		is_context_new = UI_view2d_tab_set(v2d, contextnr);
	
	/* before setting the view */
	if (vertical) {
		/* only allow scrolling in vertical direction */
		v2d->keepofs |= V2D_LOCKOFS_X | V2D_KEEPOFS_Y;
		v2d->keepofs &= ~(V2D_LOCKOFS_Y | V2D_KEEPOFS_X);
		v2d->scroll &= ~(V2D_SCROLL_BOTTOM);
		v2d->scroll |= (V2D_SCROLL_RIGHT);
	}
	else {
		/* for now, allow scrolling in both directions (since layouts are optimized for vertical,
		 * they often don't fit in horizontal layout)
		 */
		v2d->keepofs &= ~(V2D_LOCKOFS_X | V2D_LOCKOFS_Y | V2D_KEEPOFS_X | V2D_KEEPOFS_Y);
		v2d->scroll |= (V2D_SCROLL_BOTTOM);
		v2d->scroll &= ~(V2D_SCROLL_RIGHT);
	}

	scroll = v2d->scroll;


	/* collect panels to draw */
	for (pt = ar->type->paneltypes.last; pt; pt = pt->prev) {
		/* verify context */
		if (context && pt->context[0] && !STREQ(context, pt->context)) {
			continue;
		}

		/* draw panel */
		if (pt->draw && (!pt->poll || pt->poll(C, pt))) {
			BLI_SMALLSTACK_PUSH(pt_stack, pt);
		}
	}


	/* collect categories */
	if (use_category_tabs) {
		UI_panel_category_clear_all(ar);

		/* gather unique categories */
		BLI_SMALLSTACK_ITER_BEGIN(pt_stack, pt)
		{
			if (pt->category[0]) {
				if (!UI_panel_category_find(ar, pt->category)) {
					UI_panel_category_add(ar, pt->category);
				}
			}
		}
		BLI_SMALLSTACK_ITER_END;

		if (!UI_panel_category_is_visible(ar)) {
			use_category_tabs = false;
		}
		else {
			category = UI_panel_category_active_get(ar, true);
			margin_x = category_tabs_width;
		}
	}


	/* sortof hack - but we cannot predict the height of panels, until it's being generated */
	/* the layout engine works with fixed width (from v2d->cur), which is being set at end of the loop */
	/* in case scroller settings (hide flags) differ from previous, the whole loop gets done again */
	for (redo = 2; redo > 0; redo--) {
		
		if (vertical) {
			w = BLI_rctf_size_x(&v2d->cur);
			em = (ar->type->prefsizex) ? 10 : 20; /* works out to 10*UI_UNIT_X or 20*UI_UNIT_X */
		}
		else {
			w = UI_PANEL_WIDTH;
			em = (ar->type->prefsizex) ? 10 : 20;
		}

		w -= margin_x;
		
		/* create panels */
		uiBeginPanels(C, ar);

		/* set view2d view matrix  - uiBeginBlock() stores it */
		UI_view2d_view_ortho(v2d);

		BLI_SMALLSTACK_ITER_BEGIN(pt_stack, pt)
		{
			bool open;

			panel = uiPanelFindByType(ar, pt);

			if (use_category_tabs && pt->category[0] && !STREQ(category, pt->category)) {
				if ((panel == NULL) || ((panel->flag & PNL_PIN) == 0)) {
					continue;
				}
			}

			/* draw panel */
			block = uiBeginBlock(C, ar, pt->idname, UI_EMBOSS);
			panel = uiBeginPanel(sa, ar, block, pt, panel, &open);

			/* bad fixed values */
			triangle = (int)(UI_UNIT_Y * 1.1f);

			if (pt->draw_header && !(pt->flag & PNL_NO_HEADER) && (open || vertical)) {
				/* for enabled buttons */
				panel->layout = uiBlockLayout(block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER,
				                              triangle, (UI_UNIT_Y * 1.1f) + style->panelspace, UI_UNIT_Y, 1, 0, style);

				pt->draw_header(C, panel);

				uiBlockLayoutResolve(block, &xco, &yco);
				panel->labelofs = xco - triangle;
				panel->layout = NULL;
			}
			else {
				panel->labelofs = 0;
			}

			if (open) {
				short panelContext;

				/* panel context can either be toolbar region or normal panels region */
				if (ar->regiontype == RGN_TYPE_TOOLS)
					panelContext = UI_LAYOUT_TOOLBAR;
				else
					panelContext = UI_LAYOUT_PANEL;

				panel->layout = uiBlockLayout(block, UI_LAYOUT_VERTICAL, panelContext,
				                              style->panelspace, 0, w - 2 * style->panelspace, em, 0, style);

				pt->draw(C, panel);

				uiBlockLayoutResolve(block, &xco, &yco);
				panel->layout = NULL;

				yco -= 2 * style->panelspace;
				uiEndPanel(block, w, -yco);
			}
			else {
				yco = 0;
				uiEndPanel(block, w, 0);
			}

			uiEndBlock(C, block);
		}
		BLI_SMALLSTACK_ITER_END;

		/* align panels and return size */
		uiEndPanels(C, ar, &x, &y);
		
		/* before setting the view */
		if (vertical) {
			/* we always keep the scroll offset - so the total view gets increased with the scrolled away part */
			if (v2d->cur.ymax < - 0.001f)
				y = min_ii(y, v2d->cur.ymin);
			
			y = -y;
		}
		else {
			/* don't jump back when panels close or hide */
			if (!is_context_new)
				x = max_ii(x, v2d->cur.xmax);
			y = -y;
		}
		
		/* this also changes the 'cur' */
		UI_view2d_totRect_set(v2d, x, y);
		
		if (scroll != v2d->scroll) {
			/* Note: this code scales fine, but because of rounding differences, positions of elements
			 * flip +1 or -1 pixel compared to redoing the entire layout again.
			 * Leaving in commented code for future tests */
#if 0
			uiScalePanels(ar, BLI_rctf_size_x(&v2d->cur));
			break;
#endif
		}
		else {
			break;
		}
	}

	/* clear */
	if (ar->overlap) {
		/* view should be in pixelspace */
		UI_view2d_view_restore(C);
		glEnable(GL_BLEND);
		UI_ThemeColor4((ar->type->regionid == RGN_TYPE_PREVIEW) ? TH_PREVIEW_BACK : TH_BACK);
		glRecti(0, 0, BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));
		glDisable(GL_BLEND);
	}
	else {
		UI_ThemeClearColor((ar->type->regionid == RGN_TYPE_PREVIEW) ? TH_PREVIEW_BACK : TH_BACK);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	

	/* set the view */
	UI_view2d_view_ortho(v2d);

	/* draw panels */
	uiDrawPanels(C, ar);

	/* restore view matrix */
	UI_view2d_view_restore(C);
	
	if (use_category_tabs) {
		UI_panel_category_draw_all(ar, category);
	}

	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

void ED_region_panels_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_PANELS_UI, ar->winx, ar->winy);

	keymap = WM_keymap_find(wm->defaultconf, "View2D Buttons List", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

void ED_region_header(const bContext *C, ARegion *ar)
{
	uiStyle *style = UI_GetStyleDraw();
	uiBlock *block;
	uiLayout *layout;
	HeaderType *ht;
	Header header = {NULL};
	int maxco, xco, yco;
	int headery = ED_area_headersize();

	/* clear */
	UI_ThemeClearColor((ED_screen_area_active(C)) ? TH_HEADER : TH_HEADERDESEL);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(&ar->v2d);

	xco = maxco = 0.4f * UI_UNIT_X;
	yco = headery - floor(0.2f * UI_UNIT_Y);

	/* draw all headers types */
	for (ht = ar->type->headertypes.first; ht; ht = ht->next) {
		block = uiBeginBlock(C, ar, ht->idname, UI_EMBOSS);
		layout = uiBlockLayout(block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, yco, UI_UNIT_Y, 1, 0, style);

		if (ht->draw) {
			header.type = ht;
			header.layout = layout;
			ht->draw(C, &header);
			
			/* for view2d */
			xco = uiLayoutGetWidth(layout);
			if (xco > maxco)
				maxco = xco;
		}

		uiBlockLayoutResolve(block, &xco, &yco);
		
		/* for view2d */
		if (xco > maxco)
			maxco = xco;
		
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, maxco + UI_UNIT_X + 80, headery);

	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

void ED_region_header_init(ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

/* UI_UNIT_Y is defined as U variable now, depending dpi */
int ED_area_headersize(void)
{
	return (int)(HEADERY * UI_DPI_FAC);
}

void ED_region_info_draw(ARegion *ar, const char *text, int block, float fill_color[4])
{
	const int header_height = UI_UNIT_Y;
	uiStyle *style = UI_GetStyleDraw();
	int fontid = style->widget.uifont_id;
	GLint scissor[4];
	rcti rect;

	/* background box */
	ED_region_visible_rect(ar, &rect);
	rect.ymin = BLI_rcti_size_y(&ar->winrct) - header_height;

	/* box fill entire width or just around text */
	if (!block)
		rect.xmax = min_ii(rect.xmax, rect.xmin + BLF_width(fontid, text, BLF_DRAW_STR_DUMMY_MAX) + 1.2f * U.widget_unit);

	rect.ymax = BLI_rcti_size_y(&ar->winrct);

	/* setup scissor */
	glGetIntegerv(GL_SCISSOR_BOX, scissor);
	glScissor(ar->winrct.xmin + rect.xmin, ar->winrct.ymin + rect.ymin,
	          BLI_rcti_size_x(&rect) + 1, BLI_rcti_size_y(&rect) + 1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4fv(fill_color);
	glRecti(rect.xmin, rect.ymin, rect.xmax + 1, rect.ymax + 1);
	glDisable(GL_BLEND);

	/* text */
	UI_ThemeColor(TH_TEXT_HI);
	BLF_clipping(fontid, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	BLF_enable(fontid, BLF_CLIPPING);
	BLF_position(fontid, rect.xmin + 0.6f * U.widget_unit, rect.ymin + 0.3f * U.widget_unit, 0.0f);

	BLF_draw(fontid, text, BLF_DRAW_STR_DUMMY_MAX);

	BLF_disable(fontid, BLF_CLIPPING);

	/* restore scissor as it was before */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
}

void ED_region_grid_draw(ARegion *ar, float zoomx, float zoomy)
{
	float gridsize, gridstep = 1.0f / 32.0f;
	float fac, blendfac;
	int x1, y1, x2, y2;

	/* the image is located inside (0, 0), (1, 1) as set by view2d */
	UI_ThemeColorShade(TH_BACK, 20);

	UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x1, &y1);
	UI_view2d_view_to_region(&ar->v2d, 1.0f, 1.0f, &x2, &y2);
	glRectf(x1, y1, x2, y2);

	/* gridsize adapted to zoom level */
	gridsize = 0.5f * (zoomx + zoomy);
	if (gridsize <= 0.0f)
		return;

	if (gridsize < 1.0f) {
		while (gridsize < 1.0f) {
			gridsize *= 4.0f;
			gridstep *= 4.0f;
		}
	}
	else {
		while (gridsize >= 4.0f) {
			gridsize /= 4.0f;
			gridstep /= 4.0f;
		}
	}

	/* the fine resolution level */
	blendfac = 0.25f * gridsize - floorf(0.25f * gridsize);
	CLAMP(blendfac, 0.0f, 1.0f);
	UI_ThemeColorShade(TH_BACK, (int)(20.0f * (1.0f - blendfac)));

	fac = 0.0f;
	glBegin(GL_LINES);
	while (fac < 1.0f) {
		glVertex2f(x1, y1 * (1.0f - fac) + y2 * fac);
		glVertex2f(x2, y1 * (1.0f - fac) + y2 * fac);
		glVertex2f(x1 * (1.0f - fac) + x2 * fac, y1);
		glVertex2f(x1 * (1.0f - fac) + x2 * fac, y2);
		fac += gridstep;
	}

	/* the large resolution level */
	UI_ThemeColor(TH_BACK);

	fac = 0.0f;
	while (fac < 1.0f) {
		glVertex2f(x1, y1 * (1.0f - fac) + y2 * fac);
		glVertex2f(x2, y1 * (1.0f - fac) + y2 * fac);
		glVertex2f(x1 * (1.0f - fac) + x2 * fac, y1);
		glVertex2f(x1 * (1.0f - fac) + x2 * fac, y2);
		fac += 4.0f * gridstep;
	}
	glEnd();
}

/* If the area has overlapping regions, it returns visible rect for Region *ar */
/* rect gets returned in local region coordinates */
void ED_region_visible_rect(ARegion *ar, rcti *rect)
{
	ARegion *arn = ar;
	
	/* allow function to be called without area */
	while (arn->prev)
		arn = arn->prev;
	
	*rect = ar->winrct;
	
	/* check if a region overlaps with the current one */
	for (; arn; arn = arn->next) {
		if (ar != arn && arn->overlap) {
			if (BLI_rcti_isect(rect, &arn->winrct, NULL)) {
				
				/* overlap left, also check 1 pixel offset (2 regions on one side) */
				if (ABS(rect->xmin - arn->winrct.xmin) < 2)
					rect->xmin = arn->winrct.xmax;

				/* overlap right */
				if (ABS(rect->xmax - arn->winrct.xmax) < 2)
					rect->xmax = arn->winrct.xmin;
			}
		}
	}
	BLI_rcti_translate(rect, -ar->winrct.xmin, -ar->winrct.ymin);
}

/* Cache display helpers */

void ED_region_cache_draw_background(const ARegion *ar)
{
	glColor4ub(128, 128, 255, 64);
	glRecti(0, 0, ar->winx, 8 * UI_DPI_FAC);
}

void ED_region_cache_draw_curfra_label(const int framenr, const float x, const float y)
{
	uiStyle *style = UI_GetStyle();
	int fontid = style->widget.uifont_id;
	char numstr[32];
	float font_dims[2] = {0.0f, 0.0f};

	/* frame number */
	BLF_size(fontid, 11.0f * U.pixelsize, U.dpi);
	BLI_snprintf(numstr, sizeof(numstr), "%d", framenr);

	BLF_width_and_height(fontid, numstr, sizeof(numstr), &font_dims[0], &font_dims[1]);

	glRecti(x, y, x + font_dims[0] + 6.0f, y + font_dims[1] + 4.0f);

	UI_ThemeColor(TH_TEXT);
	BLF_position(fontid, x + 2.0f, y + 2.0f, 0.0f);
	BLF_draw(fontid, numstr, sizeof(numstr));
}

void ED_region_cache_draw_cached_segments(const ARegion *ar, const int num_segments, const int *points, const int sfra, const int efra)
{
	if (num_segments) {
		int a;

		glColor4ub(128, 128, 255, 128);

		for (a = 0; a < num_segments; a++) {
			float x1, x2;

			x1 = (float)(points[a * 2] - sfra) / (efra - sfra + 1) * ar->winx;
			x2 = (float)(points[a * 2 + 1] - sfra + 1) / (efra - sfra + 1) * ar->winx;

			glRecti(x1, 0, x2, 8 * UI_DPI_FAC);
		}
	}
}

