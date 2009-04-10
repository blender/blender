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

/* 
     a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt

 */
 
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
#include "UI_text.h"
#include "UI_view2d.h"

#include "interface_intern.h"

/* Defines */

#define ANIMATION_TIME		0.30
#define ANIMATION_INTERVAL	0.02

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

static void panel_activate_state(bContext *C, Panel *pa, uiHandlePanelState state);

/* ******************************** */

/* temporary code to remove all sbuts stuff from panel code */

static int panel_aligned(ScrArea *sa, ARegion *ar)
{
	if(sa->spacetype==SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts= sa->spacedata.first;
		return sbuts->align;
	}
	else if(ar->regiontype==RGN_TYPE_UI)
		return BUT_VERTICAL;
	
	return 0;
}

static int panels_re_align(ScrArea *sa, ARegion *ar)
{
	if(sa->spacetype==SPACE_BUTS && ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceButs *sbuts= sa->spacedata.first;

		if(sbuts->align)
			if(sbuts->re_align || sbuts->mainbo!=sbuts->mainb || sbuts->tabo!=sbuts->tab[sbuts->mainb])
				return 1;

		return 0;
	}
	else if(ar->regiontype==RGN_TYPE_UI)
		return 1;
	
	return 0;
}

/* ************** panels ************* */

static void copy_panel_offset(Panel *pa, Panel *papar)
{
	/* with respect to sizes... papar is parent */

	pa->ofsx= papar->ofsx;
	pa->ofsy= papar->ofsy + papar->sizey-pa->sizey;
}

/* global... but will be NULLed after each 'newPanel' call */
static char *panel_tabbed=NULL, *group_tabbed=NULL;

void uiNewPanelTabbed(char *panelname, char *groupname)
{
	panel_tabbed= panelname;
	group_tabbed= groupname;
}

/* another global... */
static int pnl_control= UI_PNL_TRANSP;

void uiPanelControl(int control)
{
	pnl_control= control;
}

/* another global... */
static int pnl_handler= 0;

void uiSetPanelHandler(int handler)
{
	pnl_handler= handler;
}

/* ofsx/ofsy only used for new panel definitions */
/* return 1 if visible (create buttons!) */
int uiNewPanel(const bContext *C, ARegion *ar, uiBlock *block, char *panelname, char *tabname, int ofsx, int ofsy, int sizex, int sizey)
{
	Panel *pa;
	
	/* check if Panel exists, then use that one */
	for(pa=ar->panels.first; pa; pa=pa->next)
		if(strncmp(pa->panelname, panelname, UI_MAX_NAME_STR)==0)
			if(strncmp(pa->tabname, tabname, UI_MAX_NAME_STR)==0)
				break;
	
	if(pa) {
		/* scale correction */
		if(pa->control & UI_PNL_SCALE);
		else {
			pa->sizex= sizex;
			if(pa->sizey != sizey) {
				pa->ofsy+= (pa->sizey - sizey);	// check uiNewPanelHeight()
				pa->sizey= sizey; 
			}
		}
	}
	else {
		/* new panel */
		pa= MEM_callocN(sizeof(Panel), "new panel");
		BLI_addtail(&ar->panels, pa);
		strncpy(pa->panelname, panelname, UI_MAX_NAME_STR);
		strncpy(pa->tabname, tabname, UI_MAX_NAME_STR);
	
		pa->ofsx= ofsx & ~(PNL_GRID-1);
		pa->ofsy= ofsy & ~(PNL_GRID-1);
		pa->sizex= sizex;
		pa->sizey= sizey;
		
		/* make new Panel tabbed? */
		if(panel_tabbed && group_tabbed) {
			Panel *papar;
			for(papar= ar->panels.first; papar; papar= papar->next) {
				if(papar->active && papar->paneltab==NULL) {
					if( strncmp(panel_tabbed, papar->panelname, UI_MAX_NAME_STR)==0) {
						if( strncmp(group_tabbed, papar->tabname, UI_MAX_NAME_STR)==0) {
							pa->paneltab= papar;
							copy_panel_offset(pa, papar);
							break;
						}
					}
				}
			} 
		}
	}

	block->panel= pa;
	block->handler= pnl_handler;
	pa->active= 1;
	pa->control= pnl_control;
	
	/* global control over this feature; UI_PNL_TO_MOUSE only called for hotkey panels */
	if(U.uiflag & USER_PANELPINNED);
	else if(pnl_control & UI_PNL_TO_MOUSE) {
		int mx, my;

		mx= CTX_wm_window(C)->eventstate->x;
		my= CTX_wm_window(C)->eventstate->y;
		
		pa->ofsx= mx-pa->sizex/2;
		pa->ofsy= my-pa->sizey/2;
		
		if(pa->flag & PNL_CLOSED) pa->flag &= ~PNL_CLOSED;
	}
	
	if(pnl_control & UI_PNL_UNSTOW) {
		if(pa->flag & PNL_CLOSEDY) {
			pa->flag &= ~PNL_CLOSED;
		}
	}
	
	/* clear ugly globals */
	panel_tabbed= group_tabbed= NULL;
	pnl_handler= 0;
	pnl_control= UI_PNL_TRANSP; // back to default
	
	if(pa->paneltab) return 0;
	if(pa->flag & PNL_CLOSED) return 0;

	/* the 'return 0' above makes this to be in end. otherwise closes panels show wrong title */
	pa->drawname[0]= 0;
	
	return 1;
}

