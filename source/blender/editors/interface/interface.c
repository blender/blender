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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation 2002-2008, full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
 
#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_unit.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"
#include "wm_window.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "BPY_extern.h"

#include "interface_intern.h"

#define MENU_WIDTH 			120
#define MENU_ITEM_HEIGHT	20
#define MENU_SEP_HEIGHT		6

/* 
 * a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt
 * 
 * uiBlahBlah()		external function
 * ui_blah_blah()	internal function
 */

static void ui_free_but(const bContext *C, uiBut *but);

/* ************* translation ************** */

int ui_translate_buttons()
{
	return (U.transopts & USER_TR_BUTTONS);
}

int ui_translate_menus()
{
	return (U.transopts & USER_TR_MENUS);
}

int ui_translate_tooltips()
{
	return (U.transopts & USER_TR_TOOLTIPS);
}

/* ************* window matrix ************** */

void ui_block_to_window_fl(const ARegion *ar, uiBlock *block, float *x, float *y)
{
	float gx, gy;
	int sx, sy, getsizex, getsizey;

	getsizex= ar->winrct.xmax-ar->winrct.xmin+1;
	getsizey= ar->winrct.ymax-ar->winrct.ymin+1;
	sx= ar->winrct.xmin;
	sy= ar->winrct.ymin;

	gx= *x;
	gy= *y;

	if(block->panel) {
		gx += block->panel->ofsx;
		gy += block->panel->ofsy;
	}

	*x= ((float)sx) + ((float)getsizex)*(0.5+ 0.5*(gx*block->winmat[0][0]+ gy*block->winmat[1][0]+ block->winmat[3][0]));
	*y= ((float)sy) + ((float)getsizey)*(0.5+ 0.5*(gx*block->winmat[0][1]+ gy*block->winmat[1][1]+ block->winmat[3][1]));
}

void ui_block_to_window(const ARegion *ar, uiBlock *block, int *x, int *y)
{
	float fx, fy;

	fx= *x;
	fy= *y;

	ui_block_to_window_fl(ar, block, &fx, &fy);

	*x= (int)(fx+0.5f);
	*y= (int)(fy+0.5f);
}

void ui_block_to_window_rct(const ARegion *ar, uiBlock *block, rctf *graph, rcti *winr)
{
	rctf tmpr;

	tmpr= *graph;
	ui_block_to_window_fl(ar, block, &tmpr.xmin, &tmpr.ymin);
	ui_block_to_window_fl(ar, block, &tmpr.xmax, &tmpr.ymax);

	winr->xmin= tmpr.xmin;
	winr->ymin= tmpr.ymin;
	winr->xmax= tmpr.xmax;
	winr->ymax= tmpr.ymax;
}

void ui_window_to_block_fl(const ARegion *ar, uiBlock *block, float *x, float *y)	/* for mouse cursor */
{
	float a, b, c, d, e, f, px, py;
	int sx, sy, getsizex, getsizey;

	getsizex= ar->winrct.xmax-ar->winrct.xmin+1;
	getsizey= ar->winrct.ymax-ar->winrct.ymin+1;
	sx= ar->winrct.xmin;
	sy= ar->winrct.ymin;

	a= .5*((float)getsizex)*block->winmat[0][0];
	b= .5*((float)getsizex)*block->winmat[1][0];
	c= .5*((float)getsizex)*(1.0+block->winmat[3][0]);

	d= .5*((float)getsizey)*block->winmat[0][1];
	e= .5*((float)getsizey)*block->winmat[1][1];
	f= .5*((float)getsizey)*(1.0+block->winmat[3][1]);

	px= *x - sx;
	py= *y - sy;

	*y=  (a*(py-f) + d*(c-px))/(a*e-d*b);
	*x= (px- b*(*y)- c)/a;

	if(block->panel) {
		*x -= block->panel->ofsx;
		*y -= block->panel->ofsy;
	}
}

void ui_window_to_block(const ARegion *ar, uiBlock *block, int *x, int *y)
{
	float fx, fy;

	fx= *x;
	fy= *y;

	ui_window_to_block_fl(ar, block, &fx, &fy);

	*x= (int)(fx+0.5f);
	*y= (int)(fy+0.5f);
}

void ui_window_to_region(const ARegion *ar, int *x, int *y)
{
	*x-= ar->winrct.xmin;
	*y-= ar->winrct.ymin;
}

/* ******************* block calc ************************* */

void ui_block_translate(uiBlock *block, int x, int y)
{
	uiBut *bt;

	for(bt= block->buttons.first; bt; bt=bt->next) {
		bt->x1 += x;
		bt->y1 += y;
		bt->x2 += x;
		bt->y2 += y;
	}

	block->minx += x;
	block->miny += y;
	block->maxx += x;
	block->maxy += y;
}

static void ui_text_bounds_block(uiBlock *block, float offset)
{
	uiStyle *style= U.uistyles.first;	// XXX pass on as arg
	uiBut *bt;
	int i = 0, j, x1addval= offset, nextcol;
	int lastcol= 0, col= 0;
	
	uiStyleFontSet(&style->widget);
	
	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(bt->type!=SEPR) {
			//int transopts= ui_translate_buttons();
			//if(bt->type==TEX || bt->type==IDPOIN) transopts= 0;
			
			j= BLF_width(bt->drawstr);

			if(j > i) i = j;
		}

		if(bt->next && bt->x1 < bt->next->x1)
			lastcol++;
	}

	/* cope with multi collumns */
	bt= block->buttons.first;
	while(bt) {
		if(bt->next && bt->x1 < bt->next->x1) {
			nextcol= 1;
			col++;
		}
		else nextcol= 0;
		
		bt->x1 = x1addval;
		bt->x2 = bt->x1 + i + block->bounds;
		
		if(col == lastcol)
			bt->x2= MAX2(bt->x2, offset + block->minbounds);

		ui_check_but(bt);	// clips text again
		
		if(nextcol)
			x1addval+= i + block->bounds;
		
		bt= bt->next;
	}
}

void ui_bounds_block(uiBlock *block)
{
	uiBut *bt;
	int xof;
	
	if(block->buttons.first==NULL) {
		if(block->panel) {
			block->minx= 0.0; block->maxx= block->panel->sizex;
			block->miny= 0.0; block->maxy= block->panel->sizey;
		}
	}
	else {
	
		block->minx= block->miny= 10000;
		block->maxx= block->maxy= -10000;
		
		bt= block->buttons.first;
		while(bt) {
			if(bt->x1 < block->minx) block->minx= bt->x1;
			if(bt->y1 < block->miny) block->miny= bt->y1;
	
  			if(bt->x2 > block->maxx) block->maxx= bt->x2;
			if(bt->y2 > block->maxy) block->maxy= bt->y2;

			bt= bt->next;
		}
		
		block->minx -= block->bounds;
		block->miny -= block->bounds;
		block->maxx += block->bounds;
		block->maxy += block->bounds;
	}

	block->maxx= block->minx + MAX2(block->maxx - block->minx, block->minbounds);

	/* hardcoded exception... but that one is annoying with larger safety */ 
	bt= block->buttons.first;
	if(bt && strncmp(bt->str, "ERROR", 5)==0) xof= 10;
	else xof= 40;
	
	block->safety.xmin= block->minx-xof;
	block->safety.ymin= block->miny-xof;
	block->safety.xmax= block->maxx+xof;
	block->safety.ymax= block->maxy+xof;
}

static void ui_centered_bounds_block(const bContext *C, uiBlock *block)
{
	wmWindow *window= CTX_wm_window(C);
	int xmax, ymax;
	int startx, starty;
	int width, height;
	
	/* note: this is used for the splash where window bounds event has not been
	 * updated by ghost, get the window bounds from ghost directly */

	// wm_window_get_size(window, &xmax, &ymax);
	wm_window_get_size_ghost(window, &xmax, &ymax);
	
	ui_bounds_block(block);
	
	width= block->maxx - block->minx;
	height= block->maxy - block->miny;
	
	startx = (xmax * 0.5f) - (width * 0.5f);
	starty = (ymax * 0.5f) - (height * 0.5f);
	
	ui_block_translate(block, startx - block->minx, starty - block->miny);
	
	/* now recompute bounds and safety */
	ui_bounds_block(block);
	
}
static void ui_popup_bounds_block(const bContext *C, uiBlock *block, int bounds_calc)
{
	wmWindow *window= CTX_wm_window(C);
	int startx, starty, endx, endy, width, height, oldwidth, oldheight;
	int oldbounds, xmax, ymax;

	oldbounds= block->bounds;

	/* compute mouse position with user defined offset */
	ui_bounds_block(block);
	
	wm_window_get_size(window, &xmax, &ymax);

	oldwidth= block->maxx - block->minx;
	oldheight= block->maxy - block->miny;

	/* first we ensure wide enough text bounds */
	if(bounds_calc==UI_BLOCK_BOUNDS_POPUP_MENU) {
		if(block->flag & UI_BLOCK_LOOP) {
			block->bounds= 50;
			ui_text_bounds_block(block, block->minx);
		}
	}

	/* next we recompute bounds */
	block->bounds= oldbounds;
	ui_bounds_block(block);

	/* and we adjust the position to fit within window */
	width= block->maxx - block->minx;
	height= block->maxy - block->miny;
    
    /* avoid divide by zero below, caused by calling with no UI, but better not crash */
    oldwidth= oldwidth > 0 ? oldwidth : MAX2(1, width);
    oldheight= oldheight > 0 ? oldheight : MAX2(1, height);

	/* offset block based on mouse position, user offset is scaled
	   along in case we resized the block in ui_text_bounds_block */
	startx= window->eventstate->x + block->minx + (block->mx*width)/oldwidth;
	starty= window->eventstate->y + block->miny + (block->my*height)/oldheight;

	if(startx<10)
		startx= 10;
	if(starty<10)
		starty= 10;
	
	endx= startx+width;
	endy= starty+height;
	
	if(endx>xmax) {
		endx= xmax-10;
		startx= endx-width;
	}
	if(endy>ymax-20) {
		endy= ymax-20;
		starty= endy-height;
	}

	ui_block_translate(block, startx - block->minx, starty - block->miny);

	/* now recompute bounds and safety */
	ui_bounds_block(block);
}

/* used for various cases */
void uiBoundsBlock(uiBlock *block, int addval)
{
	if(block==NULL)
		return;
	
	block->bounds= addval;
	block->dobounds= UI_BLOCK_BOUNDS;
}

/* used for pulldowns */
void uiTextBoundsBlock(uiBlock *block, int addval)
{
	block->bounds= addval;
	block->dobounds= UI_BLOCK_BOUNDS_TEXT;
}

/* used for block popups */
void uiPopupBoundsBlock(uiBlock *block, int addval, int mx, int my)
{
	block->bounds= addval;
	block->dobounds= UI_BLOCK_BOUNDS_POPUP_MOUSE;
	block->mx= mx;
	block->my= my;
}

/* used for menu popups */
void uiMenuPopupBoundsBlock(uiBlock *block, int addval, int mx, int my)
{
	block->bounds= addval;
	block->dobounds= UI_BLOCK_BOUNDS_POPUP_MENU;
	block->mx= mx;
	block->my= my;
}

/* used for centered popups, i.e. splash */
void uiCenteredBoundsBlock(uiBlock *block, int addval)
{
	block->bounds= addval;
	block->dobounds= UI_BLOCK_BOUNDS_POPUP_CENTER;
}

