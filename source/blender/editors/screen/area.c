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
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"

#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_space_api.h"
#include "ED_types.h"
#include "ED_fileselect.h" 

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "screen_intern.h"

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
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	/* right  */
	glColor4ub(0,0,0, 30);
	sdrawline(rect.xmax, rect.ymin, rect.xmax, rect.ymax);
	
	/* bottom  */
	glColor4ub(0,0,0, 30);
	sdrawline(rect.xmin, rect.ymin, rect.xmax, rect.ymin);
	
	/* top  */
	glColor4ub(255,255,255, 30);
	sdrawline(rect.xmin, rect.ymax, rect.xmax, rect.ymax);

	/* left  */
	glColor4ub(255,255,255, 30);
	sdrawline(rect.xmin, rect.ymin, rect.xmin, rect.ymax);
	
	glDisable( GL_BLEND );
}

void ED_region_pixelspace(ARegion *ar)
{
	int width= ar->winrct.xmax-ar->winrct.xmin+1;
	int height= ar->winrct.ymax-ar->winrct.ymin+1;
	
	wmOrtho2(-0.375f, (float)width-0.375f, -0.375f, (float)height-0.375f);
	glLoadIdentity();
}

/* only exported for WM */
void ED_region_do_listen(ARegion *ar, wmNotifier *note)
{
	/* generic notes first */
	switch(note->category) {
		case NC_WM:
			if (note->data==ND_FILEREAD)
				ED_region_tag_redraw(ar);
			break;
		case NC_WINDOW:
			ED_region_tag_redraw(ar);
			break;
	}

	if (ar->type && ar->type->listener)
		ar->type->listener(ar, note);
}

/* only exported for WM */
void ED_area_do_listen(ScrArea *sa, wmNotifier *note)
{
	/* no generic notes? */
	if (sa->type && sa->type->listener) {
		sa->type->listener(sa, note);
	}
}

/* only exported for WM */
void ED_area_do_refresh(bContext *C, ScrArea *sa)
{
	/* no generic notes? */
	if (sa->type && sa->type->refresh) {
		sa->type->refresh(C, sa);
	}
	sa->do_refresh= 0;
}

/* based on screen region draw tags, set draw tags in azones, and future region tabs etc */
/* only exported for WM */
void ED_area_overdraw_flush(ScrArea *sa, ARegion *ar)
{
	AZone *az;
	
	for (az= sa->actionzones.first; az; az= az->next) {
		int xs, ys;
		
		xs= (az->x1+az->x2)/2;
		ys= (az->y1+az->y2)/2;

		/* test if inside */
		if (BLI_in_rcti(&ar->winrct, xs, ys)) {
			az->do_draw= 1;
		}
	}
}

static void area_draw_azone(short x1, short y1, short x2, short y2)
{
	int dx = x2 - x1;
	int dy = y2 - y1;

	dx= copysign(ceil(0.3f*fabs(dx)), dx);
	dy= copysign(ceil(0.3f*fabs(dy)), dy);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	
	glColor4ub(255, 255, 255, 180);
	fdrawline(x1, y2, x2, y1);
	glColor4ub(255, 255, 255, 130);
	fdrawline(x1, y2-dy, x2-dx, y1);
	glColor4ub(255, 255, 255, 80);
	fdrawline(x1, y2-2*dy, x2-2*dx, y1);
	
	glColor4ub(0, 0, 0, 210);
	fdrawline(x1, y2+1, x2+1, y1);
	glColor4ub(0, 0, 0, 180);
	fdrawline(x1, y2-dy+1, x2-dx+1, y1);
	glColor4ub(0, 0, 0, 150);
	fdrawline(x1, y2-2*dy+1, x2-2*dx+1, y1);

	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

static void region_draw_azone_icon(AZone *az)
{
	GLUquadricObj *qobj = NULL; 
	short midx = az->x1 + (az->x2 - az->x1)/2;
	short midy = az->y1 + (az->y2 - az->y1)/2;
		
	qobj = gluNewQuadric();
	
	glPushMatrix();
	glTranslatef(midx, midy, 0.0);
	
	/* outlined circle */
	glEnable(GL_LINE_SMOOTH);

	glColor4f(1.f, 1.f, 1.f, 0.8f);

	gluQuadricDrawStyle(qobj, GLU_FILL); 
	gluDisk( qobj, 0.0,  4.25f, 16, 1); 

	glColor4f(0.2f, 0.2f, 0.2f, 0.9f);
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	gluDisk( qobj, 0.0,  4.25f, 16, 1); 
	
	glDisable(GL_LINE_SMOOTH);
	
	glPopMatrix();
	gluDeleteQuadric(qobj);
	
	/* + */
	sdrawline(midx, midy-2, midx, midy+3);
	sdrawline(midx-2, midy, midx+3, midy);
}

static void draw_azone_plus(float x1, float y1, float x2, float y2)
{
	float width = 2.0f;
	float pad = 4.0f;
	
	glRectf((x1 + x2 - width)*0.5f, y1 + pad, (x1 + x2 + width)*0.5f, y2 - pad);
	glRectf(x1 + pad, (y1 + y2 - width)*0.5f, (x1 + x2 - width)*0.5f, (y1 + y2 + width)*0.5f);
	glRectf((x1 + x2 + width)*0.5f, (y1 + y2 - width)*0.5f, x2 - pad, (y1 + y2 + width)*0.5f);
}

static void region_draw_azone_tab_plus(AZone *az)
{
	extern void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3); /* xxx temp */
	
	glEnable(GL_BLEND);
	
	/* add code to draw region hidden as 'too small' */
	switch(az->edge) {
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
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			uiSetRoundBox(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, 0.3f+(float)az->y1, (float)az->x2, 0.3f+(float)az->y2, 4.0f);
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			uiSetRoundBox(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT | UI_RB_ALPHA);
			
			uiDrawBoxShade(GL_POLYGON, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, -0.3f, 0.05f);
			glColor4ub(0, 0, 0, 255);
			uiRoundRect((float)az->x1, 0.3f+(float)az->y1, (float)az->x2, 0.3f+(float)az->y2, 4.0f);
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
	extern void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3); /* xxx temp */
	
	glEnable(GL_BLEND);
	//UI_GetThemeColor3fv(TH_HEADER, col);
	glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
	
	/* add code to draw region hidden as 'too small' */
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			ui_draw_anti_tria((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y1, (float)(az->x1+az->x2)/2, (float)az->y2);
			break;
			
		case AE_BOTTOM_TO_TOPLEFT:
			ui_draw_anti_tria((float)az->x1, (float)az->y2, (float)az->x2, (float)az->y2, (float)(az->x1+az->x2)/2, (float)az->y1);
			break;

		case AE_LEFT_TO_TOPRIGHT:
			ui_draw_anti_tria((float)az->x2, (float)az->y1, (float)az->x2, (float)az->y2, (float)az->x1, (float)(az->y1+az->y2)/2);
			break;
			
		case AE_RIGHT_TO_TOPLEFT:
			ui_draw_anti_tria((float)az->x1, (float)az->y1, (float)az->x1, (float)az->y2, (float)az->x2, (float)(az->y1+az->y2)/2);
			break;
			
	}
	
	glDisable(GL_BLEND);
}

