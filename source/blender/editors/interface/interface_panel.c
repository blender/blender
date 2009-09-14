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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2003-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt */
 
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "interface_intern.h"

/*********************** defines and structs ************************/

#define ANIMATION_TIME		0.30
#define ANIMATION_INTERVAL	0.02

#define PNL_LAST_ADDED		1
#define PNL_ACTIVE			2
#define PNL_WAS_ACTIVE		4
#define PNL_ANIM_ALIGN		8
#define PNL_NEW_ADDED		16
#define PNL_FIRST			32

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
	if(sa->spacetype==SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts= sa->spacedata.first;
		return sbuts->align;
	}
	else if(sa->spacetype==SPACE_USERPREF && ar->regiontype == RGN_TYPE_WINDOW)
		return BUT_VERTICAL;
	else if(sa->spacetype==SPACE_FILE && ar->regiontype == RGN_TYPE_CHANNELS)
		return BUT_VERTICAL;
	else if(ELEM3(ar->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS))
		return BUT_VERTICAL;
	
	return 0;
}

static int panels_re_align(ScrArea *sa, ARegion *ar, Panel **r_pa)
{
	Panel *pa;
	int active= 0;

	*r_pa= NULL;

	if(sa->spacetype==SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts= sa->spacedata.first;

		if(sbuts->align)
			if(sbuts->re_align || sbuts->mainbo!=sbuts->mainb)
				return 1;
	}
	else if(ar->regiontype==RGN_TYPE_UI)
		return 1;
	else if(sa->spacetype==SPACE_FILE && ar->regiontype == RGN_TYPE_CHANNELS)
		return 1;

	/* in case panel is added or disappears */
	for(pa=ar->panels.first; pa; pa=pa->next) {
		if((pa->runtime_flag & PNL_WAS_ACTIVE) && !(pa->runtime_flag & PNL_ACTIVE))
			return 1;
		if(!(pa->runtime_flag & PNL_WAS_ACTIVE) && (pa->runtime_flag & PNL_ACTIVE))
			return 1;
		if(pa->activedata)
			active= 1;
	}

	/* in case we need to do an animation (size changes) */
	for(pa=ar->panels.first; pa; pa=pa->next) {
		if(pa->runtime_flag & PNL_ANIM_ALIGN) {
			if(!active)
				*r_pa= pa;
			return 1;
		}
	}
	
	return 0;
}

/****************************** panels ******************************/

static void ui_panel_copy_offset(Panel *pa, Panel *papar)
{
	/* with respect to sizes... papar is parent */

	pa->ofsx= papar->ofsx;
	pa->ofsy= papar->ofsy + papar->sizey-pa->sizey;
}

Panel *uiBeginPanel(ScrArea *sa, ARegion *ar, uiBlock *block, PanelType *pt, int *open)
{
	uiStyle *style= U.uistyles.first;
	Panel *pa, *patab, *palast, *panext;
	char *drawname= pt->label;
	char *idname= pt->idname;
	char *tabname= pt->idname;
	char *hookname= NULL;
	int newpanel;
	int align= panel_aligned(sa, ar);
	
	/* check if Panel exists, then use that one */
	for(pa=ar->panels.first; pa; pa=pa->next)
		if(strncmp(pa->panelname, idname, UI_MAX_NAME_STR)==0)
			if(strncmp(pa->tabname, tabname, UI_MAX_NAME_STR)==0)
				break;
	
	newpanel= (pa == NULL);

	if(!newpanel) {
		pa->type= pt;
	}
	else {
		/* new panel */
		pa= MEM_callocN(sizeof(Panel), "new panel");
		pa->type= pt;
		BLI_strncpy(pa->panelname, idname, UI_MAX_NAME_STR);
		BLI_strncpy(pa->tabname, tabname, UI_MAX_NAME_STR);

		if(pt->flag & PNL_DEFAULT_CLOSED) {
			if(align == BUT_VERTICAL)
				pa->flag |= PNL_CLOSEDY;
			else
				pa->flag |= PNL_CLOSEDX;
		}
	
		pa->ofsx= 0;
		pa->ofsy= style->panelouter;
		pa->sizex= 0;
		pa->sizey= 0;
		pa->runtime_flag |= PNL_NEW_ADDED;

		BLI_addtail(&ar->panels, pa);
		
		/* make new Panel tabbed? */
		if(hookname) {
			for(patab= ar->panels.first; patab; patab= patab->next) {
				if((patab->runtime_flag & PNL_ACTIVE) && patab->paneltab==NULL) {
					if(strncmp(hookname, patab->panelname, UI_MAX_NAME_STR)==0) {
						if(strncmp(tabname, patab->tabname, UI_MAX_NAME_STR)==0) {
							pa->paneltab= patab;
							ui_panel_copy_offset(pa, patab);
							break;
						}
					}
				}
			} 
		}
	}

	BLI_strncpy(pa->drawname, drawname, UI_MAX_NAME_STR);

	/* if a new panel is added, we insert it right after the panel
	 * that was last added. this way new panels are inserted in the
	 * right place between versions */
	for(palast=ar->panels.first; palast; palast=palast->next)
		if(palast->runtime_flag & PNL_LAST_ADDED)
			break;
	
	if(newpanel) {
		pa->sortorder= (palast)? palast->sortorder+1: 0;

		for(panext=ar->panels.first; panext; panext=panext->next)
			if(panext != pa && panext->sortorder >= pa->sortorder)
				panext->sortorder++;
	}

	if(palast)
		palast->runtime_flag &= ~PNL_LAST_ADDED;

	/* assign to block */
	block->panel= pa;
	pa->runtime_flag |= PNL_ACTIVE|PNL_LAST_ADDED;

	*open= 0;

	if(pa->paneltab) return pa;
	if(pa->flag & PNL_CLOSED) return pa;

	*open= 1;
	
	return pa;
}