/* ************** LINK LINE DRAWING  ************* */

/* link line drawing is not part of buttons or theme.. so we stick with it here */

static void ui_draw_linkline(uiBut *but, uiLinkLine *line)
{
	rcti rect;

	if(line->from==NULL || line->to==NULL) return;
	
	rect.xmin= (line->from->x1+line->from->x2)/2.0;
	rect.ymin= (line->from->y1+line->from->y2)/2.0;
	rect.xmax= (line->to->x1+line->to->x2)/2.0;
	rect.ymax= (line->to->y1+line->to->y2)/2.0;
	
	if(line->flag & UI_SELECT) 
		glColor3ub(100,100,100);
	else 
		glColor3ub(0,0,0);

	ui_draw_link_bezier(&rect);
}

static void ui_draw_links(uiBlock *block)
{
	uiBut *but;
	uiLinkLine *line;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				ui_draw_linkline(but, line);
				line= line->next;
			}
		}
		but= but->next;
	}	
}

/* ************** BLOCK ENDING FUNCTION ************* */

/* NOTE: if but->poin is allocated memory for every defbut, things fail... */
static int ui_but_equals_old(uiBut *but, uiBut *oldbut)
{
	/* various properties are being compared here, hopfully sufficient
	 * to catch all cases, but it is simple to add more checks later */
	if(but->retval != oldbut->retval) return 0;
	if(but->rnapoin.data != oldbut->rnapoin.data) return 0;
	if(but->rnaprop != oldbut->rnaprop)
		if(but->rnaindex != oldbut->rnaindex) return 0;
	if(but->func != oldbut->func) return 0;
	if(but->funcN != oldbut->funcN) return 0;
	if(oldbut->func_arg1 != oldbut && but->func_arg1 != oldbut->func_arg1) return 0;
	if(oldbut->func_arg2 != oldbut && but->func_arg2 != oldbut->func_arg2) return 0;
	if(!but->funcN && ((but->poin != oldbut->poin && (uiBut*)oldbut->poin != oldbut) || but->pointype != oldbut->pointype)) return 0;

	return 1;
}

static int ui_but_update_from_old_block(const bContext *C, uiBlock *block, uiBut *but)
{
	uiBlock *oldblock;
	uiBut *oldbut;
	int found= 0;

	oldblock= block->oldblock;
	if(!oldblock)
		return found;

	for(oldbut=oldblock->buttons.first; oldbut; oldbut=oldbut->next) {
		if(ui_but_equals_old(oldbut, but)) {
			if(oldbut->active) {
				but->flag= oldbut->flag;
				but->active= oldbut->active;
				but->pos= oldbut->pos;
				but->editstr= oldbut->editstr;
				but->editval= oldbut->editval;
				but->editvec= oldbut->editvec;
				but->editcoba= oldbut->editcoba;
				but->editcumap= oldbut->editcumap;
				but->selsta= oldbut->selsta;
				but->selend= oldbut->selend;
				but->softmin= oldbut->softmin;
				but->softmax= oldbut->softmax;
				but->linkto[0]= oldbut->linkto[0];
				but->linkto[1]= oldbut->linkto[1];
				found= 1;

				oldbut->active= NULL;
			}

			/* ensures one button can get activated, and in case the buttons
			 * draw are the same this gives O(1) lookup for each button */
			BLI_remlink(&oldblock->buttons, oldbut);
			ui_free_but(C, oldbut);
			
			break;
		}
	}
	
	return found;
}

/* needed for temporarily rename buttons, such as in outliner or fileselect,
   they should keep calling uiDefButs to keep them alive */
/* returns 0 when button removed */
int uiButActiveOnly(const bContext *C, uiBlock *block, uiBut *but)
{
	uiBlock *oldblock;
	uiBut *oldbut;
	int activate= 0, found= 0, isactive= 0;
	
	oldblock= block->oldblock;
	if(!oldblock)
		activate= 1;
	else {
		for(oldbut=oldblock->buttons.first; oldbut; oldbut=oldbut->next) {
			if(ui_but_equals_old(oldbut, but)) {
				found= 1;
				
				if(oldbut->active)
					isactive= 1;
				
				break;
			}
		}
	}
	if(activate || found==0) {
		ui_button_activate_do( (bContext *)C, CTX_wm_region(C), but);
	}
	else if(found && isactive==0) {
		
		BLI_remlink(&block->buttons, but);
		ui_free_but(C, but);
		return 0;
	}
	
	return 1;
}

void ui_menu_block_set_keymaps(const bContext *C, uiBlock *block)
{
	uiBut *but;
	IDProperty *prop;
	char buf[512], *butstr;

	/* only do it before bounding */
	if(block->minx != block->maxx)
		return;

	for(but=block->buttons.first; but; but=but->next) {
		if(but->optype) {
			prop= (but->opptr)? but->opptr->data: NULL;

			if(WM_key_event_operator_string(C, but->optype->idname, but->opcontext, prop, buf, sizeof(buf))) {
				butstr= MEM_mallocN(strlen(but->str)+strlen(buf)+2, "menu_block_set_keymaps");
				strcpy(butstr, but->str);
				strcat(butstr, "|");
				strcat(butstr, buf);

				but->str= but->strdata;
				BLI_strncpy(but->str, butstr, sizeof(but->strdata));
				MEM_freeN(butstr);

				ui_check_but(but);
			}
		}
	}
}

void uiEndBlock(const bContext *C, uiBlock *block)
{
	uiBut *but;
	Scene *scene= CTX_data_scene(C);

	/* inherit flags from 'old' buttons that was drawn here previous, based
	 * on matching buttons, we need this to make button event handling non
	 * blocking, while still alowing buttons to be remade each redraw as it
	 * is expected by blender code */
	for(but=block->buttons.first; but; but=but->next) {
		if(ui_but_update_from_old_block(C, block, but))
			ui_check_but(but);
		
		/* temp? Proper check for greying out */
		if(but->optype) {
			wmOperatorType *ot= but->optype;

			if(but->context)
				CTX_store_set((bContext*)C, but->context);

			if(ot == NULL || WM_operator_poll((bContext*)C, ot)==0) {
				but->flag |= UI_BUT_DISABLED;
				but->lock = 1;
			}

			if(but->context)
				CTX_store_set((bContext*)C, NULL);
		}

		ui_but_anim_flag(but, (scene)? scene->r.cfra: 0.0f);
	}

	if(block->oldblock) {
		block->auto_open= block->oldblock->auto_open;
		block->auto_open_last= block->oldblock->auto_open_last;
		block->tooltipdisabled= block->oldblock->tooltipdisabled;

		block->oldblock= NULL;
	}

	/* handle pending stuff */
	if(block->layouts.first) uiBlockLayoutResolve(block, NULL, NULL);
	ui_block_do_align(block);
	if(block->flag & UI_BLOCK_LOOP) ui_menu_block_set_keymaps(C, block);
	
	/* after keymaps! */
	if(block->dobounds == UI_BLOCK_BOUNDS) ui_bounds_block(block);
	else if(block->dobounds == UI_BLOCK_BOUNDS_TEXT) ui_text_bounds_block(block, 0.0f);
	else if(block->dobounds == UI_BLOCK_BOUNDS_POPUP_CENTER) ui_centered_bounds_block(C, block);
	else if(block->dobounds) ui_popup_bounds_block(C, block, block->dobounds);

	if(block->minx==0.0 && block->maxx==0.0) uiBoundsBlock(block, 0);
	if(block->flag & UI_BUT_ALIGN) uiBlockEndAlign(block);

	block->endblock= 1;
}

/* ************** BLOCK DRAWING FUNCTION ************* */

void ui_fontscale(short *points, float aspect)
{
	if(aspect < 0.9f || aspect > 1.1f) {
		float pointsf= *points;
		
		/* for some reason scaling fonts goes too fast compared to widget size */
		aspect= sqrt(aspect);
		pointsf /= aspect;
		
		if(aspect > 1.0)
			*points= ceil(pointsf);
		else
			*points= floor(pointsf);
	}
}

/* project button or block (but==NULL) to pixels in regionspace */
static void ui_but_to_pixelrect(rcti *rect, const ARegion *ar, uiBlock *block, uiBut *but)
{
	float gx, gy;
	float getsizex, getsizey;
	
	getsizex= ar->winx;
	getsizey= ar->winy;

	gx= (but?but->x1:block->minx) + (block->panel?block->panel->ofsx:0.0f);
	gy= (but?but->y1:block->miny) + (block->panel?block->panel->ofsy:0.0f);
	
	rect->xmin= floor(getsizex*(0.5+ 0.5*(gx*block->winmat[0][0]+ gy*block->winmat[1][0]+ block->winmat[3][0])));
	rect->ymin= floor(getsizey*(0.5+ 0.5*(gx*block->winmat[0][1]+ gy*block->winmat[1][1]+ block->winmat[3][1])));
	
	gx= (but?but->x2:block->maxx) + (block->panel?block->panel->ofsx:0.0f);
	gy= (but?but->y2:block->maxy) + (block->panel?block->panel->ofsy:0.0f);
	
	rect->xmax= floor(getsizex*(0.5+ 0.5*(gx*block->winmat[0][0]+ gy*block->winmat[1][0]+ block->winmat[3][0])));
	rect->ymax= floor(getsizey*(0.5+ 0.5*(gx*block->winmat[0][1]+ gy*block->winmat[1][1]+ block->winmat[3][1])));

}

/* uses local copy of style, to scale things down, and allow widgets to change stuff */
void uiDrawBlock(const bContext *C, uiBlock *block)
{
	uiStyle style= *((uiStyle *)U.uistyles.first);	// XXX pass on as arg
	ARegion *ar;
	uiBut *but;
	rcti rect;
	
	/* get menu region or area region */
	ar= CTX_wm_menu(C);
	if(!ar)
		ar= CTX_wm_region(C);

	if(!block->endblock)
		uiEndBlock(C, block);

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
	
	wmOrtho2(-0.01f, ar->winx-0.01f, -0.01f, ar->winy-0.01f);
	
	/* back */
	if(block->flag & UI_BLOCK_LOOP)
		ui_draw_menu_back(&style, block, &rect);
	else if(block->panel)
		ui_draw_aligned_panel(ar, &style, block, &rect);

	/* widgets */
	for(but= block->buttons.first; but; but= but->next) {
		ui_but_to_pixelrect(&rect, ar, block, but);
		if(!(but->flag & UI_HIDDEN))
			ui_draw_but(C, ar, &style, but, &rect);
	}
	
	/* restore matrix */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	ui_draw_links(block);
}

/* ************* EVENTS ************* */

static void ui_is_but_sel(uiBut *but)
{
	double value;
	int lvalue;
	short push=0, true=1;

	value= ui_get_but_val(but);

	if(ELEM3(but->type, TOGN, ICONTOGN, OPTIONN)) true= 0;

	if( but->bit ) {
		lvalue= (int)value;
		if( BTST(lvalue, (but->bitnr)) ) push= true;
		else push= !true;
	}
	else {
		switch(but->type) {
		case BUT:
			push= 2;
			break;
		case HOTKEYEVT:
		case KEYEVT:
			push= 2;
			break;
		case TOGBUT:
		case TOG:
		case TOGR:
		case TOG3:
		case BUT_TOGDUAL:
		case ICONTOG:
		case OPTION:
			if(value!=but->hardmin) push= 1;
			break;
		case ICONTOGN:
		case TOGN:
		case OPTIONN:
			if(value==0.0) push= 1;
			break;
		case ROW:
		case LISTROW:
			if(value == but->hardmax) push= 1;
			break;
		case COL:
			push= 2;
			break;
		default:
			push= 2;
			break;
		}
	}
	
	if(push==2);
	else if(push==1) but->flag |= UI_SELECT;
	else but->flag &= ~UI_SELECT;
}