/* only exported for WM */
void ED_area_overdraw(bContext *C)
{
	wmWindow *win= CTX_wm_window(C);
	bScreen *screen= CTX_wm_screen(C);
	ScrArea *sa;
	
	/* Draw AZones, in screenspace */
	wmSubWindowSet(win, screen->mainwin);

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	for (sa= screen->areabase.first; sa; sa= sa->next) {
		AZone *az;
		for (az= sa->actionzones.first; az; az= az->next) {
			if (az->do_draw) {
				if (az->type==AZONE_AREA) {
					area_draw_azone(az->x1, az->y1, az->x2, az->y2);
				}
				else if (az->type==AZONE_REGION) {
					
					if (az->ar) {
						/* only display tab or icons when the region is hidden */
						if (az->ar->flag & (RGN_FLAG_HIDDEN|RGN_FLAG_TOO_SMALL)) {
							if (G.rt==3)
								region_draw_azone_icon(az);
							else if (G.rt==2)
								region_draw_azone_tria(az);
							else if (G.rt==1)
								region_draw_azone_tab(az);
							else
								region_draw_azone_tab_plus(az);
						}
					}
				}
				
				az->do_draw= 0;
			}
		}
	}	
	glDisable( GL_BLEND );
	
}

/* get scissor rect, checking overlapping regions */
void region_scissor_winrct(ARegion *ar, rcti *winrct)
{
	*winrct= ar->winrct;
	
	if (ELEM(ar->alignment, RGN_OVERLAP_LEFT, RGN_OVERLAP_RIGHT))
		return;

	while (ar->prev) {
		ar= ar->prev;
		
		if (BLI_isect_rcti(winrct, &ar->winrct, NULL)) {
			if (ar->flag & RGN_FLAG_HIDDEN);
			else if (ar->alignment & RGN_SPLIT_PREV);
			else if (ar->alignment==RGN_OVERLAP_LEFT) {
				winrct->xmin = ar->winrct.xmax + 1;
			}
			else if (ar->alignment==RGN_OVERLAP_RIGHT) {
				winrct->xmax = ar->winrct.xmin - 1;
			}
			else break;
		}
	}
}

/* only exported for WM */
/* makes region ready for drawing, sets pixelspace */
void ED_region_set(const bContext *C, ARegion *ar)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *sa= CTX_wm_area(C);
	rcti winrct;
	
	/* checks other overlapping regions */
	region_scissor_winrct(ar, &winrct);
	
	ar->drawrct= winrct;
	
	/* note; this sets state, so we can use wmOrtho and friends */
	wmSubWindowScissorSet(win, ar->swinid, &ar->drawrct);
	
	UI_SetTheme(sa?sa->spacetype:0, ar->type?ar->type->regionid:0);
	
	ED_region_pixelspace(ar);
}


/* only exported for WM */
void ED_region_do_draw(bContext *C, ARegion *ar)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *sa= CTX_wm_area(C);
	ARegionType *at= ar->type;
	rcti winrct;
	
	/* see BKE_spacedata_draw_locks() */
	if (at->do_lock)
		return;
	
	/* checks other overlapping regions */
	region_scissor_winrct(ar, &winrct);
	
	/* if no partial draw rect set, full rect */
	if (ar->drawrct.xmin == ar->drawrct.xmax)
		ar->drawrct= winrct;
	else {
		/* extra clip for safety */
		ar->drawrct.xmin = MAX2(winrct.xmin, ar->drawrct.xmin);
		ar->drawrct.ymin = MAX2(winrct.ymin, ar->drawrct.ymin);
		ar->drawrct.xmax = MIN2(winrct.xmax, ar->drawrct.xmax);
		ar->drawrct.ymax = MIN2(winrct.ymax, ar->drawrct.ymax);
	}
	
	/* note; this sets state, so we can use wmOrtho and friends */
	wmSubWindowScissorSet(win, ar->swinid, &ar->drawrct);
	
	UI_SetTheme(sa?sa->spacetype:0, ar->type?ar->type->regionid:0);
	
	/* optional header info instead? */
	if (ar->headerstr) {
		UI_ThemeClearColor(TH_HEADER);
		glClear(GL_COLOR_BUFFER_BIT);
		
		UI_ThemeColor(TH_TEXT);
		BLF_draw_default(20, 8, 0.0f, ar->headerstr, BLF_DRAW_STR_DUMMY_MAX);
	}
	else if (at->draw) {
		at->draw(C, ar);
	}

	/* XXX test: add convention to end regions always in pixel space, for drawing of borders/gestures etc */
	ED_region_pixelspace(ar);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_PIXEL);

	ar->do_draw= 0;
	memset(&ar->drawrct, 0, sizeof(ar->drawrct));
	
	uiFreeInactiveBlocks(C, &ar->uiblocks);

	if (sa)
		region_draw_emboss(ar, &winrct);
}

/* **********************************
 * maybe silly, but let's try for now
 * to keep these tags protected
 * ********************************** */

void ED_region_tag_redraw(ARegion *ar)
{
	if (ar) {
		/* zero region means full region redraw */
		ar->do_draw= RGN_DRAW;
		memset(&ar->drawrct, 0, sizeof(ar->drawrct));
	}
}

void ED_region_tag_redraw_overlay(ARegion *ar)
{
	if (ar)
		ar->do_draw_overlay= RGN_DRAW;
}

void ED_region_tag_redraw_partial(ARegion *ar, rcti *rct)
{
	if (ar) {
		if (!ar->do_draw) {
			/* no redraw set yet, set partial region */
			ar->do_draw= RGN_DRAW_PARTIAL;
			ar->drawrct= *rct;
		}
		else if (ar->drawrct.xmin != ar->drawrct.xmax) {
			/* partial redraw already set, expand region */
			ar->drawrct.xmin = MIN2(ar->drawrct.xmin, rct->xmin);
			ar->drawrct.ymin = MIN2(ar->drawrct.ymin, rct->ymin);
			ar->drawrct.xmax = MAX2(ar->drawrct.xmax, rct->xmax);
			ar->drawrct.ymax = MAX2(ar->drawrct.ymax, rct->ymax);
		}
	}
}