void uiEndPanel(uiBlock *block, int width, int height)
{
	Panel *pa= block->panel;

	if(pa->runtime_flag & PNL_NEW_ADDED) {
		pa->runtime_flag &= ~PNL_NEW_ADDED;
		pa->sizex= width;
		pa->sizey= height;
	}
	else {
		/* check if we need to do an animation */
		if(!ELEM(width, 0, pa->sizex) || !ELEM(height, 0, pa->sizey)) {
			pa->runtime_flag |= PNL_ANIM_ALIGN;
			if(height != 0)
				pa->ofsy += pa->sizey-height;
		}

		/* update width/height if non-zero */
		if(width != 0)
			pa->sizex= width;
		if(height != 0)
			pa->sizey= height;
	}
}

#if 0
void uiPanelToMouse(const bContext *C, Panel *pa)
{
	/* global control over this feature; UI_PNL_TO_MOUSE only called for hotkey panels */
	if(U.uiflag & USER_PANELPINNED);
	else if(pa->control & UI_PNL_TO_MOUSE) {
		int mx, my;

		mx= CTX_wm_window(C)->eventstate->x;
		my= CTX_wm_window(C)->eventstate->y;
		
		pa->ofsx= mx-pa->sizex/2;
		pa->ofsy= my-pa->sizey/2;
		
		if(pa->flag & PNL_CLOSED) pa->flag &= ~PNL_CLOSED;
	}
	
	if(pa->control & UI_PNL_UNSTOW) {
		if(pa->flag & PNL_CLOSEDY) {
			pa->flag &= ~PNL_CLOSED;
		}
	}
}
#endif

static int panel_has_tabs(ARegion *ar, Panel *panel)
{
	Panel *pa= ar->panels.first;
	
	if(panel==NULL) return 0;
	
	while(pa) {
		if((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==panel) {
			return 1;
		}
		pa= pa->next;
	}
	return 0;
}

static void ui_offset_panel_block(uiBlock *block)
{
	uiStyle *style= U.uistyles.first;
	uiBut *but;
	int ofsy;

	/* compute bounds and offset */
	ui_bounds_block(block);

	ofsy= block->panel->sizey - style->panelspace;

	for(but= block->buttons.first; but; but=but->next) {
		but->y1 += ofsy;
		but->y2 += ofsy;
	}

	block->maxx= block->panel->sizex;
	block->maxy= block->panel->sizey;
	block->minx= block->miny= 0.0;
}

/**************************** drawing *******************************/

/* extern used by previewrender */
void uiPanelPush(uiBlock *block)
{
	glPushMatrix(); 

	if(block->panel)
		glTranslatef((float)block->panel->ofsx, (float)block->panel->ofsy, 0.0);
}

void uiPanelPop(uiBlock *block)
{
	glPopMatrix();
}

/* triangle 'icon' for panel header */
void ui_draw_tria_icon(float x, float y, char dir)
{
	if(dir=='h') {
		ui_draw_anti_tria(x-1, y, x-1, y+11.0, x+9, y+6.25);
	}
	else {
		ui_draw_anti_tria(x-3, y+10,  x+8-1, y+10, x+4.25-2, y);	
	}
}

/* triangle 'icon' inside rect */
void ui_draw_tria_rect(rctf *rect, char dir)
{
	if(dir=='h') {
		float half= 0.5f*(rect->ymax - rect->ymin);
		ui_draw_anti_tria(rect->xmin, rect->ymin, rect->xmin, rect->ymax, rect->xmax, rect->ymin+half);
	}
	else {
		float half= 0.5f*(rect->xmax - rect->xmin);
		ui_draw_anti_tria(rect->xmin, rect->ymax, rect->xmax, rect->ymax, rect->xmin+half, rect->ymin);
	}
}

void ui_draw_anti_x(float x1, float y1, float x2, float y2)
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

	ui_draw_anti_x(x, y, x+9.375, y+9.375);

}

#define PNL_ICON 	20

static void ui_draw_panel_scalewidget(rcti *rect)
{
	float xmin, xmax, dx;
	float ymin, ymax, dy;
	
	xmin= rect->xmax-PNL_HEADER+2;
	xmax= rect->xmax-3;
	ymin= rect->ymin+3;
	ymax= rect->ymin+PNL_HEADER-2;
		
	dx= 0.5f*(xmax-xmin);
	dy= 0.5f*(ymax-ymin);
	
	glEnable(GL_BLEND);
	glColor4ub(255, 255, 255, 50);
	fdrawline(xmin, ymin, xmax, ymax);
	fdrawline(xmin+dx, ymin, xmax, ymax-dy);
	
	glColor4ub(0, 0, 0, 50);
	fdrawline(xmin, ymin+1, xmax, ymax+1);
	fdrawline(xmin+dx, ymin+1, xmax, ymax-dy+1);
	glDisable(GL_BLEND);
}