/* XXX 2.50 no links supported yet */

static int uibut_contains_pt(uiBut *but, short *mval)
{
	return 0;

}

uiBut *ui_get_valid_link_button(uiBlock *block, uiBut *but, short *mval)
{
	uiBut *bt;
	
		/* find button to link to */
	for (bt= block->buttons.first; bt; bt= bt->next)
		if(bt!=but && uibut_contains_pt(bt, mval))
			break;

	if (bt) {
		if (but->type==LINK && bt->type==INLINK) {
			if( but->link->tocode == (int)bt->hardmin ) {
				return bt;
			}
		}
		else if(but->type==INLINK && bt->type==LINK) {
			if( bt->link->tocode == (int)but->hardmin ) {
				return bt;
			}
		}
	}

	return NULL;
}


static uiBut *ui_find_inlink(uiBlock *block, void *poin)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==INLINK) {
			if(but->poin == poin) return but;
		}
		but= but->next;
	}
	return NULL;
}

static void ui_add_link_line(ListBase *listb, uiBut *but, uiBut *bt)
{
	uiLinkLine *line;
	
	line= MEM_callocN(sizeof(uiLinkLine), "linkline");
	BLI_addtail(listb, line);
	line->from= but;
	line->to= bt;
}

uiBut *uiFindInlink(uiBlock *block, void *poin)
{
	return ui_find_inlink(block, poin);
}

void uiComposeLinks(uiBlock *block)
{
	uiBut *but, *bt;
	uiLink *link;
	void ***ppoin;
	int a;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK) {
			link= but->link;
			
			/* for all pointers in the array */
			if(link) {
				if(link->ppoin) {
					ppoin= link->ppoin;
					for(a=0; a < *(link->totlink); a++) {
						bt= ui_find_inlink(block, (*ppoin)[a] );
						if(bt) {
							ui_add_link_line(&link->lines, but, bt);
						}
					}
				}
				else if(link->poin) {
					bt= ui_find_inlink(block, *(link->poin) );
					if(bt) {
						ui_add_link_line(&link->lines, but, bt);
					}
				}
			}
		}
		but= but->next;
	}
}


/* ************************************************ */

void uiBlockSetButLock(uiBlock *block, int val, char *lockstr)
{
	if(val) {
		block->lock |= val;
		block->lockstr= lockstr;
	}
}

void uiBlockClearButLock(uiBlock *block)
{
	block->lock= 0;
	block->lockstr= NULL;
}

/* *************************************************************** */

void ui_delete_linkline(uiLinkLine *line, uiBut *but)
{
	uiLink *link;
	int a, b;
	
	BLI_remlink(&but->link->lines, line);

	link= line->from->link;

	/* are there more pointers allowed? */
	if(link->ppoin) {
		
		if(*(link->totlink)==1) {
			*(link->totlink)= 0;
			MEM_freeN(*(link->ppoin));
			*(link->ppoin)= NULL;
		}
		else {
			b= 0;
			for(a=0; a< (*(link->totlink)); a++) {
				
				if( (*(link->ppoin))[a] != line->to->poin ) {
					(*(link->ppoin))[b]= (*(link->ppoin))[a];
					b++;
				}
			}	
			(*(link->totlink))--;
		}
	}
	else {
		*(link->poin)= NULL;
	}

	MEM_freeN(line);
	//REDRAW
}
/* XXX 2.50 no links supported yet */
#if 0
static void ui_delete_active_linkline(uiBlock *block)
{
	uiBut *but;
	uiLink *link;
	uiLinkLine *line, *nline;
	int a, b;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				
				nline= line->next;
				
				if(line->flag & UI_SELECT) {
					BLI_remlink(&but->link->lines, line);

					link= line->from->link;

					/* are there more pointers allowed? */
					if(link->ppoin) {
						
						if(*(link->totlink)==1) {
							*(link->totlink)= 0;
							MEM_freeN(*(link->ppoin));
							*(link->ppoin)= NULL;
						}
						else {
							b= 0;
							for(a=0; a< (*(link->totlink)); a++) {
								
								if( (*(link->ppoin))[a] != line->to->poin ) {
									(*(link->ppoin))[b]= (*(link->ppoin))[a];
									b++;
								}
							}	
							(*(link->totlink))--;
						}
					}
					else {
						*(link->poin)= NULL;
					}

					MEM_freeN(line);
				}
				line= nline;
			}
		}
		but= but->next;
	}
	
	/* temporal! these buttons can be everywhere... */
	allqueue(REDRAWBUTSLOGIC, 0);
}

static void ui_do_active_linklines(uiBlock *block, short *mval)
{
	uiBut *but;
	uiLinkLine *line, *act= NULL;
	float mindist= 12.0, fac, v1[2], v2[2], v3[3];
	int foundone= 0; 
	
	if(mval) {
		v1[0]= mval[0];
		v1[1]= mval[1];
		
		/* find a line close to the mouse */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				foundone= 1;
				line= but->link->lines.first;
				while(line) {
					v2[0]= line->from->x2;
					v2[1]= (line->from->y1+line->from->y2)/2.0;
					v3[0]= line->to->x1;
					v3[1]= (line->to->y1+line->to->y2)/2.0;
					
					fac= dist_to_line_segment_v2(v1, v2, v3);
					if(fac < mindist) {
						mindist= fac;
						act= line;
					}
					line= line->next;
				}
			}
			but= but->next;
		}
	}

	/* check for a 'found one' to prevent going to 'frontbuffer' mode.
		this slows done gfx quite some, and at OSX the 'finish' forces a swapbuffer */
	if(foundone) {
		glDrawBuffer(GL_FRONT);
		
		/* draw */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				line= but->link->lines.first;
				while(line) {
					if(line==act) {
						if((line->flag & UI_SELECT)==0) {
							line->flag |= UI_SELECT;
							ui_draw_linkline(but, line);
						}
					}
					else if(line->flag & UI_SELECT) {
						line->flag &= ~UI_SELECT;
						ui_draw_linkline(but, line);
					}
					line= line->next;
				}
			}
			but= but->next;
		}
		bglFlush();
		glDrawBuffer(GL_BACK);
	}
}
#endif

/* ******************************************************* */

/* XXX 2.50 no screendump supported yet */

#if 0
/* nasty but safe way to store screendump rect */
static int scr_x=0, scr_y=0, scr_sizex=0, scr_sizey=0;

static void ui_set_screendump_bbox(uiBlock *block)
{
	if(block) {
		scr_x= block->minx;
		scr_y= block->miny;
		scr_sizex= block->maxx - block->minx;
		scr_sizey= block->maxy - block->miny;
	}
	else {
		scr_sizex= scr_sizey= 0;
	}
}

/* used for making screenshots for menus, called in screendump.c */
int uiIsMenu(int *x, int *y, int *sizex, int *sizey)
{
	if(scr_sizex!=0 && scr_sizey!=0) {
		*x= scr_x;
		*y= scr_y;
		*sizex= scr_sizex;
		*sizey= scr_sizey;
		return 1;
	}
	
	return 0;
}
#endif

/* *********************** data get/set ***********************
 * this either works with the pointed to data, or can work with
 * an edit override pointer while dragging for example */

/* for buttons pointing to color for example */
void ui_get_but_vectorf(uiBut *but, float *vec)
{
	PropertyRNA *prop;
	int a, tot;

	if(but->editvec) {
		VECCOPY(vec, but->editvec);
	}

	if(but->rnaprop) {
		prop= but->rnaprop;

		vec[0]= vec[1]= vec[2]= 0.0f;

		if(RNA_property_type(prop) == PROP_FLOAT) {
			tot= RNA_property_array_length(&but->rnapoin, prop);
			tot= MIN2(tot, 3);

			for(a=0; a<tot; a++)
				vec[a]= RNA_property_float_get_index(&but->rnapoin, prop, a);
		}
	}
	else if(but->pointype == CHA) {
		char *cp= (char *)but->poin;
		vec[0]= ((float)cp[0])/255.0;
		vec[1]= ((float)cp[1])/255.0;
		vec[2]= ((float)cp[2])/255.0;
	}
	else if(but->pointype == FLO) {
		float *fp= (float *)but->poin;
		VECCOPY(vec, fp);
	}
    else {
        if (but->editvec==NULL) {
            fprintf(stderr, "ui_get_but_vectorf: can't get color, should never happen\n");
            vec[0]= vec[1]= vec[2]= 0.0f;
        }
    }
}

/* for buttons pointing to color for example */
void ui_set_but_vectorf(uiBut *but, float *vec)
{
	PropertyRNA *prop;
	int a, tot;

	if(but->editvec) {
		VECCOPY(but->editvec, vec);
	}

	if(but->rnaprop) {
		prop= but->rnaprop;

		if(RNA_property_type(prop) == PROP_FLOAT) {
			tot= RNA_property_array_length(&but->rnapoin, prop);
			tot= MIN2(tot, 3);

			for(a=0; a<tot; a++)
				RNA_property_float_set_index(&but->rnapoin, prop, a, vec[a]);
		}
	}
	else if(but->pointype == CHA) {
		char *cp= (char *)but->poin;
		cp[0]= (char)(0.5 +vec[0]*255.0);
		cp[1]= (char)(0.5 +vec[1]*255.0);
		cp[2]= (char)(0.5 +vec[2]*255.0);
	}
	else if(but->pointype == FLO) {
		float *fp= (float *)but->poin;
		VECCOPY(fp, vec);
	}
}

int ui_is_but_float(uiBut *but)
{
	if(but->pointype==FLO && but->poin)
		return 1;
	
	if(but->rnaprop && RNA_property_type(but->rnaprop) == PROP_FLOAT)
		return 1;
	
	return 0;
}

int ui_is_but_unit(uiBut *but)
{
	Scene *scene= CTX_data_scene((bContext *)but->block->evil_C);
	int unit_type;
	
	if(but->rnaprop==NULL)
		return 0;
	
	unit_type = RNA_SUBTYPE_UNIT(RNA_property_subtype(but->rnaprop));
	
	if (scene->unit.flag & USER_UNIT_ROT_RADIANS && unit_type == PROP_UNIT_ROTATION)
		return 0;
	
	/* for now disable time unit conversion */	
	if (unit_type == PROP_UNIT_TIME)
		return 0;

	if (scene->unit.system == USER_UNIT_NONE) {
	   if (unit_type != PROP_UNIT_ROTATION)
			return 0;
	}

	if(unit_type == PROP_UNIT_NONE)
		return 0;

	return 1;
}