Panel *uiPanelFromBlock(uiBlock *block)
{
	return block->panel;
}

void uiFreePanels(ListBase *lb)
{
	BLI_freelistN(lb);
}

void uiNewPanelHeight(uiBlock *block, int sizey)
{
	if(sizey<0) sizey= 0;
	
	if(block->panel) {
		block->panel->ofsy+= (block->panel->sizey - sizey);
		block->panel->sizey= sizey;
	}
}

void uiNewPanelTitle(uiBlock *block, char *str)
{
	if(block->panel)
		BLI_strncpy(block->panel->drawname, str, UI_MAX_NAME_STR);
}

static int panel_has_tabs(ARegion *ar, Panel *panel)
{
	Panel *pa= ar->panels.first;
	
	if(panel==NULL) return 0;
	
	while(pa) {
		if(pa->active && pa->paneltab==panel) {
			return 1;
		}
		pa= pa->next;
	}
	return 0;
}

static void ui_scale_panel_block(uiBlock *block)
{
	uiBut *but;
	float facx= 1.0, facy= 1.0;
	int centerx= 0, topy=0, tabsy=0;
	
	if(block->panel==NULL) return;

	if(block->autofill) ui_autofill(block);
	/* buttons min/max centered, offset calculated */
	ui_bounds_block(block);

	if( block->maxx-block->minx > block->panel->sizex - 2*PNL_SAFETY ) {
		facx= (block->panel->sizex - (2*PNL_SAFETY))/( block->maxx-block->minx );
	}
	else centerx= (block->panel->sizex-( block->maxx-block->minx ) - 2*PNL_SAFETY)/2;
	
	// tabsy= PNL_HEADER*panel_has_tabs(block->panel);
	if( (block->maxy-block->miny) > block->panel->sizey - 2*PNL_SAFETY - tabsy) {
		facy= (block->panel->sizey - (2*PNL_SAFETY) - tabsy)/( block->maxy-block->miny );
	}
	else topy= (block->panel->sizey- 2*PNL_SAFETY - tabsy) - ( block->maxy-block->miny ) ;

	but= block->buttons.first;
	while(but) {
		but->x1= PNL_SAFETY+centerx+ facx*(but->x1-block->minx);
		but->y1= PNL_SAFETY+topy   + facy*(but->y1-block->miny);
		but->x2= PNL_SAFETY+centerx+ facx*(but->x2-block->minx);
		but->y2= PNL_SAFETY+topy   + facy*(but->y2-block->miny);
		if(facx!=1.0) ui_check_but(but);	/* for strlen */
		but= but->next;
	}

	block->maxx= block->panel->sizex;
	block->maxy= block->panel->sizey;
	block->minx= block->miny= 0.0;
	
}