static void ui_draw_panel_dragwidget(rctf *rect)
{
	float xmin, xmax, dx;
	float ymin, ymax, dy;
	
	xmin= rect->xmin;
	xmax= rect->xmax;
	ymin= rect->ymin;
	ymax= rect->ymax;
	
	dx= 0.333f*(xmax-xmin);
	dy= 0.333f*(ymax-ymin);
	
	glEnable(GL_BLEND);
	glColor4ub(255, 255, 255, 50);
	fdrawline(xmin, ymax, xmax, ymin);
	fdrawline(xmin+dx, ymax, xmax, ymin+dy);
	fdrawline(xmin+2*dx, ymax, xmax, ymin+2*dy);
	
	glColor4ub(0, 0, 0, 50);
	fdrawline(xmin, ymax+1, xmax, ymin+1);
	fdrawline(xmin+dx, ymax+1, xmax, ymin+dy+1);
	fdrawline(xmin+2*dx, ymax+1, xmax, ymin+2*dy+1);
	glDisable(GL_BLEND);
}


static void ui_draw_aligned_panel_header(ARegion *ar, uiStyle *style, uiBlock *block, rcti *rect)
{
	Panel *panel= block->panel;
	Panel *pa;
	rcti hrect;
	float width;
	int a, nr= 1, pnl_icons;
	char *activename= panel->drawname[0]?panel->drawname:panel->panelname;
	char *panelname;
	
	/* count */
	for(pa= ar->panels.first; pa; pa=pa->next)
		if(pa->runtime_flag & PNL_ACTIVE)
			if(pa->paneltab==panel)
				nr++;
	
	/* + 0.001f to avoid flirting with float inaccuracy */
	if(panel->control & UI_PNL_CLOSE) pnl_icons=(panel->labelofs+2*PNL_ICON+5)/block->aspect + 0.001f;
	else pnl_icons= (panel->labelofs+PNL_ICON+5)/block->aspect + 0.001f;
	
	if(nr==1) {
		
		/* active tab */
		/* draw text label */
		UI_ThemeColor(TH_TITLE);
		
		hrect= *rect;
		hrect.xmin= rect->xmin+pnl_icons;
		uiStyleFontDraw(&style->paneltitle, &hrect, activename);
		
		return;
	}
	
	a= 0;
	width= (rect->xmax-rect->xmin - 3 - pnl_icons - PNL_ICON)/nr;
	for(pa= ar->panels.first; pa; pa=pa->next) {
		panelname= pa->drawname[0]?pa->drawname:pa->panelname;
		
		if((pa->runtime_flag & PNL_ACTIVE) && (pa==panel || pa->paneltab==panel)) {
			float col[3];
			
			UI_GetThemeColor3fv(TH_TITLE, col);

			/* active tab */
			if(pa==panel)
				glColor4f(col[0], col[1], col[2], 1.0f);
			else
				glColor4f(col[0], col[1], col[2], 0.5f);
			
			hrect= *rect;
			hrect.xmin= rect->xmin+pnl_icons + a*width;
			hrect.xmax= hrect.xmin + width;
			uiStyleFontDraw(&style->paneltitle, &hrect, panelname);
			
			a++;
		}
	}
}

static void rectf_scale(rctf *rect, float scale)
{
	float centx= 0.5f*(rect->xmin+rect->xmax);
	float centy= 0.5f*(rect->ymin+rect->ymax);
	float sizex= 0.5f*scale*(rect->xmax - rect->xmin);
	float sizey= 0.5f*scale*(rect->ymax - rect->ymin);
	
	rect->xmin= centx - sizex;
	rect->xmax= centx + sizex;
	rect->ymin= centy - sizey;
	rect->ymax= centy + sizey;
}