double ui_get_but_val(uiBut *but)
{
	PropertyRNA *prop;
	double value = 0.0;

	if(but->editval) { return *(but->editval); }
	if(but->poin==NULL && but->rnapoin.data==NULL) return 0.0;

	if(but->rnaprop) {
		prop= but->rnaprop;

		switch(RNA_property_type(prop)) {
			case PROP_BOOLEAN:
				if(RNA_property_array_length(&but->rnapoin, prop))
					value= RNA_property_boolean_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value= RNA_property_boolean_get(&but->rnapoin, prop);
				break;
			case PROP_INT:
				if(RNA_property_array_length(&but->rnapoin, prop))
					value= RNA_property_int_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value= RNA_property_int_get(&but->rnapoin, prop);
				break;
			case PROP_FLOAT:
				if(RNA_property_array_length(&but->rnapoin, prop))
					value= RNA_property_float_get_index(&but->rnapoin, prop, but->rnaindex);
				else
					value= RNA_property_float_get(&but->rnapoin, prop);
				break;
			case PROP_ENUM:
				value= RNA_property_enum_get(&but->rnapoin, prop);
				break;
			default:
				value= 0.0;
				break;
		}
	}
	else if(but->type== HSVSLI) {
		float h, s, v, *fp;
		
		fp= (but->editvec)? but->editvec: (float *)but->poin;
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);

		switch(but->str[0]) {
			case 'H': value= h; break;
			case 'S': value= s; break;
			case 'V': value= v; break;
		}
	} 
	else if( but->pointype == CHA ) {
		value= *(char *)but->poin;
	}
	else if( but->pointype == SHO ) {
		value= *(short *)but->poin;
	} 
	else if( but->pointype == INT ) {
		value= *(int *)but->poin;
	} 
	else if( but->pointype == FLO ) {
		value= *(float *)but->poin;
	}
    
	return value;
}

void ui_set_but_val(uiBut *but, double value)
{
	PropertyRNA *prop;

	/* value is a hsv value: convert to rgb */
	if(but->rnaprop) {
		prop= but->rnaprop;

		if(RNA_property_editable(&but->rnapoin, prop)) {
			switch(RNA_property_type(prop)) {
				case PROP_BOOLEAN:
					if(RNA_property_array_length(&but->rnapoin, prop))
						RNA_property_boolean_set_index(&but->rnapoin, prop, but->rnaindex, value);
					else
						RNA_property_boolean_set(&but->rnapoin, prop, value);
					break;
				case PROP_INT:
					if(RNA_property_array_length(&but->rnapoin, prop))
						RNA_property_int_set_index(&but->rnapoin, prop, but->rnaindex, value);
					else
						RNA_property_int_set(&but->rnapoin, prop, value);
					break;
				case PROP_FLOAT:
					if(RNA_property_array_length(&but->rnapoin, prop))
						RNA_property_float_set_index(&but->rnapoin, prop, but->rnaindex, value);
					else
						RNA_property_float_set(&but->rnapoin, prop, value);
					break;
				case PROP_ENUM:
					RNA_property_enum_set(&but->rnapoin, prop, value);
					break;
				default:
					break;
			}
		}
	}
	else if(but->pointype==0);
	else if(but->type==HSVSLI ) {
		float h, s, v, *fp;
		
		fp= (but->editvec)? but->editvec: (float *)but->poin;
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);
		
		switch(but->str[0]) {
		case 'H': h= value; break;
		case 'S': s= value; break;
		case 'V': v= value; break;
		}
		
		hsv_to_rgb(h, s, v, fp, fp+1, fp+2);
		
	}
	else {
		/* first do rounding */
		if(but->pointype==CHA)
			value= (char)floor(value+0.5);
		else if(but->pointype==SHO ) {
			/* gcc 3.2.1 seems to have problems 
			 * casting a double like 32772.0 to
			 * a short so we cast to an int, then 
			 to a short */
			int gcckludge;
			gcckludge = (int) floor(value+0.5);
			value= (short)gcckludge;
		}
		else if(but->pointype==INT )
			value= (int)floor(value+0.5);
		else if(but->pointype==FLO ) {
			float fval= (float)value;
			if(fval>= -0.00001f && fval<= 0.00001f) fval= 0.0f;	/* prevent negative zero */
			value= fval;
		}
		
		/* then set value with possible edit override */
		if(but->editval)
			*but->editval= value;
		else if(but->pointype==CHA)
			*((char *)but->poin)= (char)value;
		else if(but->pointype==SHO)
			*((short *)but->poin)= (short)value;
		else if(but->pointype==INT)
			*((int *)but->poin)= (int)value;
		else if(but->pointype==FLO)
			*((float *)but->poin)= (float)value;
	}

	/* update select flag */
	ui_is_but_sel(but);
}

int ui_get_but_string_max_length(uiBut *but)
{
	if(ELEM(but->type, TEX, SEARCH_MENU))
		return but->hardmax;
	else if(but->type == IDPOIN)
		return sizeof(((ID*)NULL)->name)-2;
	else
		return UI_MAX_DRAW_STR;
}

static double ui_get_but_scale_unit(uiBut *but, double value)
{
	Scene *scene= CTX_data_scene((bContext *)but->block->evil_C);
	int subtype= RNA_SUBTYPE_UNIT(RNA_property_subtype(but->rnaprop));

	if(subtype == PROP_UNIT_LENGTH) {
		return value * scene->unit.scale_length;
	}
	else if(subtype == PROP_UNIT_AREA) {
		return value * pow(scene->unit.scale_length, 2);
	}
	else if(subtype == PROP_UNIT_VOLUME) {
		return value * pow(scene->unit.scale_length, 3);
	}
	else if(subtype == PROP_UNIT_TIME) { /* WARNING - using evil_C :| */
		return FRA2TIME(value);
	}
	else {
		return value;
	}
}

static void ui_get_but_string_unit(uiBut *but, char *str, int len_max, double value, int pad)
{
	Scene *scene= CTX_data_scene((bContext *)but->block->evil_C);
	int do_split= scene->unit.flag & USER_UNIT_OPT_SPLIT;
	int unit_type=  RNA_SUBTYPE_UNIT_VALUE(RNA_property_subtype(but->rnaprop));
	int precision= but->a2;

	if(scene->unit.scale_length<0.0001) scene->unit.scale_length= 1.0; // XXX do_versions

	/* Sanity checks */
	if(precision>4)		precision= 4;
	else if(precision==0)	precision= 2;

	bUnit_AsString(str, len_max, ui_get_but_scale_unit(but, value), precision, scene->unit.system, unit_type, do_split, pad);
}

static float ui_get_but_step_unit(uiBut *but, double value, float step_default)
{
	Scene *scene= CTX_data_scene((bContext *)but->block->evil_C);
	int unit_type=  RNA_SUBTYPE_UNIT_VALUE(RNA_property_subtype(but->rnaprop));
	float step;

	step = bUnit_ClosestScalar(ui_get_but_scale_unit(but, value), scene->unit.system, unit_type);

	if(step > 0.0) { /* -1 is an error value */
		return (step/ui_get_but_scale_unit(but, 1.0))*100;
	}
	else {
		return step_default;
	}
}


void ui_get_but_string(uiBut *but, char *str, int maxlen)
{
	if(but->rnaprop && ELEM3(but->type, TEX, IDPOIN, SEARCH_MENU)) {
		PropertyType type;
		char *buf= NULL;

		type= RNA_property_type(but->rnaprop);

		if(type == PROP_STRING) {
			/* RNA string */
			buf= RNA_property_string_get_alloc(&but->rnapoin, but->rnaprop, str, maxlen);
		}
		else if(type == PROP_POINTER) {
			/* RNA pointer */
			PointerRNA ptr= RNA_property_pointer_get(&but->rnapoin, but->rnaprop);
			buf= RNA_struct_name_get_alloc(&ptr, str, maxlen);
		}

		if(!buf) {
			str[0] = '\0';
		}
		else if(buf && buf != str) {
			/* string was too long, we have to truncate */
			BLI_strncpy(str, buf, maxlen);
			MEM_freeN(buf);
		}
	}
	else if(but->type == IDPOIN) {
		/* ID pointer */
		if(but->idpoin_idpp) { /* Can be NULL for ID properties by python */
			ID *id= *(but->idpoin_idpp);
			if(id) {
				BLI_strncpy(str, id->name+2, maxlen);
				return;
			}
		}
		str[0] = '\0';
		return;
	}
	else if(but->type == TEX) {
		/* string */
		BLI_strncpy(str, but->poin, maxlen);
		return;
	}
	else if(but->type == SEARCH_MENU) {
		/* string */
		BLI_strncpy(str, but->poin, maxlen);
		return;
	}
	else if(ui_but_anim_expression_get(but, str, maxlen))
		; /* driver expression */
	else {
		/* number editing */
		double value;

		value= ui_get_but_val(but);

		if(ui_is_but_float(but)) {
			if(ui_is_but_unit(but)) {
				ui_get_but_string_unit(but, str, maxlen, value, 0);
			}
			else if(but->a2) { /* amount of digits defined */
				if(but->a2==1) BLI_snprintf(str, maxlen, "%.1f", value);
				else if(but->a2==2) BLI_snprintf(str, maxlen, "%.2f", value);
				else if(but->a2==3) BLI_snprintf(str, maxlen, "%.3f", value);
				else BLI_snprintf(str, maxlen, "%.4f", value);
			}
			else
				BLI_snprintf(str, maxlen, "%.3f", value);
		}
		else
			BLI_snprintf(str, maxlen, "%d", (int)value);
	}
}

int ui_set_but_string(bContext *C, uiBut *but, const char *str)
{
	if(but->rnaprop && ELEM3(but->type, TEX, IDPOIN, SEARCH_MENU)) {
		if(RNA_property_editable(&but->rnapoin, but->rnaprop)) {
			PropertyType type;

			type= RNA_property_type(but->rnaprop);

			if(type == PROP_STRING) {
				/* RNA string */
				RNA_property_string_set(&but->rnapoin, but->rnaprop, str);
				return 1;
			}
			else if(type == PROP_POINTER) {
				/* RNA pointer */
				PointerRNA ptr, rptr;
				PropertyRNA *prop;

				if(str == NULL || str[0] == '\0') {
					RNA_property_pointer_set(&but->rnapoin, but->rnaprop, PointerRNA_NULL);
					return 1;
				}
				else {
					ptr= but->rnasearchpoin;
					prop= but->rnasearchprop;
					
					if(prop && RNA_property_collection_lookup_string(&ptr, prop, str, &rptr))
						RNA_property_pointer_set(&but->rnapoin, but->rnaprop, rptr);

					return 1;
				}

				return 0;
			}
		}
	}
	else if(but->type == IDPOIN) {
		/* ID pointer */
    	but->idpoin_func(C, (char*)str, but->idpoin_idpp);
		return 1;
	}
	else if(but->type == TEX) {
		/* string */
		BLI_strncpy(but->poin, str, but->hardmax);
		return 1;
	}
	else if(but->type == SEARCH_MENU) {
		/* string */
		BLI_strncpy(but->poin, str, but->hardmax);
		return 1;
	}
	else if(ui_but_anim_expression_set(but, str)) {
		/* driver expression */
		return 1;
	}
	else {
		/* number editing */
		double value;

#ifndef DISABLE_PYTHON
		{
			char str_unit_convert[256];
			int unit_type;
			Scene *scene= CTX_data_scene((bContext *)but->block->evil_C);

			if(but->rnaprop)
				unit_type= RNA_SUBTYPE_UNIT_VALUE(RNA_property_subtype(but->rnaprop));
			else
				unit_type= 0;

			BLI_strncpy(str_unit_convert, str, sizeof(str_unit_convert));

			if(ui_is_but_unit(but)) {
				/* ugly, use the draw string to get the value, this could cause problems if it includes some text which resolves to a unit */
				bUnit_ReplaceString(str_unit_convert, sizeof(str_unit_convert), but->drawstr, ui_get_but_scale_unit(but, 1.0), scene->unit.system, unit_type);
			}

			if(BPY_button_eval(C, str_unit_convert, &value)) {
				value = ui_get_but_val(but); /* use its original value */

				if(str[0])
					return 0;
			}
		}
#else
		value= atof(str);
#endif

		if(!ui_is_but_float(but)) value= (int)value;
		if(but->type==NUMABS) value= fabs(value);

		/* not that we use hard limits here */
		if(value<but->hardmin) value= but->hardmin;
		if(value>but->hardmax) value= but->hardmax;

		ui_set_but_val(but, value);
		return 1;
	}

	return 0;
}