void ED_area_tag_redraw(ScrArea *sa)
{
	ARegion *ar;
	
	if (sa)
		for (ar= sa->regionbase.first; ar; ar= ar->next)
			ED_region_tag_redraw(ar);
}

void ED_area_tag_redraw_regiontype(ScrArea *sa, int regiontype)
{
	ARegion *ar;
	
	if (sa) {
		for (ar= sa->regionbase.first; ar; ar= ar->next) {
			if (ar->regiontype == regiontype) {
				ED_region_tag_redraw(ar);
			}
		}
	}
}

void ED_area_tag_refresh(ScrArea *sa)
{
	if (sa)
		sa->do_refresh= 1;
}

/* *************************************************************** */

/* use NULL to disable it */
void ED_area_headerprint(ScrArea *sa, const char *str)
{
	ARegion *ar;

	/* happens when running transform operators in backround mode */
	if (sa == NULL)
		return;

	for (ar= sa->regionbase.first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_HEADER) {
			if (str) {
				if (ar->headerstr==NULL)
					ar->headerstr= MEM_mallocN(256, "headerprint");
				BLI_strncpy(ar->headerstr, str, 256);
			}
			else if (ar->headerstr) {
				MEM_freeN(ar->headerstr);
				ar->headerstr= NULL;
			}
			ED_region_tag_redraw(ar);
		}
	}
}

/* ************************************************************ */