/* panel integrated in buttonswindow, tool/property lists etc */
void ui_draw_aligned_panel(ARegion *ar, uiStyle *style, uiBlock *block, rcti *rect)
{
	Panel *panel= block->panel;
	rcti headrect;
	rctf itemrect;
	int ofsx;
	
	if(panel->paneltab) return;
	if(panel->type && (panel->type->flag & PNL_NO_HEADER)) return;

	/* calculate header rect */
	/* + 0.001f to prevent flicker due to float inaccuracy */
	headrect= *rect;
	headrect.ymin= headrect.ymax;
	headrect.ymax= headrect.ymin + floor(PNL_HEADER/block->aspect + 0.001f);
	
	if(!(panel->runtime_flag & PNL_FIRST)) {
		float minx= rect->xmin+5.0f/block->aspect;
		float maxx= rect->xmax-5.0f/block->aspect;
		float y= headrect.ymax;
		
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
		fdrawline(minx, y+1, maxx, y+1);
		glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
		fdrawline(minx, y, maxx, y);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(!(panel->flag & PNL_CLOSEDX)) {
		ui_draw_aligned_panel_header(ar, style, block, &headrect);
		
		/* itemrect smaller */	
		itemrect.xmax= headrect.xmax - 5.0f/block->aspect;
		itemrect.xmin= itemrect.xmax - (headrect.ymax-headrect.ymin);
		itemrect.ymin= headrect.ymin;
		itemrect.ymax= headrect.ymax;
		rectf_scale(&itemrect, 0.8f);
		ui_draw_panel_dragwidget(&itemrect);
	}
	
	/* if the panel is minimized vertically:
		* (------)
		*/
	if(panel->flag & PNL_CLOSEDY) {
		
		
		/* if it's being overlapped by a panel being dragged */
		if(panel->flag & PNL_OVERLAP) {
			UI_ThemeColor(TH_TEXT_HI);
			uiRoundRect(rect->xmin, rect->ymax, rect->xmax, rect->ymax+PNL_HEADER, 8);
		}
		
	}
	else if(panel->flag & PNL_CLOSEDX) {
		
	}
	/* an open panel */
	else {
		
		/* in some occasions, draw a border */
		if(panel->flag & PNL_SELECT) {
			if(panel->control & UI_PNL_SOLID) uiSetRoundBox(15);
			else uiSetRoundBox(3);
			
			UI_ThemeColorShade(TH_BACK, -120);
			uiRoundRect(0.5f + rect->xmin, 0.5f + rect->ymin, 0.5f + rect->xmax, 0.5f + headrect.ymax+1, 8);
		}
		if(panel->flag & PNL_OVERLAP) {
			if(panel->control & UI_PNL_SOLID) uiSetRoundBox(15);
			else uiSetRoundBox(3);
			
			UI_ThemeColor(TH_TEXT_HI);
			uiRoundRect(rect->xmin, rect->ymin, rect->xmax, headrect.ymax+1, 8);
		}
		
		if(panel->control & UI_PNL_SCALE)
			ui_draw_panel_scalewidget(rect);
	}
	
	/* draw optional close icon */
	
	ofsx= 6;
	if(panel->control & UI_PNL_CLOSE) {
		
		UI_ThemeColor(TH_TEXT);
		ui_draw_x_icon(rect->xmin+2+ofsx, rect->ymax+2);
		ofsx= 22;
	}
	
	/* draw collapse icon */
	UI_ThemeColor(TH_TEXT);
	
	/* itemrect smaller */	
	itemrect.xmin= headrect.xmin + 5.0f/block->aspect;
	itemrect.xmax= itemrect.xmin + (headrect.ymax-headrect.ymin);
	itemrect.ymin= headrect.ymin;
	itemrect.ymax= headrect.ymax;
	
	rectf_scale(&itemrect, 0.5f);
	
	if(panel->flag & PNL_CLOSEDY)
		ui_draw_tria_rect(&itemrect, 'h');
	else if(panel->flag & PNL_CLOSEDX)
		ui_draw_tria_rect(&itemrect, 'h');
	else
		ui_draw_tria_rect(&itemrect, 'v');
	
	
}

/************************** panel alignment *************************/

static int get_panel_header(Panel *pa)
{
	if(pa->type && (pa->type->flag & PNL_NO_HEADER))
		return 0;

	return PNL_HEADER;
}

static int get_panel_size_y(uiStyle *style, Panel *pa)
{
	if(pa->type && (pa->type->flag & PNL_NO_HEADER))
		return pa->sizey;

	return PNL_HEADER + pa->sizey + style->panelouter;
}

/* this function is needed because uiBlock and Panel itself dont
change sizey or location when closed */
static int get_panel_real_ofsy(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDY) return pa->ofsy+pa->sizey;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDY)) return pa->ofsy+pa->sizey;
	else if(pa->paneltab) return pa->paneltab->ofsy;
	else return pa->ofsy;
}

static int get_panel_real_ofsx(Panel *pa)
{
	if(pa->flag & PNL_CLOSEDX) return pa->ofsx+get_panel_header(pa);
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDX)) return pa->ofsx+get_panel_header(pa);
	else return pa->ofsx+pa->sizex;
}

typedef struct PanelSort {
	Panel *pa, *orig;
} PanelSort;

/* note about sorting;
   the sortorder has a lower value for new panels being added.
   however, that only works to insert a single panel, when more new panels get
   added the coordinates of existing panels and the previously stored to-be-insterted
   panels do not match for sorting */

static int find_leftmost_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if(ps1->pa->ofsx > ps2->pa->ofsx) return 1;
	else if(ps1->pa->ofsx < ps2->pa->ofsx) return -1;
	else if(ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if(ps1->pa->sortorder < ps2->pa->sortorder) return -1;

	return 0;
}


static int find_highest_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if(ps1->pa->ofsy+ps1->pa->sizey < ps2->pa->ofsy+ps2->pa->sizey) return 1;
	else if(ps1->pa->ofsy+ps1->pa->sizey > ps2->pa->ofsy+ps2->pa->sizey) return -1;
	else if(ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if(ps1->pa->sortorder < ps2->pa->sortorder) return -1;
	
	return 0;
}

static int compare_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if(ps1->pa->sortorder > ps2->pa->sortorder) return 1;
	else if(ps1->pa->sortorder < ps2->pa->sortorder) return -1;
	
	return 0;
}