void ui_set_but_default(bContext *C, uiBut *but)
{
	/* if there is a valid property that is editable... */
	if (but->rnapoin.data && but->rnaprop && RNA_property_editable(&but->rnapoin, but->rnaprop)) {
		if(RNA_property_reset(&but->rnapoin, but->rnaprop, -1)) {
			/* perform updates required for this property */
			RNA_property_update(C, &but->rnapoin, but->rnaprop);
		}
	}
}

static double soft_range_round_up(double value, double max)
{
	/* round up to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, .. */
	double newmax= pow(10.0, ceil(log(value)/log(10.0)));

	if(newmax*0.2 >= max && newmax*0.2 >= value)
		return newmax*0.2;
	else if(newmax*0.5 >= max && newmax*0.5 >= value)
		return newmax*0.5;
	else
		return newmax;
}

static double soft_range_round_down(double value, double max)
{
	/* round down to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, .. */
	double newmax= pow(10.0, floor(log(value)/log(10.0)));

	if(newmax*5.0 <= max && newmax*5.0 <= value)
		return newmax*5.0;
	else if(newmax*2.0 <= max && newmax*2.0 <= value)
		return newmax*2.0;
	else
		return newmax;
}

void ui_set_but_soft_range(uiBut *but, double value)
{
	PropertyType type;
	double softmin, softmax, step, precision;
	
	if(but->rnaprop) {
		type= RNA_property_type(but->rnaprop);

		/* clamp button range to something reasonable in case
		 * we get -inf/inf from RNA properties */
		if(type == PROP_INT) {
			int imin, imax, istep;

			RNA_property_int_ui_range(&but->rnapoin, but->rnaprop, &imin, &imax, &istep);
			softmin= (imin == INT_MIN)? -1e4: imin;
			softmax= (imin == INT_MAX)? 1e4: imax;
			step= istep;
			precision= 1;
		}
		else if(type == PROP_FLOAT) {
			float fmin, fmax, fstep, fprecision;

			RNA_property_float_ui_range(&but->rnapoin, but->rnaprop, &fmin, &fmax, &fstep, &fprecision);
			softmin= (fmin == -FLT_MAX)? -1e4: fmin;
			softmax= (fmax == FLT_MAX)? 1e4: fmax;
			step= fstep;
			precision= fprecision;
		}
		else
			return;

		/* if the value goes out of the soft/max range, adapt the range */
		if(value+1e-10 < softmin) {
			if(value < 0.0)
				softmin= -soft_range_round_up(-value, -softmin);
			else
				softmin= soft_range_round_down(value, softmin);

			if(softmin < but->hardmin)
				softmin= but->hardmin;
		}
		else if(value-1e-10 > softmax) {
			if(value < 0.0)
				softmax= -soft_range_round_down(-value, -softmax);
			else
				softmax= soft_range_round_up(value, softmax);

			if(softmax > but->hardmax)
				softmax= but->hardmax;
		}

		but->softmin= softmin;
		but->softmax= softmax;
	}
}

/* ******************* Free ********************/

static void ui_free_link(uiLink *link)
{
	if(link) {	
		BLI_freelistN(&link->lines);
		MEM_freeN(link);
	}
}

/* can be called with C==NULL */
static void ui_free_but(const bContext *C, uiBut *but)
{
	if(but->opptr) {
		WM_operator_properties_free(but->opptr);
		MEM_freeN(but->opptr);
	}
	if(but->func_argN) MEM_freeN(but->func_argN);
	if(but->active) {
		/* XXX solve later, buttons should be free-able without context? */
		if(C) 
			ui_button_active_cancel(C, but);
		else
			if(but->active) 
				MEM_freeN(but->active);
	}
	if(but->str && but->str != but->strdata) MEM_freeN(but->str);
	ui_free_link(but->link);

	MEM_freeN(but);
}

/* can be called with C==NULL */
void uiFreeBlock(const bContext *C, uiBlock *block)
{
	uiBut *but;

	while( (but= block->buttons.first) ) {
		BLI_remlink(&block->buttons, but);	
		ui_free_but(C, but);
	}

	if(block->func_argN)
		MEM_freeN(block->func_argN);

	CTX_store_free_list(&block->contexts);

	BLI_freelistN(&block->saferct);
	
	MEM_freeN(block);
}

/* can be called with C==NULL */
void uiFreeBlocks(const bContext *C, ListBase *lb)
{
	uiBlock *block;
	
	while( (block= lb->first) ) {
		BLI_remlink(lb, block);
		uiFreeBlock(C, block);
	}
}

void uiFreeInactiveBlocks(const bContext *C, ListBase *lb)
{
	uiBlock *block, *nextblock;

	for(block=lb->first; block; block=nextblock) {
		nextblock= block->next;
	
		if(!block->handle) {
			if(!block->active) {
				BLI_remlink(lb, block);
				uiFreeBlock(C, block);
			}
			else
				block->active= 0;
		}
	}
}

void uiBlockSetRegion(uiBlock *block, ARegion *region)
{
	ListBase *lb;
	uiBlock *oldblock= NULL;

	lb= &region->uiblocks;
	
	/* each listbase only has one block with this name, free block
	 * if is already there so it can be rebuilt from scratch */
	if(lb) {
		for (oldblock= lb->first; oldblock; oldblock= oldblock->next)
			if (BLI_streq(oldblock->name, block->name))
				break;

		if (oldblock) {
			oldblock->active= 0;
			oldblock->panel= NULL;
		}
	}

	block->oldblock= oldblock;

	/* at the beginning of the list! for dynamical menus/blocks */
	if(lb)
		BLI_addhead(lb, block);
}

uiBlock *uiBeginBlock(const bContext *C, ARegion *region, const char *name, short dt)
{
	uiBlock *block;
	wmWindow *window;
	Scene *scn;
	int getsizex, getsizey;

	window= CTX_wm_window(C);
	scn = CTX_data_scene(C);

	block= MEM_callocN(sizeof(uiBlock), "uiBlock");
	block->active= 1;
	block->dt= dt;
	block->evil_C= (void*)C; // XXX
	if (scn) block->color_profile= (scn->r.color_mgt_flag & R_COLOR_MANAGEMENT);
	BLI_strncpy(block->name, name, sizeof(block->name));

	if(region)
		uiBlockSetRegion(block, region);

	/* window matrix and aspect */
	if(region && region->swinid) {
		wm_subwindow_getmatrix(window, region->swinid, block->winmat);
		wm_subwindow_getsize(window, region->swinid, &getsizex, &getsizey);

		/* TODO - investigate why block->winmat[0][0] is negative
		 * in the image view when viewRedrawForce is called */
		block->aspect= 2.0/fabs( (getsizex)*block->winmat[0][0]);
	}
	else {
		/* no subwindow created yet, for menus for example, so we
		 * use the main window instead, since buttons are created
		 * there anyway */
		wm_subwindow_getmatrix(window, window->screen->mainwin, block->winmat);
		wm_subwindow_getsize(window, window->screen->mainwin, &getsizex, &getsizey);

		block->aspect= 2.0/fabs(getsizex*block->winmat[0][0]);
		block->auto_open= 2;
		block->flag |= UI_BLOCK_LOOP; /* tag as menu */
	}

	return block;
}

uiBlock *uiGetBlock(char *name, ARegion *ar)
{
	uiBlock *block= ar->uiblocks.first;
	
	while(block) {
		if( strcmp(name, block->name)==0 ) return block;
		block= block->next;
	}
	
	return NULL;
}

void uiBlockSetEmboss(uiBlock *block, short dt)
{
	block->dt= dt;
}