static void area_azone_initialize(ScrArea *sa) 
{
	AZone *az;
	
	/* reinitalize entirely, regions add azones too */
	BLI_freelistN(&sa->actionzones);
	
	/* set area action zones */
	az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
	BLI_addtail(&(sa->actionzones), az);
	az->type= AZONE_AREA;
	az->x1= sa->totrct.xmin - 1;
	az->y1= sa->totrct.ymin - 1;
	az->x2= sa->totrct.xmin + (AZONESPOT-1);
	az->y2= sa->totrct.ymin + (AZONESPOT-1);
	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
	
	az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
	BLI_addtail(&(sa->actionzones), az);
	az->type= AZONE_AREA;
	az->x1= sa->totrct.xmax + 1;
	az->y1= sa->totrct.ymax + 1;
	az->x2= sa->totrct.xmax - (AZONESPOT-1);
	az->y2= sa->totrct.ymax - (AZONESPOT-1);
	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

#define AZONEPAD_EDGE	4
#define AZONEPAD_ICON	9
static void region_azone_edge(AZone *az, ARegion *ar)
{
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			az->x1= ar->winrct.xmin;
			az->y1= ar->winrct.ymax - AZONEPAD_EDGE;
			az->x2= ar->winrct.xmax;
			az->y2= ar->winrct.ymax;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1= ar->winrct.xmin;
			az->y1= ar->winrct.ymin + AZONEPAD_EDGE;
			az->x2= ar->winrct.xmax;
			az->y2= ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1= ar->winrct.xmin;
			az->y1= ar->winrct.ymin;
			az->x2= ar->winrct.xmin + AZONEPAD_EDGE;
			az->y2= ar->winrct.ymax;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1= ar->winrct.xmax;
			az->y1= ar->winrct.ymin;
			az->x2= ar->winrct.xmax - AZONEPAD_EDGE;
			az->y2= ar->winrct.ymax;
			break;
	}

	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

static void region_azone_icon(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot=0;
	
	/* count how many actionzones with along same edge are available.
	 * This allows for adding more action zones in the future without
	 * having to worry about correct offset */
	for (azt= sa->actionzones.first; azt; azt= azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			az->x1= ar->winrct.xmax - tot*2*AZONEPAD_ICON;
			az->y1= ar->winrct.ymax + AZONEPAD_ICON;
			az->x2= ar->winrct.xmax - tot*AZONEPAD_ICON;
			az->y2= ar->winrct.ymax + 2*AZONEPAD_ICON;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1= ar->winrct.xmin + AZONEPAD_ICON;
			az->y1= ar->winrct.ymin - 2*AZONEPAD_ICON;
			az->x2= ar->winrct.xmin + 2*AZONEPAD_ICON;
			az->y2= ar->winrct.ymin - AZONEPAD_ICON;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1= ar->winrct.xmin - 2*AZONEPAD_ICON;
			az->y1= ar->winrct.ymax - tot*2*AZONEPAD_ICON;
			az->x2= ar->winrct.xmin - AZONEPAD_ICON;
			az->y2= ar->winrct.ymax - tot*AZONEPAD_ICON;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1= ar->winrct.xmax + AZONEPAD_ICON;
			az->y1= ar->winrct.ymax - tot*2*AZONEPAD_ICON;
			az->x2= ar->winrct.xmax + 2*AZONEPAD_ICON;
			az->y2= ar->winrct.ymax - tot*AZONEPAD_ICON;
			break;
	}

	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
	
	/* if more azones on 1 spot, set offset */
	for (azt= sa->actionzones.first; azt; azt= azt->next) {
		if (az!=azt) {
			if ( ABS(az->x1-azt->x1) < 2 && ABS(az->y1-azt->y1) < 2) {
				if (az->edge==AE_TOP_TO_BOTTOMRIGHT || az->edge==AE_BOTTOM_TO_TOPLEFT) {
					az->x1+= AZONESPOT;
					az->x2+= AZONESPOT;
				}
				else {
					az->y1-= AZONESPOT;
					az->y2-= AZONESPOT;
				}
				BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
			}
		}
	}
}

#define AZONEPAD_TAB_PLUSW	14
#define AZONEPAD_TAB_PLUSH	14

/* region already made zero sized, in shape of edge */
static void region_azone_tab_plus(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot= 0, add;
	
	for (azt= sa->actionzones.first; azt; azt= azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add= 1; else add= 0;
			az->x1= ar->winrct.xmax - 2.5*AZONEPAD_TAB_PLUSW;
			az->y1= ar->winrct.ymax - add;
			az->x2= ar->winrct.xmax - 1.5*AZONEPAD_TAB_PLUSW;
			az->y2= ar->winrct.ymax - add + AZONEPAD_TAB_PLUSH;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1= ar->winrct.xmax - 2.5*AZONEPAD_TAB_PLUSW;
			az->y1= ar->winrct.ymin - AZONEPAD_TAB_PLUSH;
			az->x2= ar->winrct.xmax - 1.5*AZONEPAD_TAB_PLUSW;
			az->y2= ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1= ar->winrct.xmin - AZONEPAD_TAB_PLUSH;
			az->y1= ar->winrct.ymax - 2.5*AZONEPAD_TAB_PLUSW;
			az->x2= ar->winrct.xmin;
			az->y2= ar->winrct.ymax - 1.5*AZONEPAD_TAB_PLUSW;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1= ar->winrct.xmax - 1;
			az->y1= ar->winrct.ymax - 2.5*AZONEPAD_TAB_PLUSW;
			az->x2= ar->winrct.xmax - 1 + AZONEPAD_TAB_PLUSH;
			az->y2= ar->winrct.ymax - 1.5*AZONEPAD_TAB_PLUSW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	


#define AZONEPAD_TABW	18
#define AZONEPAD_TABH	7

/* region already made zero sized, in shape of edge */
static void region_azone_tab(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot= 0, add;
	
	for (azt= sa->actionzones.first; azt; azt= azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add= 1; else add= 0;
			az->x1= ar->winrct.xmax - 2*AZONEPAD_TABW;
			az->y1= ar->winrct.ymax - add;
			az->x2= ar->winrct.xmax - AZONEPAD_TABW;
			az->y2= ar->winrct.ymax - add + AZONEPAD_TABH;
			break;
		case AE_BOTTOM_TO_TOPLEFT:
			az->x1= ar->winrct.xmin + AZONEPAD_TABW;
			az->y1= ar->winrct.ymin - AZONEPAD_TABH;
			az->x2= ar->winrct.xmin + 2*AZONEPAD_TABW;
			az->y2= ar->winrct.ymin;
			break;
		case AE_LEFT_TO_TOPRIGHT:
			az->x1= ar->winrct.xmin + 1 - AZONEPAD_TABH;
			az->y1= ar->winrct.ymax - 2*AZONEPAD_TABW;
			az->x2= ar->winrct.xmin + 1;
			az->y2= ar->winrct.ymax - AZONEPAD_TABW;
			break;
		case AE_RIGHT_TO_TOPLEFT:
			az->x1= ar->winrct.xmax - 1;
			az->y1= ar->winrct.ymax - 2*AZONEPAD_TABW;
			az->x2= ar->winrct.xmax - 1 + AZONEPAD_TABH;
			az->y2= ar->winrct.ymax - AZONEPAD_TABW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	

#define AZONEPAD_TRIAW	16
#define AZONEPAD_TRIAH	9


/* region already made zero sized, in shape of edge */
static void region_azone_tria(ScrArea *sa, AZone *az, ARegion *ar)
{
	AZone *azt;
	int tot= 0, add;
	
	for (azt= sa->actionzones.first; azt; azt= azt->next) {
		if (azt->edge == az->edge) tot++;
	}
	
	switch(az->edge) {
		case AE_TOP_TO_BOTTOMRIGHT:
			if (ar->winrct.ymax == sa->totrct.ymin) add= 1; else add= 0;
			az->x1= ar->winrct.xmax - 2*AZONEPAD_TRIAW;
			az->y1= ar->winrct.ymax - add;
			az->x2= ar->winrct.xmax - AZONEPAD_TRIAW;
			az->y2= ar->winrct.ymax - add + AZONEPAD_TRIAH;
			break;
			case AE_BOTTOM_TO_TOPLEFT:
			az->x1= ar->winrct.xmin + AZONEPAD_TRIAW;
			az->y1= ar->winrct.ymin - AZONEPAD_TRIAH;
			az->x2= ar->winrct.xmin + 2*AZONEPAD_TRIAW;
			az->y2= ar->winrct.ymin;
			break;
			case AE_LEFT_TO_TOPRIGHT:
			az->x1= ar->winrct.xmin + 1 - AZONEPAD_TRIAH;
			az->y1= ar->winrct.ymax - 2*AZONEPAD_TRIAW;
			az->x2= ar->winrct.xmin + 1;
			az->y2= ar->winrct.ymax - AZONEPAD_TRIAW;
			break;
			case AE_RIGHT_TO_TOPLEFT:
			az->x1= ar->winrct.xmax - 1;
			az->y1= ar->winrct.ymax - 2*AZONEPAD_TRIAW;
			az->x2= ar->winrct.xmax - 1 + AZONEPAD_TRIAH;
			az->y2= ar->winrct.ymax - AZONEPAD_TRIAW;
			break;
	}
	/* rect needed for mouse pointer test */
	BLI_init_rcti(&az->rect, az->x1, az->x2, az->y1, az->y2);
}	


static void region_azone_initialize(ScrArea *sa, ARegion *ar, AZEdge edge) 
{
	AZone *az;
	
	az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
	BLI_addtail(&(sa->actionzones), az);
	az->type= AZONE_REGION;
	az->ar= ar;
	az->edge= edge;
	
	if (ar->flag & (RGN_FLAG_HIDDEN|RGN_FLAG_TOO_SMALL)) {
		if (G.rt==3)
			region_azone_icon(sa, az, ar);
		else if (G.rt==2)
			region_azone_tria(sa, az, ar);
		else if (G.rt==1)
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
	
	if (alignment==RGN_ALIGN_TOP)
		region_azone_initialize(sa, ar, AE_BOTTOM_TO_TOPLEFT);
	else if (alignment==RGN_ALIGN_BOTTOM)
		region_azone_initialize(sa, ar, AE_TOP_TO_BOTTOMRIGHT);
	else if (ELEM(alignment, RGN_ALIGN_RIGHT, RGN_OVERLAP_RIGHT))
		region_azone_initialize(sa, ar, AE_LEFT_TO_TOPRIGHT);
	else if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_OVERLAP_LEFT))
		region_azone_initialize(sa, ar, AE_RIGHT_TO_TOPLEFT);
}

/* dir is direction to check, not the splitting edge direction! */
static int rct_fits(rcti *rect, char dir, int size)
{
	if (dir=='h') {
		return rect->xmax-rect->xmin - size;
	}
	else { // 'v'
		return rect->ymax-rect->ymin - size;
	}
}

static void region_rect_recursive(ScrArea *sa, ARegion *ar, rcti *remainder, int quad)
{
	rcti *remainder_prev= remainder;
	int prefsizex, prefsizey;
	int alignment;
	
	if (ar==NULL)
		return;
	
	/* no returns in function, winrct gets set in the end again */
	BLI_init_rcti(&ar->winrct, 0, 0, 0, 0);
	
	/* for test; allow split of previously defined region */
	if (ar->alignment & RGN_SPLIT_PREV)
		if (ar->prev)
			remainder= &ar->prev->winrct;
	
	alignment = ar->alignment & ~RGN_SPLIT_PREV;
	
	/* clear state flags first */
	ar->flag &= ~RGN_FLAG_TOO_SMALL;
	/* user errors */
	if (ar->next==NULL && alignment!=RGN_ALIGN_QSPLIT)
		alignment= RGN_ALIGN_NONE;
	
	/* prefsize, for header we stick to exception */
	prefsizex= ar->sizex?ar->sizex:ar->type->prefsizex;
	if (ar->regiontype==RGN_TYPE_HEADER)
		prefsizey= ar->type->prefsizey;
	else if (ar->regiontype==RGN_TYPE_UI && sa->spacetype == SPACE_FILE) {
		prefsizey= UI_UNIT_Y * 2 + (UI_UNIT_Y/2);
	}
	else
		prefsizey= ar->sizey?ar->sizey:ar->type->prefsizey;
	
	/* hidden is user flag */
	if (ar->flag & RGN_FLAG_HIDDEN);
	/* XXX floating area region, not handled yet here */
	else if (alignment == RGN_ALIGN_FLOAT);
	/* remainder is too small for any usage */
	else if ( rct_fits(remainder, 'v', 1)<0 || rct_fits(remainder, 'h', 1) < 0 ) {
		ar->flag |= RGN_FLAG_TOO_SMALL;
	}
	else if (alignment==RGN_ALIGN_NONE) {
		/* typically last region */
		ar->winrct= *remainder;
		BLI_init_rcti(remainder, 0, 0, 0, 0);
	}
	else if (alignment==RGN_ALIGN_TOP || alignment==RGN_ALIGN_BOTTOM) {
		
		if ( rct_fits(remainder, 'v', prefsizey) < 0 ) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac= rct_fits(remainder, 'v', prefsizey);

			if (fac < 0 )
				prefsizey += fac;
			
			ar->winrct= *remainder;
			
			if (alignment==RGN_ALIGN_TOP) {
				ar->winrct.ymin = ar->winrct.ymax - prefsizey + 1;
				remainder->ymax = ar->winrct.ymin - 1;
			}
			else {
				ar->winrct.ymax = ar->winrct.ymin + prefsizey - 1;
				remainder->ymin = ar->winrct.ymax + 1;
			}
		}
	}
	else if ( ELEM4(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT, RGN_OVERLAP_LEFT, RGN_OVERLAP_RIGHT)) {
		
		if ( rct_fits(remainder, 'h', prefsizex) < 0 ) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac= rct_fits(remainder, 'h', prefsizex);
			
			if (fac < 0 )
				prefsizex += fac;
			
			ar->winrct= *remainder;
			
			if (ELEM(alignment, RGN_ALIGN_RIGHT, RGN_OVERLAP_RIGHT)) {
				ar->winrct.xmin = ar->winrct.xmax - prefsizex + 1;
				if (alignment==RGN_ALIGN_RIGHT)
					remainder->xmax = ar->winrct.xmin - 1;
			}
			else {
				ar->winrct.xmax = ar->winrct.xmin + prefsizex - 1;
				if (alignment==RGN_ALIGN_LEFT)
					remainder->xmin = ar->winrct.xmax + 1;
			}
		}
	}
	else if (alignment==RGN_ALIGN_VSPLIT || alignment==RGN_ALIGN_HSPLIT) {
		/* percentage subdiv*/
		ar->winrct= *remainder;
		
		if (alignment==RGN_ALIGN_HSPLIT) {
			if ( rct_fits(remainder, 'h', prefsizex) > 4) {
				ar->winrct.xmax = (remainder->xmin+remainder->xmax)/2;
				remainder->xmin = ar->winrct.xmax+1;
			}
			else {
				BLI_init_rcti(remainder, 0, 0, 0, 0);
			}
		}
		else {
			if ( rct_fits(remainder, 'v', prefsizey) > 4) {
				ar->winrct.ymax = (remainder->ymin+remainder->ymax)/2;
				remainder->ymin = ar->winrct.ymax+1;
			}
			else {
				BLI_init_rcti(remainder, 0, 0, 0, 0);
			}
		}
	}
	else if (alignment==RGN_ALIGN_QSPLIT) {
		ar->winrct= *remainder;
		
		/* test if there's still 4 regions left */
		if (quad==0) {
			ARegion *artest= ar->next;
			int count= 1;
			
			while (artest) {
				artest->alignment= RGN_ALIGN_QSPLIT;
				artest= artest->next;
				count++;
			}
			
			if (count!=4) {
				/* let's stop adding regions */
				BLI_init_rcti(remainder, 0, 0, 0, 0);
				if (G.debug & G_DEBUG)
					printf("region quadsplit failed\n");
			}
			else quad= 1;
		}
		if (quad) {
			if (quad==1) { /* left bottom */
				ar->winrct.xmax = (remainder->xmin + remainder->xmax)/2;
				ar->winrct.ymax = (remainder->ymin + remainder->ymax)/2;
			}
			else if (quad==2) { /* left top */
				ar->winrct.xmax = (remainder->xmin + remainder->xmax)/2;
				ar->winrct.ymin = 1 + (remainder->ymin + remainder->ymax)/2;
			}
			else if (quad==3) { /* right bottom */
				ar->winrct.xmin = 1 + (remainder->xmin + remainder->xmax)/2;
				ar->winrct.ymax = (remainder->ymin + remainder->ymax)/2;
			}
			else {	/* right top */
				ar->winrct.xmin = 1 + (remainder->xmin + remainder->xmax)/2;
				ar->winrct.ymin = 1 + (remainder->ymin + remainder->ymax)/2;
				BLI_init_rcti(remainder, 0, 0, 0, 0);
			}

			quad++;
		}
	}
	
	/* for speedup */
	ar->winx= ar->winrct.xmax - ar->winrct.xmin + 1;
	ar->winy= ar->winrct.ymax - ar->winrct.ymin + 1;
	
	/* set winrect for azones */
	if (ar->flag & (RGN_FLAG_HIDDEN|RGN_FLAG_TOO_SMALL)) {
		ar->winrct= *remainder;
		
		if (alignment==RGN_ALIGN_TOP)
			ar->winrct.ymin = ar->winrct.ymax;
		else if (alignment==RGN_ALIGN_BOTTOM)
			ar->winrct.ymax = ar->winrct.ymin;
		else if (ELEM(alignment, RGN_ALIGN_RIGHT, RGN_OVERLAP_RIGHT))
			ar->winrct.xmin = ar->winrct.xmax;
		else if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_OVERLAP_LEFT))
			ar->winrct.xmax = ar->winrct.xmin;
		else /* prevent winrct to be valid */
			ar->winrct.xmax = ar->winrct.xmin;
	}

	/* restore prev-split exception */
	if (ar->alignment & RGN_SPLIT_PREV) {
		if (ar->prev) {
			remainder= remainder_prev;
			ar->prev->winx= ar->prev->winrct.xmax - ar->prev->winrct.xmin + 1;
			ar->prev->winy= ar->prev->winrct.ymax - ar->prev->winrct.ymin + 1;
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

	region_rect_recursive(sa, ar->next, remainder, quad);
}

static void area_calc_totrct(ScrArea *sa, int sizex, int sizey)
{
	short rt= 0; // CLAMPIS(G.rt, 0, 16);

	if (sa->v1->vec.x>0) sa->totrct.xmin = sa->v1->vec.x+1+rt;
	else sa->totrct.xmin = sa->v1->vec.x;
	if (sa->v4->vec.x<sizex-1) sa->totrct.xmax = sa->v4->vec.x-1-rt;
	else sa->totrct.xmax = sa->v4->vec.x;
	
	if (sa->v1->vec.y>0) sa->totrct.ymin = sa->v1->vec.y+1+rt;
	else sa->totrct.ymin = sa->v1->vec.y;
	if (sa->v2->vec.y<sizey-1) sa->totrct.ymax = sa->v2->vec.y-1-rt;
	else sa->totrct.ymax = sa->v2->vec.y;
	
	/* for speedup */
	sa->winx= sa->totrct.xmax-sa->totrct.xmin+1;
	sa->winy= sa->totrct.ymax-sa->totrct.ymin+1;
}


/* used for area initialize below */
static void region_subwindow(wmWindow *win, ARegion *ar)
{
	if (ar->flag & (RGN_FLAG_HIDDEN|RGN_FLAG_TOO_SMALL)) {
		if (ar->swinid)
			wm_subwindow_close(win, ar->swinid);
		ar->swinid= 0;
	}
	else if (ar->swinid==0)
		ar->swinid= wm_subwindow_open(win, &ar->winrct);
	else 
		wm_subwindow_position(win, ar->swinid, &ar->winrct);
}

static void ed_default_handlers(wmWindowManager *wm, ScrArea *sa, ListBase *handlers, int flag)
{
	/* note, add-handler checks if it already exists */
	
	// XXX it would be good to have boundbox checks for some of these...
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
		if (sa->spacetype!=SPACE_TIME) {
			ARegion *ar;
			static rcti rect= {0, 10000, 0, 30};	/* same local check for all areas */
			ar= BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
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
	sa->type= BKE_spacetype_from_id(sa->spacetype);
	
	if (sa->type==NULL) {
		sa->butspacetype= sa->spacetype= SPACE_VIEW3D;
		sa->type= BKE_spacetype_from_id(sa->spacetype);
	}
	
	for (ar= sa->regionbase.first; ar; ar= ar->next)
		ar->type= BKE_regiontype_from_id(sa->type, ar->regiontype);
	
	/* area sizes */
	area_calc_totrct(sa, win->sizex, win->sizey);
	
	/* clear all azones, add the area triange widgets */
	area_azone_initialize(sa);

	/* region rect sizes */
	rect= sa->totrct;
	region_rect_recursive(sa, sa->regionbase.first, &rect, 0);
	
	/* default area handlers */
	ed_default_handlers(wm, sa, &sa->handlers, sa->type->keymapflag);
	/* checks spacedata, adds own handlers */
	if (sa->type->init)
		sa->type->init(wm, sa);
	
	/* region windows, default and own handlers */
	for (ar= sa->regionbase.first; ar; ar= ar->next) {
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
		
		/* rechecks 2d matrix for header on dpi changing, do not do for other regions, it resets view && blocks view2d operator polls (ton) */
		if (ar->regiontype==RGN_TYPE_HEADER)
			ar->v2d.flag &= ~V2D_IS_INITIALISED;
	}
}

/* externally called for floating regions like menus */
void ED_region_init(bContext *C, ARegion *ar)
{
//	ARegionType *at= ar->type;
	
	/* refresh can be called before window opened */
	region_subwindow(CTX_wm_window(C), ar);
	
	ar->winx= ar->winrct.xmax - ar->winrct.xmin + 1;
	ar->winy= ar->winrct.ymax - ar->winrct.ymin + 1;
	
	/* UI convention */
	wmOrtho2(-0.01f, ar->winx-0.01f, -0.01f, ar->winy-0.01f);
	glLoadIdentity();
}

void ED_region_toggle_hidden(bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);

	ar->flag ^= RGN_FLAG_HIDDEN;

	if (ar->flag & RGN_FLAG_HIDDEN)
		WM_event_remove_handlers(C, &ar->handlers);

	ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
	ED_area_tag_redraw(sa);
}

/* sa2 to sa1, we swap spaces for fullscreen to keep all allocated data */
/* area vertices were set */
void area_copy_data(ScrArea *sa1, ScrArea *sa2, int swap_space)
{
	SpaceType *st;
	ARegion *ar;
	int spacetype= sa1->spacetype;
	
	sa1->headertype= sa2->headertype;
	sa1->spacetype= sa2->spacetype;
	sa1->butspacetype= sa2->butspacetype;
	
	if (swap_space == 1) {
		SWAP(ListBase, sa1->spacedata, sa2->spacedata);
		/* exception: ensure preview is reset */
//		if (sa1->spacetype==SPACE_VIEW3D)
// XXX			BIF_view3d_previewrender_free(sa1->spacedata.first);
	}
	else if (swap_space == 2) {
		BKE_spacedata_copylist(&sa1->spacedata, &sa2->spacedata);
	}
	else {
		BKE_spacedata_freelist(&sa1->spacedata);
		BKE_spacedata_copylist(&sa1->spacedata, &sa2->spacedata);
	}
	
	/* Note; SPACE_EMPTY is possible on new screens */
	
	/* regions */
	if (swap_space == 1) {
		SWAP(ListBase, sa1->regionbase, sa2->regionbase);
	}
	else {
		if (swap_space<2) {
			st= BKE_spacetype_from_id(spacetype);
			for (ar= sa1->regionbase.first; ar; ar= ar->next)
				BKE_area_region_free(st, ar);
			BLI_freelistN(&sa1->regionbase);
		}
		
		st= BKE_spacetype_from_id(sa2->spacetype);
		for (ar= sa2->regionbase.first; ar; ar= ar->next) {
			ARegion *newar= BKE_area_region_copy(st, ar);
			BLI_addtail(&sa1->regionbase, newar);
		}
	}
}

/* *********** Space switching code *********** */

void ED_area_swapspace(bContext *C, ScrArea *sa1, ScrArea *sa2)
{
	ScrArea *tmp= MEM_callocN(sizeof(ScrArea), "addscrarea");

	ED_area_exit(C, sa1);
	ED_area_exit(C, sa2);

	area_copy_data(tmp, sa1, 2);
	area_copy_data(sa1, sa2, 0);
	area_copy_data(sa2, tmp, 0);
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

		st= BKE_spacetype_from_id(type);
		slold= sa->spacedata.first;

		sa->spacetype= type;
		sa->butspacetype= type;
		sa->type= st;
		
		/* check previously stored space */
		for (sl= sa->spacedata.first; sl; sl= sl->next)
			if (sl->spacetype==type)
				break;
		
		/* old spacedata... happened during work on 2.50, remove */
		if (sl && sl->regionbase.first==NULL) {
			st->free(sl);
			BLI_freelinkN(&sa->spacedata, sl);
			if (slold == sl) {
				slold= NULL;
			}
			sl= NULL;
		}
		
		if (sl) {
			
			/* swap regions */
			slold->regionbase= sa->regionbase;
			sa->regionbase= sl->regionbase;
			sl->regionbase.first= sl->regionbase.last= NULL;
			
			/* put in front of list */
			BLI_remlink(&sa->spacedata, sl);
			BLI_addhead(&sa->spacedata, sl);
		} 
		else {
			/* new space */
			if (st) {
				sl= st->new(C);
				BLI_addhead(&sa->spacedata, sl);
				
				/* swap regions */
				if (slold)
					slold->regionbase= sa->regionbase;
				sa->regionbase= sl->regionbase;
				sl->regionbase.first= sl->regionbase.last= NULL;
			}
		}
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		
		/* tell WM to refresh, cursor types etc */
		WM_event_add_mousemove(C);
				
		/*send space change notifier*/
		WM_event_add_notifier(C, NC_SPACE|ND_SPACE_CHANGED, sa);
		
		ED_area_tag_refresh(sa);
	}
	
	/* also redraw when re-used */
	ED_area_tag_redraw(sa);
}

void ED_area_prevspace(bContext *C, ScrArea *sa)
{
	SpaceLink *sl = (sa) ? sa->spacedata.first : CTX_wm_space_data(C);

	if (sl->next) {
		/* workaround for case of double prevspace, render window
		 * with a file browser on top of it */
		if (sl->next->spacetype == SPACE_FILE && sl->next->next)
			ED_area_newspace(C, sa, sl->next->next->spacetype);
		else
			ED_area_newspace(C, sa, sl->next->spacetype);
	}
	else {
		ED_area_newspace(C, sa, SPACE_INFO);
	}
	ED_area_tag_redraw(sa);

	/*send space change notifier*/
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_CHANGED, sa);
}