/* this doesnt draw */
/* returns 1 when it did something */
int uiAlignPanelStep(ScrArea *sa, ARegion *ar, float fac, int drag)
{
	uiStyle *style= U.uistyles.first;
	Panel *pa;
	PanelSort *ps, *panelsort, *psnext;
	int a, tot=0, done;
	int align= panel_aligned(sa, ar);
	
	/* count active, not tabbed panels */
	for(pa= ar->panels.first; pa; pa= pa->next)
		if((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==NULL)
			tot++;

	if(tot==0) return 0;

	/* extra; change close direction? */
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==NULL) {
			if((pa->flag & PNL_CLOSEDX) && (align==BUT_VERTICAL))
				pa->flag ^= PNL_CLOSED;
			else if((pa->flag & PNL_CLOSEDY) && (align==BUT_HORIZONTAL))
				pa->flag ^= PNL_CLOSED;
		}
	}

	/* sort panels */
	panelsort= MEM_callocN(tot*sizeof(PanelSort), "panelsort");
	
	ps= panelsort;
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==NULL) {
			ps->pa= MEM_dupallocN(pa);
			ps->orig= pa;
			ps++;
		}
	}
	
	if(drag) {
		/* while we are dragging, we sort on location and update sortorder */
		if(align==BUT_VERTICAL) 
			qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
		else
			qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);

		for(ps=panelsort, a=0; a<tot; a++, ps++)
			ps->orig->sortorder= a;
	}
	else
		/* otherwise use sortorder */
		qsort(panelsort, tot, sizeof(PanelSort), compare_panel);
	
	/* no smart other default start loc! this keeps switching f5/f6/etc compatible */
	ps= panelsort;
	ps->pa->ofsx= 0;
	ps->pa->ofsy= -get_panel_size_y(style, ps->pa);

	for(a=0; a<tot-1; a++, ps++) {
		psnext= ps+1;
	
		if(align==BUT_VERTICAL) {
			psnext->pa->ofsx= ps->pa->ofsx;
			psnext->pa->ofsy= get_panel_real_ofsy(ps->pa) - get_panel_size_y(style, psnext->pa);
		}
		else {
			psnext->pa->ofsx= get_panel_real_ofsx(ps->pa);
			psnext->pa->ofsy= ps->pa->ofsy + get_panel_size_y(style, ps->pa) - get_panel_size_y(style, psnext->pa);
		}
	}
	
	/* we interpolate */
	done= 0;
	ps= panelsort;
	for(a=0; a<tot; a++, ps++) {
		if((ps->pa->flag & PNL_SELECT)==0) {
			if((ps->orig->ofsx != ps->pa->ofsx) || (ps->orig->ofsy != ps->pa->ofsy)) {
				ps->orig->ofsx= floor(0.5 + fac*ps->pa->ofsx + (1.0-fac)*ps->orig->ofsx);
				ps->orig->ofsy= floor(0.5 + fac*ps->pa->ofsy + (1.0-fac)*ps->orig->ofsy);
				done= 1;
			}
		}
	}

	/* copy locations to tabs */
	for(pa= ar->panels.first; pa; pa= pa->next)
		if(pa->paneltab && (pa->runtime_flag & PNL_ACTIVE))
			ui_panel_copy_offset(pa, pa->paneltab);

	/* free panelsort array */
	for(ps= panelsort, a=0; a<tot; a++, ps++)
		MEM_freeN(ps->pa);
	MEM_freeN(panelsort);
	
	return done;
}


static void ui_do_animate(const bContext *C, Panel *panel)
{
	uiHandlePanelData *data= panel->activedata;
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	float fac;

	fac= (PIL_check_seconds_timer()-data->starttime)/ANIMATION_TIME;
	fac= sqrt(fac);
	fac= MIN2(fac, 1.0f);

	/* for max 1 second, interpolate positions */
	if(uiAlignPanelStep(sa, ar, fac, 0))
		ED_region_tag_redraw(ar);
	else
		fac= 1.0f;

	if(fac >= 1.0f) {
		panel_activate_state(C, panel, PANEL_STATE_EXIT);
		return;
	}
}

void uiBeginPanels(const bContext *C, ARegion *ar)
{
	Panel *pa;
  
  	/* set all panels as inactive, so that at the end we know
	 * which ones were used */
	for(pa=ar->panels.first; pa; pa=pa->next) {
		if(pa->runtime_flag & PNL_ACTIVE)
			pa->runtime_flag= PNL_WAS_ACTIVE;
		else
			pa->runtime_flag= 0;
	}
}

/* only draws blocks with panels */
void uiEndPanels(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	Panel *panot, *panew, *patest, *pa, *firstpa;
	
	/* offset contents */
	for(block= ar->uiblocks.first; block; block= block->next)
		if(block->active && block->panel)
			ui_offset_panel_block(block);

	/* consistancy; are panels not made, whilst they have tabs */
	for(panot= ar->panels.first; panot; panot= panot->next) {
		if((panot->runtime_flag & PNL_ACTIVE)==0) { // not made

			for(panew= ar->panels.first; panew; panew= panew->next) {
				if((panew->runtime_flag & PNL_ACTIVE)) {
					if(panew->paneltab==panot) { // panew is tab in notmade pa
						break;
					}
				}
			}
			/* now panew can become the new parent, check all other tabs */
			if(panew) {
				for(patest= ar->panels.first; patest; patest= patest->next) {
					if(patest->paneltab == panot) {
						patest->paneltab= panew;
					}
				}
				panot->paneltab= panew;
				panew->paneltab= NULL;
				ED_region_tag_redraw(ar); // the buttons panew were not made
			}
		}	
	}

	/* re-align, possibly with animation */
	if(panels_re_align(sa, ar, &pa)) {
		if(pa)
			panel_activate_state(C, pa, PANEL_STATE_ANIMATION);
		else
			uiAlignPanelStep(sa, ar, 1.0, 0);
	}

	/* tag first panel */
	firstpa= NULL;
	for(block= ar->uiblocks.first; block; block=block->next)
		if(block->active && block->panel)
			if(!firstpa || block->panel->sortorder < firstpa->sortorder)
				firstpa= block->panel;
	
	if(firstpa)
		firstpa->runtime_flag |= PNL_FIRST;

	/* draw panels, selected on top */
	for(block= ar->uiblocks.first; block; block=block->next) {
		if(block->active && block->panel && !(block->panel->flag & PNL_SELECT)) {
			uiDrawBlock(C, block);
		}
	}

	for(block= ar->uiblocks.first; block; block=block->next) {
		if(block->active && block->panel && (block->panel->flag & PNL_SELECT)) {
			uiDrawBlock(C, block);
		}
	}
}

/* ------------ panel merging ---------------- */