void ui_check_but(uiBut *but)
{
	/* if something changed in the button */
	double value;
	float okwidth;
//	int transopts= ui_translate_buttons();
	
	ui_is_but_sel(but);
	
//	if(but->type==TEX || but->type==IDPOIN) transopts= 0;

	/* only update soft range while not editing */
	if(but->rnaprop && !(but->editval || but->editstr || but->editvec))
		ui_set_but_soft_range(but, ui_get_but_val(but));

	/* test for min and max, icon sliders, etc */
	switch( but->type ) {
		case NUM:
		case SLI:
		case SCROLL:
		case NUMSLI:
		case HSVSLI:
			value= ui_get_but_val(but);
			if(value < but->hardmin) ui_set_but_val(but, but->hardmin);
			else if(value > but->hardmax) ui_set_but_val(but, but->hardmax);
			break;
			
		case NUMABS:
			value= fabs( ui_get_but_val(but) );
			if(value < but->hardmin) ui_set_but_val(but, but->hardmin);
			else if(value > but->hardmax) ui_set_but_val(but, but->hardmax);
			break;
			
		case ICONTOG: 
		case ICONTOGN:
			if(!but->rnaprop || (RNA_property_flag(but->rnaprop) & PROP_ICONS_CONSECUTIVE)) {
				if(but->flag & UI_SELECT) but->iconadd= 1;
				else but->iconadd= 0;
			}
			break;
			
		case ICONROW:
			if(!but->rnaprop || (RNA_property_flag(but->rnaprop) & PROP_ICONS_CONSECUTIVE)) {
				value= ui_get_but_val(but);
				but->iconadd= (int)value- (int)(but->hardmin);
			}
			break;
			
		case ICONTEXTROW:
			if(!but->rnaprop || (RNA_property_flag(but->rnaprop) & PROP_ICONS_CONSECUTIVE)) {
				value= ui_get_but_val(but);
				but->iconadd= (int)value- (int)(but->hardmin);
			}
			break;
	}
	
	
	/* safety is 4 to enable small number buttons (like 'users') */
	okwidth= -4 + (but->x2 - but->x1); 
	
	/* name: */
	switch( but->type ) {
	
	case MENU:
	case ICONTEXTROW:
		
		if(but->x2 - but->x1 > 24) {
			value= ui_get_but_val(but);
			ui_set_name_menu(but, (int)value);
		}
		break;
	
	case NUM:
	case NUMSLI:
	case HSVSLI:
	case NUMABS:

		value= ui_get_but_val(but);

		if(ui_is_but_float(but)) {
			if(value == FLT_MAX) sprintf(but->drawstr, "%sinf", but->str);
			else if(value == -FLT_MAX) sprintf(but->drawstr, "%s-inf", but->str);
			/* support length type buttons */
			else if(ui_is_but_unit(but)) {
				char new_str[sizeof(but->drawstr)];
				ui_get_but_string_unit(but, new_str, sizeof(new_str), value, TRUE);
				BLI_snprintf(but->drawstr, sizeof(but->drawstr), "%s%s", but->str, new_str);
			}
			else if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(but->drawstr, "%s%.1f", but->str, value);
				else if(but->a2==2) sprintf(but->drawstr, "%s%.2f", but->str, value);
				else if(but->a2==3) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.4f", but->str, value);
			}
			else {
				if(but->hardmax<10.001) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.2f", but->str, value);
			}
		}
		else {
			sprintf(but->drawstr, "%s%d", but->str, (int)value);
		}
			
		if(but->rnaprop) {
			PropertySubType pstype = RNA_property_subtype(but->rnaprop);
			
			if (pstype == PROP_PERCENTAGE)
				strcat(but->drawstr, "%");
		}
		break;

	case LABEL:
		if(ui_is_but_float(but)) {
			value= ui_get_but_val(but);
			if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(but->drawstr, "%s%.1f", but->str, value);
				else if(but->a2==2) sprintf(but->drawstr, "%s%.2f", but->str, value);
				else if(but->a2==3) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.4f", but->str, value);
			}
			else {
				sprintf(but->drawstr, "%s%.2f", but->str, value);
			}
		}
		else strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);
		
		break;

	case IDPOIN:
	case TEX:
	case SEARCH_MENU:
		if(!but->editstr) {
			char str[UI_MAX_DRAW_STR];

			ui_get_but_string(but, str, UI_MAX_DRAW_STR-strlen(but->str));

			strcpy(but->drawstr, but->str);
			strcat(but->drawstr, str);
		}
		break;
	
	case KEYEVT:
		strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);
		if (but->flag & UI_SELECT) {
			strcat(but->drawstr, "Press a key");
		} else {
			strcat(but->drawstr, WM_key_event_string((short) ui_get_but_val(but)));
		}
		break;
		
	case HOTKEYEVT:
		if (but->flag & UI_SELECT) {
			but->drawstr[0]= '\0';
			
			if(but->modifier_key) {
				char *str= but->drawstr;
				
				if(but->modifier_key & KM_SHIFT)
					str= strcat(str, "Shift ");
				if(but->modifier_key & KM_CTRL)
					str= strcat(str, "Ctrl ");
				if(but->modifier_key & KM_ALT)
					str= strcat(str, "Alt ");
				if(but->modifier_key & KM_OSKEY)
					str= strcat(str, "Cmd ");
			}
			else
				strcat(but->drawstr, "Press a key  ");
		}
		else
			strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);

		break;
		
	case BUT_TOGDUAL:
		/* trying to get the dual-icon to left of text... not very nice */
		if(but->str[0]) {
			strncpy(but->drawstr, "  ", UI_MAX_DRAW_STR);
			strncpy(but->drawstr+2, but->str, UI_MAX_DRAW_STR-2);
		}
		break;

	case HSVCUBE:
	case HSVCIRCLE:
		{
			float rgb[3];
			ui_get_but_vectorf(but, rgb);
			rgb_to_hsv(rgb[0], rgb[1], rgb[2], but->hsv, but->hsv+1, but->hsv+2);
		}
		break;
	default:
		strncpy(but->drawstr, but->str, UI_MAX_DRAW_STR);
		
	}

	/* if we are doing text editing, this will override the drawstr */
	if(but->editstr)
		strncpy(but->drawstr, but->editstr, UI_MAX_DRAW_STR);
	
	/* text clipping moved to widget drawing code itself */
}


void uiBlockBeginAlign(uiBlock *block)
{
	/* if other align was active, end it */
	if(block->flag & UI_BUT_ALIGN) uiBlockEndAlign(block);

	block->flag |= UI_BUT_ALIGN_DOWN;	
	block->alignnr++;

	/* buttons declared after this call will get this align nr */ // XXX flag?
}

static int buts_are_horiz(uiBut *but1, uiBut *but2)
{
	float dx, dy;
	
	dx= fabs( but1->x2 - but2->x1);
	dy= fabs( but1->y1 - but2->y2);
	
	if(dx > dy) return 0;
	return 1;
}

void uiBlockEndAlign(uiBlock *block)
{
	block->flag &= ~UI_BUT_ALIGN;	// all 4 flags
}

int ui_but_can_align(uiBut *but)
{
	return !ELEM3(but->type, LABEL, OPTION, OPTIONN);
}

static void ui_block_do_align_but(uiBlock *block, uiBut *first, int nr)
{
	uiBut *prev, *but=NULL, *next;
	int flag= 0, cols=0, rows=0;
	
	/* auto align */

	for(but=first; but && but->alignnr == nr; but=but->next) {
		if(but->next && but->next->alignnr == nr) {
			if(buts_are_horiz(but, but->next)) cols++;
			else rows++;
		}
	}

	/* rows==0: 1 row, cols==0: 1 collumn */
	
	/* note;  how it uses 'flag' in loop below (either set it, or OR it) is confusing */
	for(but=first, prev=NULL; but && but->alignnr == nr; prev=but, but=but->next) {
		next= but->next;
		if(next && next->alignnr != nr)
			next= NULL;

		/* clear old flag */
		but->flag &= ~UI_BUT_ALIGN;
		
		if(flag==0) {	/* first case */
			if(next) {
				if(buts_are_horiz(but, next)) {
					if(rows==0)
						flag= UI_BUT_ALIGN_RIGHT;
					else 
						flag= UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT;
				}
				else {
					flag= UI_BUT_ALIGN_DOWN;
				}
			}
		}
		else if(next==NULL) {	/* last case */
			if(prev) {
				if(buts_are_horiz(prev, but)) {
					if(rows==0) 
						flag= UI_BUT_ALIGN_LEFT;
					else
						flag= UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT;
				}
				else flag= UI_BUT_ALIGN_TOP;
			}
		}
		else if(buts_are_horiz(but, next)) {
			/* check if this is already second row */
			if( prev && buts_are_horiz(prev, but)==0) {
				flag &= ~UI_BUT_ALIGN_LEFT;
				flag |= UI_BUT_ALIGN_TOP;
				/* exception case: bottom row */
				if(rows>0) {
					uiBut *bt= but;
					while(bt && bt->alignnr == nr) {
						if(bt->next && bt->next->alignnr == nr && buts_are_horiz(bt, bt->next)==0 ) break; 
						bt= bt->next;
					}
					if(bt==0 || bt->alignnr != nr) flag= UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT;
				}
			}
			else flag |= UI_BUT_ALIGN_LEFT;
		}
		else {
			if(cols==0) {
				flag |= UI_BUT_ALIGN_TOP;
			}
			else {	/* next button switches to new row */
				
				if(prev && buts_are_horiz(prev, but))
				   flag |= UI_BUT_ALIGN_LEFT;
				
				if( (flag & UI_BUT_ALIGN_TOP)==0) {	/* stil top row */
					if(prev)
						flag= UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT;
					else 
						flag |= UI_BUT_ALIGN_DOWN;
				}
				else 
					flag |= UI_BUT_ALIGN_TOP;
			}
		}
		
		but->flag |= flag;
		
		/* merge coordinates */
		if(prev) {
			// simple cases 
			if(rows==0) {
				but->x1= (prev->x2+but->x1)/2.0;
				prev->x2= but->x1;
			}
			else if(cols==0) {
				but->y2= (prev->y1+but->y2)/2.0;
				prev->y1= but->y2;
			}
			else {
				if(buts_are_horiz(prev, but)) {
					but->x1= (prev->x2+but->x1)/2.0;
					prev->x2= but->x1;
					/* copy height too */
					but->y2= prev->y2;
				}
				else if(prev->prev && buts_are_horiz(prev->prev, prev)==0) {
					/* the previous button is a single one in its row */
					but->y2= (prev->y1+but->y2)/2.0;
					prev->y1= but->y2;
				}
				else {
					/* the previous button is not a single one in its row */
					but->y2= prev->y1;
				}
			}
		}
	}
}

void ui_block_do_align(uiBlock *block)
{
	uiBut *but;
	int nr;

	/* align buttons with same align nr */
	for(but=block->buttons.first; but;) {
		if(but->alignnr) {
			nr= but->alignnr;
			ui_block_do_align_but(block, but, nr);

			/* skip with same number */
			for(; but && but->alignnr == nr; but=but->next);

			if(!but)
				break;
		}
		else
			but= but->next;
	}
}

/*
ui_def_but is the function that draws many button types

for float buttons:
	"a1" Click Step (how much to change the value each click)
	"a2" Number of decimal point values to display. 0 defaults to 3 (0.000) 1,2,3, and a maximum of 4,
       all greater values will be clamped to 4.

*/
static uiBut *ui_def_but(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;
	short slen;
	
	if(type & BUTPOIN) {		/* a pointer is required */
		if(poin==NULL)
			return NULL;
	}

	but= MEM_callocN(sizeof(uiBut), "uiBut");

	but->type= type & BUTTYPE;
	but->pointype= type & BUTPOIN;
	but->bit= type & BIT;
	but->bitnr= type & 31;
	but->icon = 0;
	but->iconadd=0;

	but->retval= retval;
	if( strlen(str)>=UI_MAX_NAME_STR-1 ) {
		but->str= MEM_callocN( strlen(str)+2, "uiDefBut");
		strcpy(but->str, str);
	}
	else {
		but->str= but->strdata;
		strcpy(but->str, str);
	}
	but->x1= x1; 
	but->y1= y1;
	but->x2= (x1+x2); 
	but->y2= (y1+y2);
	
	but->poin= poin;
	but->hardmin= but->softmin= min; 
	but->hardmax= but->softmax= max;
	but->a1= a1; 
	but->a2= a2;
	but->tip= tip;
	
	but->lock= block->lock;
	but->lockstr= block->lockstr;
	but->dt= block->dt;

	but->aspect= 1.0f; //XXX block->aspect;
	but->block= block;		// pointer back, used for frontbuffer status, and picker

	if((block->flag & UI_BUT_ALIGN) && ui_but_can_align(but))
		but->alignnr= block->alignnr;

	but->func= block->func;
	but->func_arg1= block->func_arg1;
	but->func_arg2= block->func_arg2;

	but->funcN= block->funcN;
	if(block->func_argN)
		but->func_argN= MEM_dupallocN(block->func_argN);
	
	but->pos= -1;	/* cursor invisible */

	if(ELEM4(but->type, NUM, NUMABS, NUMSLI, HSVSLI)) {	/* add a space to name */
		slen= strlen(but->str);
		if(slen>0 && slen<UI_MAX_NAME_STR-2) {
			if(but->str[slen-1]!=' ') {
				but->str[slen]= ' ';
				but->str[slen+1]= 0;
			}
		}
	}

	if((block->flag & UI_BLOCK_LOOP) || ELEM7(but->type, MENU, TEX, LABEL, IDPOIN, BLOCK, BUTM, SEARCH_MENU))
		but->flag |= (UI_TEXT_LEFT|UI_ICON_LEFT);
	else if(but->type==BUT_TOGDUAL)
		but->flag |= UI_ICON_LEFT;

	but->flag |= (block->flag & UI_BUT_ALIGN);

	if (but->lock) {
		if (but->lockstr) {
			but->flag |= UI_BUT_DISABLED;
		}
	}

	if(ELEM8(but->type, BLOCK, BUT, LABEL, PULLDOWN, ROUNDBOX, LISTBOX, SEARCH_MENU, BUTM));
	else if(ELEM5(but->type, SCROLL, SEPR, LINK, INLINK, FTPREVIEW));
	else but->flag |= UI_BUT_UNDO;

	BLI_addtail(&block->buttons, but);
	
	if(block->curlayout)
		ui_layout_add_but(block->curlayout, but);

	return but;
}