static const char *editortype_pup(void)
{
	const char *types= N_(
		   "Editor type:%t"
		   "|3D View %x1"

		   "|%l"
		   
		   "|Timeline %x15"
		   "|Graph Editor %x2"
		   "|DopeSheet %x12"
		   "|NLA Editor %x13"
		   
		   "|%l"
		   
		   "|UV/Image Editor %x6"
		   
		   "|Video Sequence Editor %x8"
		   "|Movie Clip Editor %x20"
		   "|Text Editor %x9" 
		   "|Node Editor %x16"
		   "|Logic Editor %x17"
		   
		   "|%l"
		   
		   "|Properties %x4"
		   "|Outliner %x3"
		   "|User Preferences %x19"
		   "|Info%x7"

		   "|%l"

		   "|File Browser %x5"
		   
		   "|%l"
		   
		   "|Python Console %x18"
		   );

	return IFACE_(types);
}

static void spacefunc(struct bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	ED_area_newspace(C, CTX_wm_area(C), CTX_wm_area(C)->butspacetype);
	ED_area_tag_redraw(CTX_wm_area(C));

	/*send space change notifier*/
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_CHANGED, CTX_wm_area(C));
}

/* returns offset for next button in header */
int ED_area_header_switchbutton(const bContext *C, uiBlock *block, int yco)
{
	ScrArea *sa= CTX_wm_area(C);
	uiBut *but;
	int xco= 8;
	
	but = uiDefIconTextButC(block, ICONTEXTROW, 0, ICON_VIEW3D, 
						   editortype_pup(), xco, yco, UI_UNIT_X+10, UI_UNIT_Y, 
						   &(sa->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
						   TIP_("Displays current editor type. Click for menu of available types"));
	uiButSetFunc(but, spacefunc, NULL, NULL);
	uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */
	
	return xco + UI_UNIT_X + 14;
}

int ED_area_header_standardbuttons(const bContext *C, uiBlock *block, int yco)
{
	ScrArea *sa= CTX_wm_area(C);
	int xco= 8;
	uiBut *but;
	
	if (!sa->full)
		xco= ED_area_header_switchbutton(C, block, yco);

	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (sa->flag & HEADER_NO_PULLDOWN) {
		but = uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, 0,
						 ICON_DISCLOSURE_TRI_RIGHT,
						 xco,yco,UI_UNIT_X,UI_UNIT_Y-2,
						 &(sa->flag), 0, 0, 0, 0, 
						 "Show pulldown menus");
	}
	else {
		but = uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, 0,
						 ICON_DISCLOSURE_TRI_DOWN,
						 xco,yco,UI_UNIT_X,UI_UNIT_Y-2,
						 &(sa->flag), 0, 0, 0, 0, 
						 "Hide pulldown menus");
	}

	uiButClearFlag(but, UI_BUT_UNDO); /* skip undo on screen buttons */

	uiBlockSetEmboss(block, UI_EMBOSS);
	
	return xco + UI_UNIT_X;
}