static void check_panel_overlap(ARegion *ar, Panel *panel)
{
	Panel *pa;

	/* also called with panel==NULL for clear */
	
	for(pa=ar->panels.first; pa; pa=pa->next) {
		pa->flag &= ~PNL_OVERLAP;
		if(panel && (pa != panel)) {
			if(pa->paneltab==NULL && (pa->runtime_flag & PNL_ACTIVE)) {
				float safex= 0.2, safey= 0.2;
				
				if(pa->flag & PNL_CLOSEDX) safex= 0.05;
				else if(pa->flag & PNL_CLOSEDY) safey= 0.05;
				else if(panel->flag & PNL_CLOSEDX) safex= 0.05;
				else if(panel->flag & PNL_CLOSEDY) safey= 0.05;
				
				if(pa->ofsx > panel->ofsx- safex*panel->sizex)
				if(pa->ofsx+pa->sizex < panel->ofsx+ (1.0+safex)*panel->sizex)
				if(pa->ofsy > panel->ofsy- safey*panel->sizey)
				if(pa->ofsy+pa->sizey < panel->ofsy+ (1.0+safey)*panel->sizey)
					pa->flag |= PNL_OVERLAP;
			}
		}
	}
}

static void test_add_new_tabs(ARegion *ar)
{
	Panel *pa, *pasel=NULL, *palap=NULL;
	/* search selected and overlapped panel */
	
	pa= ar->panels.first;
	while(pa) {
		if(pa->runtime_flag & PNL_ACTIVE) {
			if(pa->flag & PNL_SELECT) pasel= pa;
			if(pa->flag & PNL_OVERLAP) palap= pa;
		}
		pa= pa->next;
	}
	
	if(pasel && palap==NULL) {

		/* copy locations */
		pa= ar->panels.first;
		while(pa) {
			if(pa->paneltab==pasel) {
				ui_panel_copy_offset(pa, pasel);
			}
			pa= pa->next;
		}
	}
	
	if(pasel==NULL || palap==NULL) return;
	if(palap->type && palap->type->flag & PNL_NO_HEADER) return;
	
	/* the overlapped panel becomes a tab */
	palap->paneltab= pasel;
	
	/* the selected panel gets coords of overlapped one */
	ui_panel_copy_offset(pasel, palap);

	/* and its tabs */
	pa= ar->panels.first;
	while(pa) {
		if(pa->paneltab == pasel) {
			ui_panel_copy_offset(pa, palap);
		}
		pa= pa->next;
	}
	
	/* but, the overlapped panel already can have tabs too! */
	pa= ar->panels.first;
	while(pa) {
		if(pa->paneltab == palap) {
			pa->paneltab = pasel;
		}
		pa= pa->next;
	}
}

/************************ panel dragging ****************************/

static void ui_do_drag(const bContext *C, wmEvent *event, Panel *panel)
{
	uiHandlePanelData *data= panel->activedata;
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	short align= panel_aligned(sa, ar), dx=0, dy=0;
	
	/* first clip for window, no dragging outside */
	if(!BLI_in_rcti(&ar->winrct, event->x, event->y))
		return;

	dx= (event->x-data->startx) & ~(PNL_GRID-1);
	dy= (event->y-data->starty) & ~(PNL_GRID-1);

	dx *= (float)(ar->v2d.cur.xmax - ar->v2d.cur.xmin)/(float)(ar->winrct.xmax - ar->winrct.xmin);
	dy *= (float)(ar->v2d.cur.ymax - ar->v2d.cur.ymin)/(float)(ar->winrct.ymax - ar->winrct.ymin);
	
	if(data->state == PANEL_STATE_DRAG_SCALE) {
		panel->sizex = MAX2(data->startsizex+dx, UI_PANEL_MINX);
		
		if(data->startsizey-dy < UI_PANEL_MINY)
			dy= -UI_PANEL_MINY+data->startsizey;

		panel->sizey= data->startsizey-dy;
		panel->ofsy= data->startofsy+dy;
	}
	else {
		/* reset the panel snapping, to allow dragging away from snapped edges */
		panel->snap = PNL_SNAP_NONE;
		
		panel->ofsx = data->startofsx+dx;
		panel->ofsy = data->startofsy+dy;
		check_panel_overlap(ar, panel);
		
		if(align) uiAlignPanelStep(sa, ar, 0.2, 1);
	}

	ED_region_tag_redraw(ar);
}

static void ui_do_untab(const bContext *C, wmEvent *event, Panel *panel)
{
	uiHandlePanelData *data= panel->activedata;
	ARegion *ar= CTX_wm_region(C);
	Panel *pa, *panew= NULL;
	int nr;

	/* wait until a threshold is passed to untab */
	if(abs(event->x-data->startx) + abs(event->y-data->starty) > 6) {
		/* find new parent panel */
		nr= 0;
		for(pa= ar->panels.first; pa; pa=pa->next) {
			if(pa->paneltab==panel) {
				panew= pa;
				nr++;
			}
		}
		
		/* make old tabs point to panew */
		panew->paneltab= NULL;
		
		for(pa= ar->panels.first; pa; pa=pa->next)
			if(pa->paneltab==panel)
				pa->paneltab= panew;
		
		panel_activate_state(C, panel, PANEL_STATE_DRAG);
	}
}

/******************* region level panel interaction *****************/