uiBut *ui_def_but_rna(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;
	PropertyRNA *prop;
	PropertyType proptype;
	int freestr= 0, icon= 0;

	prop= RNA_struct_find_property(ptr, propname);

	if(prop) {
		proptype= RNA_property_type(prop);

		/* use rna values if parameters are not specified */
		if(!str) {
			if(type == MENU && proptype == PROP_ENUM) {
				EnumPropertyItem *item;
				DynStr *dynstr;
				int i, totitem, value, free;

				RNA_property_enum_items(block->evil_C, ptr, prop, &item, &totitem, &free);
				value= RNA_property_enum_get(ptr, prop);

				dynstr= BLI_dynstr_new();
				BLI_dynstr_appendf(dynstr, "%s%%t", RNA_property_ui_name(prop));
				for(i=0; i<totitem; i++) {
					if(!item[i].identifier[0]) {
						if(item[i].name)
							BLI_dynstr_appendf(dynstr, "|%s%%l", item[i].name);
						else
							BLI_dynstr_append(dynstr, "|%l");
					}
					else if(item[i].icon)
						BLI_dynstr_appendf(dynstr, "|%s %%i%d %%x%d", item[i].name, item[i].icon, item[i].value);
					else
						BLI_dynstr_appendf(dynstr, "|%s %%x%d", item[i].name, item[i].value);

					if(value == item[i].value)
						icon= item[i].icon;
				}
				str= BLI_dynstr_get_cstring(dynstr);
				BLI_dynstr_free(dynstr);

				if(free)
					MEM_freeN(item);

				freestr= 1;
			}
			else if(ELEM(type, ROW, LISTROW) && proptype == PROP_ENUM) {
				EnumPropertyItem *item;
				int i, totitem, free;

				RNA_property_enum_items(block->evil_C, ptr, prop, &item, &totitem, &free);
				for(i=0; i<totitem; i++) {
					if(item[i].identifier[0] && item[i].value == (int)max) {
						str= (char*)item[i].name;
						icon= item[i].icon;
					}
				}

				if(!str)
					str= (char*)RNA_property_ui_name(prop);
				if(free)
					MEM_freeN(item);
			}
			else {
				str= (char*)RNA_property_ui_name(prop);
				icon= RNA_property_ui_icon(prop);
			}
		}

		if(!tip) {
			if(type == ROW && proptype == PROP_ENUM) {
				EnumPropertyItem *item;
				int i, totitem, free;

				RNA_property_enum_items(block->evil_C, ptr, prop, &item, &totitem, &free);

				for(i=0; i<totitem; i++) {
					if(item[i].identifier[0] && item[i].value == (int)max) {
						if(item[i].description[0])
							tip= (char*)item[i].description;
						break;
					}
				}

				if(free)
					MEM_freeN(item);
			}
		}
		
		if(!tip)
			tip= (char*)RNA_property_ui_description(prop);

		if(min == max || a1 == -1 || a2 == -1) {
			if(proptype == PROP_INT) {
				int hardmin, hardmax, softmin, softmax, step;

				RNA_property_int_range(ptr, prop, &hardmin, &hardmax);
				RNA_property_int_ui_range(ptr, prop, &softmin, &softmax, &step);

				if(!ELEM(type, ROW, LISTROW) && min == max) {
					min= hardmin;
					max= hardmax;
				}
				if(a1 == -1)
					a1= step;
				if(a2 == -1)
					a2= 0;
			}
			else if(proptype == PROP_FLOAT) {
				float hardmin, hardmax, softmin, softmax, step, precision;

				RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
				RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);

				if(!ELEM(type, ROW, LISTROW) && min == max) {
					min= hardmin;
					max= hardmax;
				}
				if(a1 == -1)
					a1= step;
				if(a2 == -1)
					a2= precision;
			}
			else if(proptype == PROP_STRING) {
				min= 0;
				max= RNA_property_string_maxlength(prop);
				if(max == 0) /* interface code should ideally support unlimited length */
					max= UI_MAX_DRAW_STR; 
			}
		}
	}
	else
		str= (char*)propname;

	/* now create button */
	but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, NULL, min, max, a1, a2, tip);

	if(prop) {
		but->rnapoin= *ptr;
		but->rnaprop= prop;

		if(RNA_property_array_length(&but->rnapoin, but->rnaprop))
			but->rnaindex= index;
		else
			but->rnaindex= 0;
	}

	if(icon) {
		but->icon= (BIFIconID)icon;
		but->flag |= UI_HAS_ICON;
		but->flag|= UI_ICON_LEFT;
	}
	
	if (!prop || !RNA_property_editable(&but->rnapoin, prop)) {
		but->flag |= UI_BUT_DISABLED;
		but->lock = 1;
		but->lockstr = "";
	}

	/* If this button uses units, calculate the step from this */
	if(ui_is_but_unit(but))
		but->a1= ui_get_but_step_unit(but, ui_get_but_val(but), but->a1);

	if(freestr)
		MEM_freeN(str);
	
	return but;
}

uiBut *ui_def_but_operator(uiBlock *block, int type, char *opname, int opcontext, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but;
	wmOperatorType *ot;
	
	ot= WM_operatortype_find(opname, 0);

	if(!str) {
		if(ot) str= ot->name;
		else str= opname;
	}
	
	if ((!tip || tip[0]=='\0') && ot && ot->description) {
		tip= ot->description;
	}

	but= ui_def_but(block, type, -1, str, x1, y1, x2, y2, NULL, 0, 0, 0, 0, tip);
	but->optype= ot;
	but->opcontext= opcontext;

	if(!ot) {
		but->flag |= UI_BUT_DISABLED;
		but->lock = 1;
		but->lockstr = "";
	}

	return but;
}

uiBut *uiDefBut(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	ui_check_but(but);
	
	return but;
}

	/* if _x_ is a power of two (only one bit) return the power,
	 * otherwise return -1. 
	 * (1<<findBitIndex(x))==x for powers of two.
	 */
static int findBitIndex(unsigned int x) {
	if (!x || (x&(x-1))!=0) {	/* x&(x-1) strips lowest bit */
		return -1;
	} else {
		int idx= 0;

		if (x&0xFFFF0000)	idx+=16, x>>=16;
		if (x&0xFF00)		idx+=8, x>>=8;
		if (x&0xF0)			idx+=4, x>>=4;
		if (x&0xC)			idx+=2, x>>=2;
		if (x&0x2)			idx+=1;

		return idx;
	}
}

/* autocomplete helper functions */
struct AutoComplete {
	int maxlen;
	char *truncate;
	char *startname;
};

AutoComplete *autocomplete_begin(char *startname, int maxlen)
{
	AutoComplete *autocpl;
	
	autocpl= MEM_callocN(sizeof(AutoComplete), "AutoComplete");
	autocpl->maxlen= maxlen;
	autocpl->truncate= MEM_callocN(sizeof(char)*maxlen, "AutoCompleteTruncate");
	autocpl->startname= startname;

	return autocpl;
}

void autocomplete_do_name(AutoComplete *autocpl, const char *name)
{
	char *truncate= autocpl->truncate;
	char *startname= autocpl->startname;
	int a;

	for(a=0; a<autocpl->maxlen-1; a++) {
		if(startname[a]==0 || startname[a]!=name[a])
			break;
	}
	/* found a match */
	if(startname[a]==0) {
		/* first match */
		if(truncate[0]==0)
			BLI_strncpy(truncate, name, autocpl->maxlen);
		else {
			/* remove from truncate what is not in bone->name */
			for(a=0; a<autocpl->maxlen-1; a++) {
				if(name[a] == 0) {
					truncate[a]= 0;
					break;
				}
				else if(truncate[a]!=name[a])
					truncate[a]= 0;
			}
		}
	}
}

void autocomplete_end(AutoComplete *autocpl, char *autoname)
{	
	if(autocpl->truncate[0])
		BLI_strncpy(autoname, autocpl->truncate, autocpl->maxlen);
	else {
		if (autoname != autocpl->startname) /* dont copy a string over its self */
			BLI_strncpy(autoname, autocpl->startname, autocpl->maxlen);
	}
	MEM_freeN(autocpl->truncate);
	MEM_freeN(autocpl);
}

/* autocomplete callback for ID buttons */
static void autocomplete_id(bContext *C, char *str, void *arg_v)
{
	int blocktype= (intptr_t)arg_v;
	ListBase *listb= wich_libbase(CTX_data_main(C), blocktype);
	
	if(listb==NULL) return;
	
	/* search if str matches the beginning of an ID struct */
	if(str[0]) {
		AutoComplete *autocpl= autocomplete_begin(str, 22);
		ID *id;
		
		for(id= listb->first; id; id= id->next)
			autocomplete_do_name(autocpl, id->name+2);

		autocomplete_end(autocpl, str);
	}
}