// for 'home' key
void uiPanelsHome(ARegion *ar)
{
	Panel *pa;
	uiBlock *block;
	View2D *v2d;
	float minx=10000, maxx= -10000, miny=10000, maxy= -10000;
	int done=0;

	v2d= &ar->v2d;
	
	for(pa= ar->panels.first; pa; pa=pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			done= 1;
			if(pa->ofsx < minx) minx= pa->ofsx;
			if(pa->ofsx+pa->sizex > maxx) maxx= pa->ofsx+pa->sizex;
			if(pa->ofsy < miny) miny= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > maxy) maxy= pa->ofsy+pa->sizey+PNL_HEADER;
		}
	}

	if(done) {
		v2d->tot.xmin= minx-PNL_DIST;
		v2d->tot.xmax= maxx+PNL_DIST;
		v2d->tot.ymin= miny-PNL_DIST;
		v2d->tot.ymax= maxy+PNL_DIST;
	}
	else {
		v2d->tot.xmin= 0;
		v2d->tot.xmax= 1280;
		v2d->tot.ymin= 0;
		v2d->tot.ymax= 228;

		/* no panels, but old 'loose' buttons, as in old logic editor */
		for(block= ar->uiblocks.first; block; block= block->next) {
			//XXX 2.50 if(block->win==sa->win) {
				if(block->minx < v2d->tot.xmin) v2d->tot.xmin= block->minx;
				if(block->maxx > v2d->tot.xmax) v2d->tot.xmax= block->maxx; 
				if(block->miny < v2d->tot.ymin) v2d->tot.ymin= block->miny;
				if(block->maxy > v2d->tot.ymax) v2d->tot.ymax= block->maxy; 
			//XXX }
		}
	}
}

// make sure the panels are not outside 'tot' area
static void ui_panels_update_totrct(ARegion *ar)
{
	Panel *pa;
	uiBlock *block;
	View2D *v2d;
	int done=0;
	
	v2d= &ar->v2d;

	v2d->tot.xmin= 0.0f;
	v2d->tot.xmax= ar->winx;
	v2d->tot.ymax= 0.0f;
	v2d->tot.ymin= -ar->winy;
	
	for(pa= ar->panels.first; pa; pa=pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			done= 1;

			if(pa->ofsx < v2d->tot.xmin)
				v2d->tot.xmin= pa->ofsx;
			if(pa->ofsx+pa->sizex > v2d->tot.xmax)
				v2d->tot.xmax= pa->ofsx+pa->sizex;
			if(pa->ofsy < v2d->tot.ymin)
				v2d->tot.ymin= pa->ofsy;
			if(pa->ofsy+pa->sizey+PNL_HEADER > v2d->tot.ymax) 
				v2d->tot.ymax= pa->ofsy+pa->sizey+PNL_HEADER;
		}
	}

	if(done==0) {
		/* no panels, but old 'loose' buttons, as in old logic editor */
		for(block= ar->uiblocks.first; block; block= block->next) {
			//XXX 2.50 if(block->win==sa->win) {
				if(block->minx < v2d->tot.xmin) v2d->tot.xmin= block->minx;
				if(block->maxx > v2d->tot.xmax) v2d->tot.xmax= block->maxx; 
				if(block->miny < v2d->tot.ymin) v2d->tot.ymin= block->miny;
				if(block->maxy > v2d->tot.ymax) v2d->tot.ymax= block->maxy; 
			//XXX }
		}
	}	

	UI_view2d_totRect_set(v2d, v2d->tot.xmax, v2d->tot.ymin);
}

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

uiBlock *uiFindOpenPanelBlockName(ListBase *lb, char *name)
{
	uiBlock *block;
	
	for(block= lb->first; block; block= block->next) {
		if(block->panel && block->panel->active && block->panel->paneltab==NULL) {
			if(block->panel->flag & PNL_CLOSED);
			else if(strncmp(name, block->panel->panelname, UI_MAX_NAME_STR)==0) break;
		}
	}
	return block;
}

/* triangle 'icon' for panel header */
void ui_draw_tria_icon(float x, float y, char dir)
{
	if(dir=='h') {
		ui_draw_anti_tria( x-1, y, x-1, y+11.0, x+9, y+6.25);
	}
	else {
		ui_draw_anti_tria( x-3, y+10,  x+8-1, y+10, x+4.25-2, y);	
	}
}