/************************ standard UI regions ************************/

void ED_region_panels(const bContext *C, ARegion *ar, int vertical, const char *context, int contextnr)
{
	ScrArea *sa= CTX_wm_area(C);
	uiStyle *style= UI_GetStyle();
	uiBlock *block;
	PanelType *pt;
	Panel *panel;
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	int x, y, xco, yco, w, em, triangle, open, newcontext= 0;

	if (contextnr >= 0)
		newcontext= UI_view2d_tab_set(v2d, contextnr);

	if (vertical) {
		w= v2d->cur.xmax - v2d->cur.xmin;
		em= (ar->type->prefsizex)? UI_UNIT_Y/2: UI_UNIT_Y;
	}
	else {
		w= UI_PANEL_WIDTH;
		em= (ar->type->prefsizex)? UI_UNIT_Y/2: UI_UNIT_Y;
	}

	/* create panels */
	uiBeginPanels(C, ar);

	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(v2d);

	for (pt= ar->type->paneltypes.first; pt; pt= pt->next) {
		/* verify context */
		if (context)
			if (pt->context[0] && strcmp(context, pt->context) != 0)
				continue;

		/* draw panel */
		if (pt->draw && (!pt->poll || pt->poll(C, pt))) {
			block= uiBeginBlock(C, ar, pt->idname, UI_EMBOSS);
			panel= uiBeginPanel(sa, ar, block, pt, &open);

			/* bad fixed values */
			triangle= (int)(UI_UNIT_Y * 1.1f);

			if (pt->draw_header && !(pt->flag & PNL_NO_HEADER) && (open || vertical)) {
				/* for enabled buttons */
				panel->layout= uiBlockLayout(block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER,
					triangle, UI_UNIT_Y+style->panelspace+2, UI_UNIT_Y, 1, style);

				pt->draw_header(C, panel);

				uiBlockLayoutResolve(block, &xco, &yco);
				panel->labelofs= xco - triangle;
				panel->layout= NULL;
			}
			else {
				panel->labelofs= 0;
			}

			if (open) {
				short panelContext;
				
				/* panel context can either be toolbar region or normal panels region */
				if (ar->regiontype == RGN_TYPE_TOOLS)
					panelContext= UI_LAYOUT_TOOLBAR;
				else
					panelContext= UI_LAYOUT_PANEL;
				
				panel->layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, panelContext,
					style->panelspace, 0, w-2*style->panelspace, em, style);

				pt->draw(C, panel);

				uiBlockLayoutResolve(block, &xco, &yco);
				panel->layout= NULL;

				yco -= 2*style->panelspace;
				uiEndPanel(block, w, -yco);
			}
			else {
				yco= 0;
				uiEndPanel(block, w, 0);
			}

			uiEndBlock(C, block);
		}
	}

	/* align panels and return size */
	uiEndPanels(C, ar, &x, &y);

	/* clear */
	UI_ThemeClearColor((ar->type->regionid == RGN_TYPE_PREVIEW)?TH_PREVIEW_BACK:TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* before setting the view */
	if (vertical) {
		/* only allow scrolling in vertical direction */
		v2d->keepofs |= V2D_LOCKOFS_X|V2D_KEEPOFS_Y;
		v2d->keepofs &= ~(V2D_LOCKOFS_Y|V2D_KEEPOFS_X);
		v2d->scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
		v2d->scroll &= ~V2D_SCROLL_VERTICAL_HIDE;
		
		// don't jump back when panels close or hide
		if (!newcontext)
			y= MAX2(-y, -v2d->cur.ymin);
		else
			y= -y;
	}
	else {
		/* for now, allow scrolling in both directions (since layouts are optimized for vertical,
		 * they often don't fit in horizontal layout)
		 */
		v2d->keepofs &= ~(V2D_LOCKOFS_X|V2D_LOCKOFS_Y|V2D_KEEPOFS_X|V2D_KEEPOFS_Y);
		//v2d->keepofs |= V2D_LOCKOFS_Y|V2D_KEEPOFS_X;
		//v2d->keepofs &= ~(V2D_LOCKOFS_X|V2D_KEEPOFS_Y);
		v2d->scroll |= V2D_SCROLL_VERTICAL_HIDE;
		v2d->scroll &= ~V2D_SCROLL_HORIZONTAL_HIDE;
		
		// don't jump back when panels close or hide
		if (!newcontext)
			x= MAX2(x, v2d->cur.xmax);
		y= -y;
	}

	// +V2D_SCROLL_HEIGHT is workaround to set the actual height
	UI_view2d_totRect_set(v2d, x+V2D_SCROLL_WIDTH, y+V2D_SCROLL_HEIGHT);

	/* set the view */
	UI_view2d_view_ortho(v2d);

	/* draw panels */
	uiDrawPanels(C, ar);

	/* restore view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

void ED_region_panels_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	// XXX quick hacks for files saved with 2.5 already (i.e. the builtin defaults file)
		// scrollbars for button regions
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM); 
	ar->v2d.scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
	ar->v2d.scroll &= ~V2D_SCROLL_VERTICAL_HIDE;
	ar->v2d.keepzoom |= V2D_KEEPZOOM;

		// correctly initialized User-Prefs?
	if (!(ar->v2d.align & V2D_ALIGN_NO_POS_Y))
		ar->v2d.flag &= ~V2D_IS_INITIALISED;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_PANELS_UI, ar->winx, ar->winy);

	keymap = WM_keymap_find(wm->defaultconf, "View2D Buttons List", 0, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

void ED_region_header(const bContext *C, ARegion *ar)
{
	uiStyle *style= UI_GetStyle();
	uiBlock *block;
	uiLayout *layout;
	HeaderType *ht;
	Header header = {NULL};
	int maxco, xco, yco;
	int headery= ED_area_headersize();

	/* clear */	
	UI_ThemeClearColor((ED_screen_area_active(C))?TH_HEADER:TH_HEADERDESEL);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(&ar->v2d);

	xco= maxco= 8;
	yco= headery-4;

	/* draw all headers types */
	for (ht= ar->type->headertypes.first; ht; ht= ht->next) {
		block= uiBeginBlock(C, ar, ht->idname, UI_EMBOSS);
		layout= uiBlockLayout(block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, yco, UI_UNIT_Y, 1, style);

		if (ht->draw) {
			header.type= ht;
			header.layout= layout;
			ht->draw(C, &header);
			
			/* for view2d */
			xco= uiLayoutGetWidth(layout);
			if (xco > maxco)
				maxco= xco;
		}

		uiBlockLayoutResolve(block, &xco, &yco);
		
		/* for view2d */
		if (xco > maxco)
			maxco= xco;
		
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
	}

	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, maxco+UI_UNIT_X+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);

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
	return UI_UNIT_Y+6;
}

void ED_region_info_draw(ARegion *ar, const char *text, int block, float alpha)
{
	const int header_height = 18;
	uiStyle *style= UI_GetStyle();
	int fontid= style->widget.uifont_id;
	rcti rect;

	BLF_size(fontid, 11.0f, 72);

	/* background box */
	rect= ar->winrct;
	rect.xmin = 0;
	rect.ymin = ar->winrct.ymax - ar->winrct.ymin - header_height;

	if (block) {
		rect.xmax = ar->winrct.xmax - ar->winrct.xmin;
	}
	else {
		rect.xmax = rect.xmin + BLF_width(fontid, text) + 24;
	}

	rect.ymax = ar->winrct.ymax - ar->winrct.ymin;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	glRecti(rect.xmin, rect.ymin, rect.xmax+1, rect.ymax+1);
	glDisable(GL_BLEND);

	/* text */
	UI_ThemeColor(TH_TEXT_HI);
	BLF_position(fontid, 12, rect.ymin + 5, 0.0f);
	BLF_draw(fontid, text, BLF_DRAW_STR_DUMMY_MAX);
}