static uiBut *uiDefButBit(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefBut(block, type|BIT|bitIdx, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|FLO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitF(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|FLO, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|INT, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitI(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|INT, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|SHO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitS(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|SHO, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|CHA, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButBitC(uiBlock *block, int type, int bit, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefButBit(block, type|CHA, bit, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButR(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;

	but= ui_def_but_rna(block, type, retval, str, x1, y1, x2, y2, ptr, propname, index, min, max, a1, a2, tip);
	if(but)
		ui_check_but(but);

	return but;
}
uiBut *uiDefButO(uiBlock *block, int type, char *opname, int opcontext, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but;

	but= ui_def_but_operator(block, type, opname, opcontext, str, x1, y1, x2, y2, tip);
	if(but)
		ui_check_but(but);

	return but;
}

/* if a1==1.0 then a2 is an extra icon blending factor (alpha 0.0 - 1.0) */
uiBut *uiDefIconBut(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, "", x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	ui_check_but(but);
	
	return but;
}
static uiBut *uiDefIconButBit(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefIconBut(block, type|BIT|bitIdx, retval, icon, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|FLO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitF(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|FLO, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|INT, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitI(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|INT, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|SHO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitS(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|SHO, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|CHA, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButBitC(uiBlock *block, int type, int bit, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconButBit(block, type|CHA, bit, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButR(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;

	but= ui_def_but_rna(block, type, retval, "", x1, y1, x2, y2, ptr, propname, index, min, max, a1, a2, tip);
	if(but) {
		if(icon) {
			but->icon= (BIFIconID) icon;
			but->flag|= UI_HAS_ICON;
		}
		ui_check_but(but);
	}

	return but;
}
uiBut *uiDefIconButO(uiBlock *block, int type, char *opname, int opcontext, int icon, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but;

	but= ui_def_but_operator(block, type, opname, opcontext, "", x1, y1, x2, y2, tip);
	if(but) {
		but->icon= (BIFIconID) icon;
		but->flag|= UI_HAS_ICON;
		ui_check_but(but);
	}

	return but;
}

/* Button containing both string label and icon */
uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;

	ui_check_but(but);

	return but;
}
static uiBut *uiDefIconTextButBit(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	int bitIdx= findBitIndex(bit);
	if (bitIdx==-1) {
		return NULL;
	} else {
		return uiDefIconTextBut(block, type|BIT|bitIdx, retval, icon, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	}
}

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|FLO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitF(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|FLO, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|INT, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitI(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|INT, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|SHO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitS(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|SHO, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|CHA, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButBitC(uiBlock *block, int type, int bit, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextButBit(block, type|CHA, bit, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButR(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, PointerRNA *ptr, const char *propname, int index, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;

	but= ui_def_but_rna(block, type, retval, str, x1, y1, x2, y2, ptr, propname, index, min, max, a1, a2, tip);
	if(but) {
		if(icon) {
			but->icon= (BIFIconID) icon;
			but->flag|= UI_HAS_ICON;
		}
		but->flag|= UI_ICON_LEFT;
		ui_check_but(but);
	}

	return but;
}
uiBut *uiDefIconTextButO(uiBlock *block, int type, char *opname, int opcontext, int icon, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but;

	but= ui_def_but_operator(block, type, opname, opcontext, str, x1, y1, x2, y2, tip);
	if(but) {
		but->icon= (BIFIconID) icon;
		but->flag|= UI_HAS_ICON;
		but->flag|= UI_ICON_LEFT;
		ui_check_but(but);
	}

	return but;
}

/* END Button containing both string label and icon */

void uiSetButLink(uiBut *but, void **poin, void ***ppoin, short *tot, int from, int to)
{
	uiLink *link;
	
	link= but->link= MEM_callocN(sizeof(uiLink), "new uilink");
	
	link->poin= poin;
	link->ppoin= ppoin;
	link->totlink= tot;
	link->fromcode= from;
	link->tocode= to;
}

/* cruft to make uiBlock and uiBut private */

int uiBlocksGetYMin(ListBase *lb)
{
	uiBlock *block;
	int min= 0;
	
	for (block= lb->first; block; block= block->next)
		if (block==lb->first || block->miny<min)
			min= block->miny;
			
	return min;
}

void uiBlockSetDirection(uiBlock *block, int direction)
{
	block->direction= direction;
}

/* this call escapes if there's alignment flags */
void uiBlockFlipOrder(uiBlock *block)
{
	ListBase lb;
	uiBut *but, *next;
	float centy, miny=10000, maxy= -10000;

	if(U.uiflag & USER_MENUFIXEDORDER)
		return;
	else if(block->flag & UI_BLOCK_NO_FLIP)
		return;
	
	for(but= block->buttons.first; but; but= but->next) {
		if(but->flag & UI_BUT_ALIGN) return;
		if(but->y1 < miny) miny= but->y1;
		if(but->y2 > maxy) maxy= but->y2;
	}
	/* mirror trick */
	centy= (miny+maxy)/2.0;
	for(but= block->buttons.first; but; but= but->next) {
		but->y1 = centy-(but->y1-centy);
		but->y2 = centy-(but->y2-centy);
		SWAP(float, but->y1, but->y2);
	}
	
	/* also flip order in block itself, for example for arrowkey */
	lb.first= lb.last= NULL;
	but= block->buttons.first;
	while(but) {
		next= but->next;
		BLI_remlink(&block->buttons, but);
		BLI_addtail(&lb, but);
		but= next;
	}
	block->buttons= lb;
}


void uiBlockSetFlag(uiBlock *block, int flag)
{
	block->flag|= flag;
}

void uiBlockClearFlag(uiBlock *block, int flag)
{
	block->flag&= ~flag;
}

void uiBlockSetXOfs(uiBlock *block, int xofs)
{
	block->xofs= xofs;
}

void uiButSetFlag(uiBut *but, int flag)
{
	but->flag|= flag;
}

void uiButClearFlag(uiBut *but, int flag)
{
	but->flag&= ~flag;
}

int uiButGetRetVal(uiBut *but)
{
	return but->retval;
}

void uiButSetDragID(uiBut *but, ID *id)
{
	but->dragtype= WM_DRAG_ID;
	but->dragpoin= (void *)id;
}

void uiButSetDragRNA(uiBut *but, PointerRNA *ptr)
{
	but->dragtype= WM_DRAG_RNA;
	but->dragpoin= (void *)ptr;
}

void uiButSetDragPath(uiBut *but, const char *path)
{
	but->dragtype= WM_DRAG_PATH;
	but->dragpoin= (void *)path;
}

void uiButSetDragName(uiBut *but, const char *name)
{
	but->dragtype= WM_DRAG_NAME;
	but->dragpoin= (void *)name;
}

/* value from button itself */
void uiButSetDragValue(uiBut *but)
{
	but->dragtype= WM_DRAG_VALUE;
}

void uiButSetDragImage(uiBut *but, const char *path, int icon, struct ImBuf *imb, float scale)
{
	but->dragtype= WM_DRAG_PATH;
	but->icon= icon; /* no flag UI_HAS_ICON, so icon doesnt draw in button */
	but->dragpoin= (void *)path;
	but->imb= imb;
	but->imb_scale= scale;
}

PointerRNA *uiButGetOperatorPtrRNA(uiBut *but)
{
	if(but->optype && !but->opptr) {
		but->opptr= MEM_callocN(sizeof(PointerRNA), "uiButOpPtr");
		WM_operator_properties_create_ptr(but->opptr, but->optype);
	}

	return but->opptr;
}

void uiBlockSetHandleFunc(uiBlock *block, uiBlockHandleFunc func, void *arg)
{
	block->handle_func= func;
	block->handle_func_arg= arg;
}

void uiBlockSetButmFunc(uiBlock *block, uiMenuHandleFunc func, void *arg)
{
	block->butm_func= func;
	block->butm_func_arg= arg;
}

void uiBlockSetFunc(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2)
{
	block->func= func;
	block->func_arg1= arg1;
	block->func_arg2= arg2;
}

void uiBlockSetNFunc(uiBlock *block, uiButHandleFunc func, void *argN, void *arg2)
{
	if(block->func_argN)
		MEM_freeN(block->func_argN);

	block->funcN= func;
	block->func_argN= argN;
	block->func_arg2= arg2;
}

void uiButSetRenameFunc(uiBut *but, uiButHandleRenameFunc func, void *arg1)
{
	but->rename_func= func;
	but->rename_arg1= arg1;
}

void uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)(const bContext *C, void *idv, void *arg1, void *arg2, rcti *rect), void *arg1, void *arg2)
{
	block->drawextra= func;
	block->drawextra_arg1= arg1;
	block->drawextra_arg2= arg2;
}

void uiButSetFunc(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2)
{
	but->func= func;
	but->func_arg1= arg1;
	but->func_arg2= arg2;
}

void uiButSetNFunc(uiBut *but, uiButHandleNFunc funcN, void *argN, void *arg2)
{
	if(but->func_argN)
		MEM_freeN(but->func_argN);

	but->funcN= funcN;
	but->func_argN= argN;
	but->func_arg2= arg2;
}

void uiButSetCompleteFunc(uiBut *but, uiButCompleteFunc func, void *arg)
{
	but->autocomplete_func= func;
	but->autofunc_arg= arg;
}

uiBut *uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, short blocktype, int retval, char *str, short x1, short y1, short x2, short y2, void *idpp, char *tip)
{
	uiBut *but= ui_def_but(block, IDPOIN, retval, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->idpoin_func= func;
	but->idpoin_idpp= (ID**) idpp;
	ui_check_but(but);
	
	if(blocktype)
		uiButSetCompleteFunc(but, autocomplete_id, (void *)(intptr_t)blocktype);

	return but;
}

uiBut *uiDefBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func= func;
	ui_check_but(but);
	return but;
}

uiBut *uiDefBlockButN(uiBlock *block, uiBlockCreateFunc func, void *argN, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func= func;
	if(but->func_argN)
		MEM_freeN(but->func_argN);
	but->func_argN= argN;
	ui_check_but(but);
	return but;
}


uiBut *uiDefPulldownBut(uiBlock *block, uiBlockCreateFunc func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, PULLDOWN, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_create_func= func;
	ui_check_but(but);
	return but;
}

uiBut *uiDefMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, PULLDOWN, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->menu_create_func= func;
	ui_check_but(but);
	return but;
}

uiBut *uiDefIconTextMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, PULLDOWN, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);

	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;
	but->flag|= UI_ICON_SUBMENU;

	but->menu_create_func= func;
	ui_check_but(but);

	return but;
}

uiBut *uiDefIconMenuBut(uiBlock *block, uiMenuCreateFunc func, void *arg, int icon, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, PULLDOWN, 0, "", x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);

	but->icon= (BIFIconID) icon;
	but->flag |= UI_HAS_ICON;
	but->flag &=~ UI_ICON_LEFT;

	but->menu_create_func= func;
	ui_check_but(but);

	return but;
}

/* Block button containing both string label and icon */
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	/* XXX temp, old menu calls pass on icon arrow, which is now UI_ICON_SUBMENU flag */
	if(icon!=ICON_RIGHTARROW_THIN) {
		but->icon= (BIFIconID) icon;
		but->flag|= UI_ICON_LEFT;
	}
	but->flag|= UI_HAS_ICON;
	but->flag|= UI_ICON_SUBMENU;

	but->block_create_func= func;
	ui_check_but(but);
	
	return but;
}

/* Block button containing icon */
uiBut *uiDefIconBlockBut(uiBlock *block, uiBlockCreateFunc func, void *arg, int retval, int icon, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, retval, "", x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;
	
	but->flag|= UI_ICON_LEFT;
	
	but->block_create_func= func;
	ui_check_but(but);
	
	return but;
}

uiBut *uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip)
{
	uiBut *but= ui_def_but(block, KEYEVT|SHO, retval, str, x1, y1, x2, y2, spoin, 0.0, 0.0, 0.0, 0.0, tip);
	ui_check_but(but);
	return but;
}

/* short pointers hardcoded */
/* modkeypoin will be set to KM_SHIFT, KM_ALT, KM_CTRL, KM_OSKEY bits */
uiBut *uiDefHotKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *keypoin, short *modkeypoin, char *tip)
{
	uiBut *but= ui_def_but(block, HOTKEYEVT|SHO, retval, str, x1, y1, x2, y2, keypoin, 0.0, 0.0, 0.0, 0.0, tip);
	but->modifier_key= *modkeypoin;
	ui_check_but(but);
	return but;
}


/* arg is pointer to string/name, use uiButSetSearchFunc() below to make this work */
/* here a1 and a2, if set, control thumbnail preview rows/cols */
uiBut *uiDefSearchBut(uiBlock *block, void *arg, int retval, int icon, int maxlen, short x1, short y1, short x2, short y2, float a1, float a2, char *tip)
{
	uiBut *but= ui_def_but(block, SEARCH_MENU, retval, "", x1, y1, x2, y2, arg, 0.0, maxlen, a1, a2, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;
	
	but->flag|= UI_ICON_LEFT|UI_TEXT_LEFT;
	
	ui_check_but(but);
	
	return but;
}

/* arg is user value, searchfunc and handlefunc both get it as arg */
/* if active set, button opens with this item visible and selected */
void uiButSetSearchFunc(uiBut *but, uiButSearchFunc sfunc, void *arg, uiButHandleFunc bfunc, void *active)
{
	but->search_func= sfunc;
	but->search_arg= arg;
	
	uiButSetFunc(but, bfunc, arg, active);
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
	/* set default colors in default theme */
	ui_theme_init_userdef();
	
	uiStyleInit();
}

void UI_exit(void)
{
	ui_resources_free();
}