void ui_draw_anti_x(float x1, float y1, float x2, float y2)
{

	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );

	glLineWidth(2.0);
	
	fdrawline(x1, y1, x2, y2);
	fdrawline(x1, y2, x2, y1);
	
	glLineWidth(1.0);
	
	glDisable( GL_LINE_SMOOTH );
	glDisable( GL_BLEND );
	
}

/* x 'icon' for panel header */
static void ui_draw_x_icon(float x, float y)
{
	UI_ThemeColor(TH_TEXT_HI);

	ui_draw_anti_x( x, y, x+9.375, y+9.375);

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

static void ui_draw_panel_dragwidget(rcti *rect)
{
	float xmin, xmax, dx;
	float ymin, ymax, dy;
	
	xmin= rect->xmax-10-PNL_HEADER+8;
	xmax= rect->xmax-10;
	ymin= rect->ymax+4;
	ymax= rect->ymax+PNL_HEADER-4;
	
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


static void ui_draw_panel_header_style(ARegion *ar, uiStyle *style, Panel *panel, rcti *rect)
{
	Panel *pa;
	rcti hrect;
	float width;
	int a, nr= 1, pnl_icons;
	char *activename= panel->drawname[0]?panel->drawname:panel->panelname;
	char *panelname;
	
	ui_draw_panel_dragwidget(rect);

	/* count */
	for(pa= ar->panels.first; pa; pa=pa->next)
		if(pa->active)
			if(pa->paneltab==panel)
				nr++;
	
	pnl_icons= PNL_ICON+8;
	if(panel->control & UI_PNL_CLOSE) pnl_icons+= PNL_ICON;
	
	if(nr==1) {
		
		/* active tab */
		/* draw text label */
		UI_ThemeColor(TH_TEXT);
		
		hrect.xmin= rect->xmin+pnl_icons;
		hrect.ymin= rect->ymax;
		hrect.xmax= rect->xmax;
		hrect.ymax= rect->ymax + PNL_HEADER;
		uiStyleFontDraw(&style->paneltitle, &hrect, activename);
		
		return;
	}
	
	a= 0;
	width= (panel->sizex - 3 - pnl_icons - PNL_ICON)/nr;
	for(pa= ar->panels.first; pa; pa=pa->next) {
		panelname= pa->drawname[0]?pa->drawname:pa->panelname;
		
		if(pa->active && (pa==panel || pa->paneltab==panel)) {
			/* active tab */
			if(pa==panel)
				UI_ThemeColor(TH_TEXT);
			else
				UI_ThemeColorBlend(TH_TEXT, TH_BACK, 0.5f);
			
			hrect.xmin= rect->xmin+pnl_icons + a*width;
			hrect.ymin= rect->ymax;
			hrect.xmax= hrect.xmin + width;
			hrect.ymax= hrect.ymin + PNL_HEADER;
			uiStyleFontDraw(&style->paneltitle, &hrect, panelname);
			
			a++;
		}
	}
}

void ui_draw_panel(ARegion *ar, uiStyle *style, uiBlock *block, rcti *rect)
{
	Panel *panel= block->panel, *prev;
	int ofsx;
	
	if(panel->paneltab) return;
	
	/* divider only when there's a previous panel */
	prev= panel->prev;
	while(prev) {
		if(prev->active) break;
		prev= prev->prev;
	}
	
	if(prev) {
		float minx= rect->xmin+10;
		float maxx= rect->xmax-10;
		float y= rect->ymax + PNL_HEADER;
		
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
		fdrawline(minx, y, maxx, y);
		glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
		fdrawline(minx, y-1, maxx, y-1);
		glDisable(GL_BLEND);
	}
	
	/* title */
	if(!(panel->flag & PNL_CLOSEDX)) {
		ui_draw_panel_header_style(ar, style, panel, rect);
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
			
			UI_ThemeColorShade(TH_HEADER, -120);
			uiRoundRect(rect->xmin, rect->ymin, rect->xmax, rect->ymax+PNL_HEADER, 8);
		}
		if(panel->flag & PNL_OVERLAP) {
			if(panel->control & UI_PNL_SOLID) uiSetRoundBox(15);
			else uiSetRoundBox(3);
			
			UI_ThemeColor(TH_TEXT_HI);
			uiRoundRect(rect->xmin, rect->ymin, rect->xmax, rect->ymax+PNL_HEADER, 8);
		}
		
		if(panel->control & UI_PNL_SCALE)
			ui_draw_panel_scalewidget(rect);
	}
	
	/* draw optional close icon */
	
	ofsx= 6;
	if(panel->control & UI_PNL_CLOSE) {
		
		ui_draw_x_icon(rect->xmin+2+ofsx, rect->ymax+2);
		ofsx= 22;
	}
	
	/* draw collapse icon */
	UI_ThemeColor(TH_TEXT);
	
	if(panel->flag & PNL_CLOSEDY)
		ui_draw_tria_icon(rect->xmin+6+ofsx, rect->ymax+3, 'h');
	else if(panel->flag & PNL_CLOSEDX)
		ui_draw_tria_icon(rect->xmin+7, rect->ymax+3, 'h');
	else
		ui_draw_tria_icon(rect->xmin+6+ofsx, rect->ymax+3, 'v');
	
	
}


/* ------------ panel alignment ---------------- */


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
	if(pa->flag & PNL_CLOSEDX) return pa->ofsx+PNL_HEADER;
	else if(pa->paneltab && (pa->paneltab->flag & PNL_CLOSEDX)) return pa->ofsx+PNL_HEADER;
	else return pa->ofsx+pa->sizex;
}