static void panel_clicked_tabs(const bContext *C, ScrArea *sa, ARegion *ar, uiBlock *block,  int mousex)
{
	Panel *pa, *tabsel=NULL, *panel= block->panel;
	int nr= 1, a, width, ofsx;
	
	ofsx= PNL_ICON;
	if(block->panel->type && (block->panel->control & UI_PNL_CLOSE)) ofsx+= PNL_ICON;
	
	/* count */
	for(pa= ar->panels.first; pa; pa=pa->next)
		if(pa!=panel)
			if((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==panel)
				nr++;

	if(nr==1) return;
	
	/* find clicked tab, mouse in panel coords */
	a= 0;
	width= (int)((float)(panel->sizex - ofsx-10)/nr);
	pa= ar->panels.first;
	while(pa) {
		if(pa==panel || ((pa->runtime_flag & PNL_ACTIVE) && pa->paneltab==panel)) {
			if((mousex > ofsx+a*width) && (mousex < ofsx+(a+1)*width)) {
				tabsel= pa;
				break;
			}
			a++;
		}
		pa= pa->next;
	}

	if(tabsel) {
		if(tabsel == panel) {
			panel_activate_state(C, panel, PANEL_STATE_WAIT_UNTAB);
		}
		else {
			/* tabsel now becomes parent for all others */
			panel->paneltab= tabsel;
			tabsel->paneltab= NULL;
			
			pa= ar->panels.first;
			while(pa) {
				if(pa->paneltab == panel) pa->paneltab = tabsel;
				pa= pa->next;
			}
			
			/* copy locations to tabs */
			for(pa= ar->panels.first; pa; pa= pa->next) {
				if(pa->paneltab && pa->runtime_flag & PNL_ACTIVE) {
					ui_panel_copy_offset(pa, pa->paneltab);
				}
			}
			
			/* panels now differ size.. */
			if(panel_aligned(sa, ar))
				panel_activate_state(C, tabsel, PANEL_STATE_ANIMATION);

			ED_region_tag_redraw(ar);
		}
	}
	
}

/* this function is supposed to call general window drawing too */
/* also it supposes a block has panel, and isnt a menu */
static void ui_handle_panel_header(const bContext *C, uiBlock *block, int mx, int my)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Panel *pa;
	int align= panel_aligned(sa, ar), button= 0;

	/* mouse coordinates in panel space! */
	
	/* XXX weak code, currently it assumes layout style for location of widgets */
	
	/* check open/collapsed button */
	if(block->panel->flag & PNL_CLOSEDX) {
		if(my >= block->maxy) button= 1;
	}
	else if(block->panel->control & UI_PNL_CLOSE) {
		if(mx <= block->minx+10+PNL_ICON-2) button= 2;
		else if(mx <= block->minx+10+2*PNL_ICON+2) button= 1;
	}
	else if(mx <= block->minx+10+PNL_ICON+2) {
		button= 1;
	}
	
	if(button) {
		if(button==2) { // close
			ED_region_tag_redraw(ar);
		}
		else {	// collapse
			if(block->panel->flag & PNL_CLOSED) {
				block->panel->flag &= ~PNL_CLOSED;
				/* snap back up so full panel aligns with screen edge */
				if (block->panel->snap & PNL_SNAP_BOTTOM) 
					block->panel->ofsy= 0;
			}
			else if(align==BUT_HORIZONTAL) {
				block->panel->flag |= PNL_CLOSEDX;
			}
			else {
				/* snap down to bottom screen edge*/
				block->panel->flag |= PNL_CLOSEDY;
				if (block->panel->snap & PNL_SNAP_BOTTOM) 
					block->panel->ofsy= -block->panel->sizey;
			}
			
			for(pa= ar->panels.first; pa; pa= pa->next) {
				if(pa->paneltab==block->panel) {
					if(block->panel->flag & PNL_CLOSED) pa->flag |= PNL_CLOSED;
					else pa->flag &= ~PNL_CLOSED;
				}
			}
		}

		if(align)
			panel_activate_state(C, block->panel, PANEL_STATE_ANIMATION);
		else
			ED_region_tag_redraw(ar);
	}
	else if(block->panel->flag & PNL_CLOSED) {
		panel_activate_state(C, block->panel, PANEL_STATE_DRAG);
	}
	/* check if clicked in tabbed area */
	else if(mx < block->maxx-PNL_ICON-3 && panel_has_tabs(ar, block->panel)) {
		panel_clicked_tabs(C, sa, ar, block, mx);
	}
	else {
		panel_activate_state(C, block->panel, PANEL_STATE_DRAG);
	}
}

int ui_handler_panel_region(bContext *C, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	Panel *pa;
	int retval, mx, my, inside_header= 0, inside_scale= 0, inside;

	retval= WM_UI_HANDLER_CONTINUE;

	/* buttons get priority */
	if(ui_button_is_active(ar))
		return retval;

	for(block=ar->uiblocks.last; block; block=block->prev) {
		mx= event->x;
		my= event->y;
		ui_window_to_block(ar, block, &mx, &my);

		/* check if inside boundbox */
		inside= 0;
		pa= block->panel;

		if(!pa || pa->paneltab!=NULL)
			continue;
		if(pa->type && pa->type->flag & PNL_NO_HEADER) // XXX - accessed freed panels when scripts reload, need to fix.
			continue;

		if(block->minx <= mx && block->maxx >= mx)
			if(block->miny <= my && block->maxy+PNL_HEADER >= my)
				inside= 1;

		if(inside) {
			/* clicked at panel header? */
			if(pa->flag & PNL_CLOSEDX) {
				if(block->minx <= mx && block->minx+PNL_HEADER >= mx) 
					inside_header= 1;
			}
			else if((block->maxy <= my) && (block->maxy+PNL_HEADER >= my)) {
				inside_header= 1;
			}
			else if(pa->control & UI_PNL_SCALE) {
				if(block->maxx-PNL_HEADER <= mx)
					if(block->miny+PNL_HEADER >= my)
						inside_scale= 1;
			}

			if(event->val==KM_PRESS) {
				if(event->type == LEFTMOUSE) {
					if(inside_header) {
						ui_handle_panel_header(C, block, mx, my);
						break;
					}
					else if(inside_scale && !(pa->flag & PNL_CLOSED)) {
						panel_activate_state(C, pa, PANEL_STATE_DRAG_SCALE);
						break;
					}
				}
				else if(event->type == ESCKEY) {
					/*XXX 2.50 if(block->handler) {
						rem_blockhandler(sa, block->handler);
						ED_region_tag_redraw(ar);
						retval= WM_UI_HANDLER_BREAK;
					}*/
				}
				else if(event->type==PADPLUSKEY || event->type==PADMINUS) {
					int zoom=0;
				
					/* if panel is closed, only zoom if mouse is over the header */
					if (pa->flag & (PNL_CLOSEDX|PNL_CLOSEDY)) {
						if (inside_header)
							zoom=1;
					}
					else
						zoom=1;

#if 0 // XXX make float panel exception?
					if(zoom) {
						ScrArea *sa= CTX_wm_area(C);
						SpaceLink *sl= sa->spacedata.first;

						if(sa->spacetype!=SPACE_BUTS) {
							if(!(pa->control & UI_PNL_SCALE)) {
								if(event->type==PADPLUSKEY) sl->blockscale+= 0.1;
								else sl->blockscale-= 0.1;
								CLAMP(sl->blockscale, 0.6, 1.0);

								ED_region_tag_redraw(ar);
								retval= WM_UI_HANDLER_BREAK;
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
static int ui_handler_panel(bContext *C, wmEvent *event, void *userdata)
{
	Panel *panel= userdata;
	uiHandlePanelData *data= panel->activedata;

	/* verify if we can stop */
	if(event->type == LEFTMOUSE && event->val!=KM_PRESS) {
		ScrArea *sa= CTX_wm_area(C);
		ARegion *ar= CTX_wm_region(C);
		int align= panel_aligned(sa, ar);

		if(align)
			panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
		else
			panel_activate_state(C, panel, PANEL_STATE_EXIT);
	}
	else if(event->type == MOUSEMOVE) {
		if(data->state == PANEL_STATE_WAIT_UNTAB)
			ui_do_untab(C, event, panel);
		else if(data->state == PANEL_STATE_DRAG)
			ui_do_drag(C, event, panel);
	}
	else if(event->type == TIMER && event->customdata == data->animtimer) {
		if(data->state == PANEL_STATE_ANIMATION)
			ui_do_animate(C, panel);
		else if(data->state == PANEL_STATE_DRAG)
			ui_do_drag(C, event, panel);
	}

	data= panel->activedata;

	if(data && data->state == PANEL_STATE_ANIMATION)
		return WM_UI_HANDLER_CONTINUE;
	else
		return WM_UI_HANDLER_BREAK;
}

static void ui_handler_remove_panel(bContext *C, void *userdata)
{
	Panel *pa= userdata;

	panel_activate_state(C, pa, PANEL_STATE_EXIT);
}

static void panel_activate_state(const bContext *C, Panel *pa, uiHandlePanelState state)
{
	uiHandlePanelData *data= pa->activedata;
	wmWindow *win= CTX_wm_window(C);
	ARegion *ar= CTX_wm_region(C);
	
	if(data && data->state == state)
		return;

	if(state == PANEL_STATE_EXIT || state == PANEL_STATE_ANIMATION) {
		if(data && data->state != PANEL_STATE_ANIMATION) {
			test_add_new_tabs(ar);   // also copies locations of tabs in dragged panel
			check_panel_overlap(ar, NULL);  // clears
		}

		pa->flag &= ~PNL_SELECT;
	}
	else
		pa->flag |= PNL_SELECT;

	if(data && data->animtimer) {
		WM_event_remove_window_timer(win, data->animtimer);
		data->animtimer= NULL;
	}

	if(state == PANEL_STATE_EXIT) {
		MEM_freeN(data);
		pa->activedata= NULL;

		WM_event_remove_ui_handler(&win->handlers, ui_handler_panel, ui_handler_remove_panel, pa);
	}
	else {
		if(!data) {
			data= MEM_callocN(sizeof(uiHandlePanelData), "uiHandlePanelData");
			pa->activedata= data;

			WM_event_add_ui_handler(C, &win->handlers, ui_handler_panel, ui_handler_remove_panel, pa);
		}

		if(ELEM(state, PANEL_STATE_ANIMATION, PANEL_STATE_DRAG))
			data->animtimer= WM_event_add_window_timer(win, TIMER, ANIMATION_INTERVAL);

		data->state= state;
		data->startx= win->eventstate->x;
		data->starty= win->eventstate->y;
		data->startofsx= pa->ofsx;
		data->startofsy= pa->ofsy;
		data->startsizex= pa->sizex;
		data->startsizey= pa->sizey;
		data->starttime= PIL_check_seconds_timer();
	}

	ED_region_tag_redraw(ar);

	/* XXX exception handling, 3d window preview panel */
	/* if(block->drawextra==BIF_view3d_previewdraw)
		BIF_view3d_previewrender_clear(curarea);*/
	
	/* XXX exception handling, 3d window preview panel */
	/* if(block->drawextra==BIF_view3d_previewdraw)
		BIF_view3d_previewrender_signal(curarea, PR_DISPRECT);
	else if(strcmp(block->name, "image_panel_preview")==0)
		image_preview_event(2); */
}