typedef struct PanelSort {
	Panel *pa, *orig;
} PanelSort;

/* note about sorting;
   the sortcounter has a lower value for new panels being added.
   however, that only works to insert a single panel, when more new panels get
   added the coordinates of existing panels and the previously stored to-be-insterted
   panels do not match for sorting */

static int find_leftmost_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsx > ps2->pa->ofsx) return 1;
	else if( ps1->pa->ofsx < ps2->pa->ofsx) return -1;
	else if( ps1->pa->sortcounter > ps2->pa->sortcounter) return 1;
	else if( ps1->pa->sortcounter < ps2->pa->sortcounter) return -1;

	return 0;
}


static int find_highest_panel(const void *a1, const void *a2)
{
	const PanelSort *ps1=a1, *ps2=a2;
	
	if( ps1->pa->ofsy+ps1->pa->sizey < ps2->pa->ofsy+ps2->pa->sizey) return 1;
	else if( ps1->pa->ofsy+ps1->pa->sizey > ps2->pa->ofsy+ps2->pa->sizey) return -1;
	else if( ps1->pa->sortcounter > ps2->pa->sortcounter) return 1;
	else if( ps1->pa->sortcounter < ps2->pa->sortcounter) return -1;
	
	return 0;
}

/* this doesnt draw */
/* returns 1 when it did something */
int uiAlignPanelStep(ScrArea *sa, ARegion *ar, float fac)
{
	Panel *pa;
	PanelSort *ps, *panelsort, *psnext;
	static int sortcounter= 0;
	int a, tot=0, done;
	int align= panel_aligned(sa, ar);
	
	/* count active, not tabbed Panels */
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) tot++;
	}

	if(tot==0) return 0;

	/* extra; change close direction? */
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			if( (pa->flag & PNL_CLOSEDX) && (align==BUT_VERTICAL) )
				pa->flag ^= PNL_CLOSED;
			
			else if( (pa->flag & PNL_CLOSEDY) && (align==BUT_HORIZONTAL) )
				pa->flag ^= PNL_CLOSED;
			
		}
	}

	panelsort= MEM_callocN( tot*sizeof(PanelSort), "panelsort");
	
	/* fill panelsort array */
	ps= panelsort;
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if(pa->active && pa->paneltab==NULL) {
			ps->pa= MEM_dupallocN(pa);
			ps->orig= pa;
			ps++;
		}
	}
	
	if(align==BUT_VERTICAL) 
		qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
	else
		qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);
	
	/* no smart other default start loc! this keeps switching f5/f6/etc compatible */
	ps= panelsort;
	ps->pa->ofsx= PNL_DIST;
	if(align==BUT_VERTICAL)
		ps->pa->ofsy= -ps->pa->sizey-PNL_HEADER-PNL_DIST;
	else
		ps->pa->ofsy= -ps->pa->sizey-PNL_HEADER-PNL_DIST; // XXX was 0;

	for(a=0 ; a<tot-1; a++, ps++) {
		psnext= ps+1;
	
		if(align==BUT_VERTICAL) {
			psnext->pa->ofsx = ps->pa->ofsx;
			psnext->pa->ofsy = get_panel_real_ofsy(ps->pa) - psnext->pa->sizey-PNL_HEADER-PNL_DIST;
		}
		else {
			psnext->pa->ofsx = get_panel_real_ofsx(ps->pa)+PNL_DIST;
			psnext->pa->ofsy = ps->pa->ofsy + ps->pa->sizey - psnext->pa->sizey;
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
	for(pa= ar->panels.first; pa; pa= pa->next) {
		if(pa->paneltab && pa->active) {
			copy_panel_offset(pa, pa->paneltab);
		}
	}

	/* set counter, used for sorting with newly added panels */
	sortcounter++;
	for(pa= ar->panels.first; pa; pa= pa->next)
		if(pa->active)
			pa->sortcounter= sortcounter;

	/* free panelsort array */
	for(ps= panelsort, a=0; a<tot; a++, ps++)
		MEM_freeN(ps->pa);
	MEM_freeN(panelsort);
	
	return done;
}


static void ui_do_animate(bContext *C, Panel *panel)
{
	uiHandlePanelData *data= panel->activedata;
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	float fac;

	fac= (PIL_check_seconds_timer()-data->starttime)/ANIMATION_TIME;
	fac= sqrt(fac);
	fac= MIN2(fac, 1.0f);

	/* for max 1 second, interpolate positions */
	if(uiAlignPanelStep(sa, ar, fac))
		ED_region_tag_redraw(ar);
	else
		fac= 1.0f;

	if(fac == 1.0f) {
		panel_activate_state(C, panel, PANEL_STATE_EXIT);
		return;
	}
}

void uiBeginPanels(const bContext *C, ARegion *ar)
{
	Panel *pa;
  
  	/* set all panels as inactive, so that at the end we know
	 * which ones were used */
	for(pa=ar->panels.first; pa; pa=pa->next)
		pa->active= 0;
}

/* only draws blocks with panels */
void uiEndPanels(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	uiBlock *block;
	Panel *panot, *panew, *patest;
	
	/* scaling contents */
	for(block= ar->uiblocks.first; block; block= block->next)
		if(block->active && block->panel)
			ui_scale_panel_block(block);

	/* consistancy; are panels not made, whilst they have tabs */
	for(panot= ar->panels.first; panot; panot= panot->next) {
		if(panot->active==0) { // not made

			for(panew= ar->panels.first; panew; panew= panew->next) {
				if(panew->active) {
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

	/* re-align */
	if(panels_re_align(sa, ar))
		uiAlignPanelStep(sa, ar, 1.0);
	
	if(sa->spacetype!=SPACE_BUTS) {
#if 0 // XXX make float panel exception
		SpaceLink *sl= sa->spacedata.first;
		for(block= ar->uiblocks.first; block; block= block->next) {
			if(block->active && block->panel && block->panel->active && block->panel->paneltab == NULL) {
				float dx=0.0, dy=0.0, minx, miny, maxx, maxy, miny_panel;
				
				minx= sl->blockscale*block->panel->ofsx;
				maxx= sl->blockscale*(block->panel->ofsx+block->panel->sizex);
				miny= sl->blockscale*(block->panel->ofsy+block->panel->sizey);
				maxy= sl->blockscale*(block->panel->ofsy+block->panel->sizey+PNL_HEADER);
				miny_panel= sl->blockscale*(block->panel->ofsy);
				
				/* check to see if snapped panels have been left out in the open by resizing a window
				 * and if so, offset them back to where they belong */
				if (block->panel->snap) {
					if (((block->panel->snap) & PNL_SNAP_RIGHT) &&
						(maxx < (float)sa->winx)) {
						
						dx = sa->winx-maxx;
						block->panel->ofsx+= dx/sl->blockscale;
					}
					if (((block->panel->snap) & PNL_SNAP_TOP) &&
						(maxy < (float)sa->winy)) {
						
						dy = sa->winy-maxy;
						block->panel->ofsy+= dy/sl->blockscale;
					}
					
					/* reset these vars with updated panel offset distances */
					minx= sl->blockscale*block->panel->ofsx;
					maxx= sl->blockscale*(block->panel->ofsx+block->panel->sizex);
					miny= sl->blockscale*(block->panel->ofsy+block->panel->sizey);
					maxy= sl->blockscale*(block->panel->ofsy+block->panel->sizey+PNL_HEADER);
					miny_panel= sl->blockscale*(block->panel->ofsy);
				} else
					/* reset to no snapping */
					block->panel->snap = PNL_SNAP_NONE;
	

				/* clip panels (headers) for non-butspace situations (maybe make optimized event later) */
				
				/* check left and right edges */
				if (minx < PNL_SNAP_DIST) {
					dx = -minx;
					block->panel->snap |= PNL_SNAP_LEFT;
				}
				else if (maxx > ((float)sa->winx - PNL_SNAP_DIST)) {
					dx= sa->winx-maxx;
					block->panel->snap |= PNL_SNAP_RIGHT;
				}				
				if( minx + dx < 0.0) dx= -minx; // when panel cant fit, put it fixed here
				
				/* check top and bottom edges */
				if ((miny_panel < PNL_SNAP_DIST) && (miny_panel > -PNL_SNAP_DIST)) {
					dy= -miny_panel;
					block->panel->snap |= PNL_SNAP_BOTTOM;
				}
				if(miny < PNL_SNAP_DIST)  {
					dy= -miny;
					block->panel->snap |= PNL_SNAP_BOTTOM;
				}
				else if(maxy > ((float)sa->winy - PNL_SNAP_DIST)) {
					dy= sa->winy-maxy;
					block->panel->snap |= PNL_SNAP_TOP;
				}
				if( miny + dy < 0.0) dy= -miny; // when panel cant fit, put it fixed here

				
				block->panel->ofsx+= dx/sl->blockscale;
				block->panel->ofsy+= dy/sl->blockscale;

				/* copy locations */
				for(patest= ar->panels.first; patest; patest= patest->next) {
					if(patest->paneltab==block->panel) copy_panel_offset(patest, block->panel);
				}
				
			}
		}
#endif
	}

	/* update v2d->totrct and update view */
	ui_panels_update_totrct(ar);
	UI_view2d_view_restore(C);
	UI_view2d_view_ortho(C, &ar->v2d);

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
			if(pa->paneltab==NULL && pa->active) {
				float safex= 0.2, safey= 0.2;
				
				if( pa->flag & PNL_CLOSEDX) safex= 0.05;
				else if(pa->flag & PNL_CLOSEDY) safey= 0.05;
				else if( panel->flag & PNL_CLOSEDX) safex= 0.05;
				else if(panel->flag & PNL_CLOSEDY) safey= 0.05;
				
				if( pa->ofsx > panel->ofsx- safex*panel->sizex)
				if( pa->ofsx+pa->sizex < panel->ofsx+ (1.0+safex)*panel->sizex)
				if( pa->ofsy > panel->ofsy- safey*panel->sizey)
				if( pa->ofsy+pa->sizey < panel->ofsy+ (1.0+safey)*panel->sizey)
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
		if(pa->active) {
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
				copy_panel_offset(pa, pasel);
			}
			pa= pa->next;
		}
	}
	
	if(pasel==NULL || palap==NULL) return;
	
	/* the overlapped panel becomes a tab */
	palap->paneltab= pasel;
	
	/* the selected panel gets coords of overlapped one */
	copy_panel_offset(pasel, palap);

	/* and its tabs */
	pa= ar->panels.first;
	while(pa) {
		if(pa->paneltab == pasel) {
			copy_panel_offset(pa, palap);
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

/* ------------ panel drag ---------------- */

static void ui_do_drag(bContext *C, wmEvent *event, Panel *panel)
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
	
	if(data->state == PANEL_STATE_DRAG_SCALE) {
		panel->sizex = MAX2(data->startsizex+dx, UI_PANEL_MINX);
		
		if(data->startsizey-dy < UI_PANEL_MINY) {
			dy= -UI_PANEL_MINY+data->startsizey;
		}
		panel->sizey = data->startsizey-dy;
		
		panel->ofsy= data->startofsy+dy;

	}
	else {
		/* reset the panel snapping, to allow dragging away from snapped edges */
		panel->snap = PNL_SNAP_NONE;
		
		panel->ofsx = data->startofsx+dx;
		panel->ofsy = data->startofsy+dy;
		check_panel_overlap(ar, panel);
		
		if(align) uiAlignPanelStep(sa, ar, 0.2);
	}

	ED_region_tag_redraw(ar);
}

static void ui_do_untab(bContext *C, wmEvent *event, Panel *panel)
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

/* region level panel interaction */

static void panel_clicked_tabs(bContext *C, ScrArea *sa, ARegion *ar, uiBlock *block,  int mousex)
{
	Panel *pa, *tabsel=NULL, *panel= block->panel;
	int nr= 1, a, width, ofsx;
	
	ofsx= PNL_ICON;
	if(block->panel->control & UI_PNL_CLOSE) ofsx+= PNL_ICON;
	
	/* count */
	for(pa= ar->panels.first; pa; pa=pa->next)
		if(pa!=panel)
			if(pa->active && pa->paneltab==panel)
				nr++;

	if(nr==1) return;
	
	/* find clicked tab, mouse in panel coords */
	a= 0;
	width= (int)((float)(panel->sizex - ofsx-10)/nr);
	pa= ar->panels.first;
	while(pa) {
		if(pa==panel || (pa->active && pa->paneltab==panel)) {
			if( (mousex > ofsx+a*width) && (mousex < ofsx+(a+1)*width) ) {
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
				if(pa->paneltab && pa->active) {
					copy_panel_offset(pa, pa->paneltab);
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
static void ui_handle_panel_header(bContext *C, uiBlock *block, int mx, int my)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Panel *pa;
	int align= panel_aligned(sa, ar), button= 0;

	/* mouse coordinates in panel space! */
	
	/* check open/collapsed button */
	if(block->panel->flag & PNL_CLOSEDX) {
		if(my >= block->maxy) button= 1;
	}
	else if(block->panel->control & UI_PNL_CLOSE) {
		if(mx <= block->minx+PNL_ICON-2) button= 2;
		else if(mx <= block->minx+2*PNL_ICON+2) button= 1;
	}
	else if(mx <= block->minx+PNL_ICON+2) {
		button= 1;
	}
	
	if(button) {
		if(button==2) { // close
			//XXX 2.50 rem_blockhandler(sa, block->handler);
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
	int retval, mx, my, inside_header= 0, inside_scale= 0, inside;

	retval= WM_UI_HANDLER_CONTINUE;

	for(block=ar->uiblocks.last; block; block=block->prev) {
		mx= event->x;
		my= event->y;
		ui_window_to_block(ar, block, &mx, &my);

		/* check if inside boundbox */
		inside= 0;

		if(block->panel && block->panel->paneltab==NULL)
			if(block->minx <= mx && block->maxx >= mx)
				if(block->miny <= my && block->maxy+PNL_HEADER >= my)
					inside= 1;

		if(inside) {
			/* clicked at panel header? */
			if(block->panel->flag & PNL_CLOSEDX) {
				if(block->minx <= mx && block->minx+PNL_HEADER >= mx) 
					inside_header= 1;
			}
			else if((block->maxy <= my) && (block->maxy+PNL_HEADER >= my)) {
				inside_header= 1;
			}
			else if(block->panel->control & UI_PNL_SCALE) {
				if(block->maxx-PNL_HEADER <= mx)
					if(block->miny+PNL_HEADER >= my)
						inside_scale= 1;
			}

			if(event->val==KM_PRESS) {
				if(event->type == LEFTMOUSE) {
					if(inside_header)
						ui_handle_panel_header(C, block, mx, my);
					else if(inside_scale && !(block->panel->flag & PNL_CLOSED))
						panel_activate_state(C, block->panel, PANEL_STATE_DRAG_SCALE);
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
					if (block->panel->flag & (PNL_CLOSEDX|PNL_CLOSEDY)) {
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
							if(!(block->panel->control & UI_PNL_SCALE)) {
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

/* window level modal panel interaction */

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

		return WM_UI_HANDLER_BREAK;
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

static void panel_activate_state(bContext *C, Panel *pa, uiHandlePanelState state)
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

