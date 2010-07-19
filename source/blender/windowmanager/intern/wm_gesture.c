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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_screen_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"	/* lasso tessellation */
#include "BLI_math.h"
#include "BLI_scanfill.h"	/* lasso tessellation */

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_event_system.h"
#include "wm_subwindow.h"
#include "wm_draw.h"


#include "BIF_gl.h"
#include "BIF_glutil.h"


/* context checked on having screen, window and area */
wmGesture *WM_gesture_new(bContext *C, wmEvent *event, int type)
{
	wmGesture *gesture= MEM_callocN(sizeof(wmGesture), "new gesture");
	wmWindow *window= CTX_wm_window(C);
	ARegion *ar= CTX_wm_region(C);
	int sx, sy;
	
	BLI_addtail(&window->gesture, gesture);
	
	gesture->type= type;
	gesture->event_type= event->type;
	gesture->swinid= ar->swinid;	/* means only in area-region context! */
	
	wm_subwindow_getorigin(window, gesture->swinid, &sx, &sy);
	
	if( ELEM5(type, WM_GESTURE_RECT, WM_GESTURE_CROSS_RECT, WM_GESTURE_TWEAK, WM_GESTURE_CIRCLE, WM_GESTURE_STRAIGHTLINE)) {
		rcti *rect= MEM_callocN(sizeof(rcti), "gesture rect new");
		
		gesture->customdata= rect;
		rect->xmin= event->x - sx;
		rect->ymin= event->y - sy;
		if(type==WM_GESTURE_CIRCLE) {
#ifdef GESTURE_MEMORY
			rect->xmax= circle_select_size;
#else
			rect->xmax= 25;	// XXX temp
#endif
		} else {
			rect->xmax= event->x - sx;
			rect->ymax= event->y - sy;
		}
	}
	else if (ELEM(type, WM_GESTURE_LINES, WM_GESTURE_LASSO)) {
		short *lasso;
		gesture->customdata= lasso= MEM_callocN(2*sizeof(short)*WM_LASSO_MIN_POINTS, "lasso points");
		lasso[0] = event->x - sx;
		lasso[1] = event->y - sy;
		gesture->points= 1;
		gesture->size = WM_LASSO_MIN_POINTS;
	}
	
	return gesture;
}

void WM_gesture_end(bContext *C, wmGesture *gesture)
{
	wmWindow *win= CTX_wm_window(C);
	
	if(win->tweak==gesture)
		win->tweak= NULL;
	BLI_remlink(&win->gesture, gesture);
	MEM_freeN(gesture->customdata);
	MEM_freeN(gesture);
}

void WM_gestures_remove(bContext *C)
{
	wmWindow *win= CTX_wm_window(C);
	
	while(win->gesture.first)
		WM_gesture_end(C, win->gesture.first);
}


/* tweak and line gestures */
#define TWEAK_THRESHOLD		10
int wm_gesture_evaluate(bContext *C, wmGesture *gesture)
{
	if(gesture->type==WM_GESTURE_TWEAK) {
		rcti *rect= gesture->customdata;
		int dx= rect->xmax - rect->xmin;
		int dy= rect->ymax - rect->ymin;
		if(ABS(dx)+ABS(dy) > TWEAK_THRESHOLD) {
			int theta= (int)floor(4.0f*atan2((float)dy, (float)dx)/M_PI + 0.5);
			int val= EVT_GESTURE_W;
			
			if(theta==0) val= EVT_GESTURE_E;
			else if(theta==1) val= EVT_GESTURE_NE;
			else if(theta==2) val= EVT_GESTURE_N;
			else if(theta==3) val= EVT_GESTURE_NW;
			else if(theta==-1) val= EVT_GESTURE_SE;
			else if(theta==-2) val= EVT_GESTURE_S;
			else if(theta==-3) val= EVT_GESTURE_SW;
			
#if 0
			/* debug */
			if(val==1) printf("tweak north\n");
			if(val==2) printf("tweak north-east\n");
			if(val==3) printf("tweak east\n");
			if(val==4) printf("tweak south-east\n");
			if(val==5) printf("tweak south\n");
			if(val==6) printf("tweak south-west\n");
			if(val==7) printf("tweak west\n");
			if(val==8) printf("tweak north-west\n");
#endif			
			return val;
		}
	}
	return 0;
}


/* ******************* gesture draw ******************* */

static void wm_gesture_draw_rect(wmWindow *win, wmGesture *gt)
{
	rcti *rect= (rcti *)gt->customdata;
	
	glEnable(GL_BLEND);
	glColor4f(1.0, 1.0, 1.0, 0.05);
	glBegin(GL_QUADS);
	glVertex2s(rect->xmax, rect->ymin);
	glVertex2s(rect->xmax, rect->ymax);
	glVertex2s(rect->xmin, rect->ymax);
	glVertex2s(rect->xmin, rect->ymin);
	glEnd();
	glDisable(GL_BLEND);
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xCCCC);
	sdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x3333);
	sdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glDisable(GL_LINE_STIPPLE);
}

static void wm_gesture_draw_line(wmWindow *win, wmGesture *gt)
{
	rcti *rect= (rcti *)gt->customdata;
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	sdrawline(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	sdrawline(rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	glDisable(GL_LINE_STIPPLE);
	
}

static void wm_gesture_draw_circle(wmWindow *win, wmGesture *gt)
{
	rcti *rect= (rcti *)gt->customdata;

	glTranslatef((float)rect->xmin, (float)rect->ymin, 0.0f);

	glEnable(GL_BLEND);
	glColor4f(1.0, 1.0, 1.0, 0.05);
	glutil_draw_filled_arc(0.0, M_PI*2.0, rect->xmax, 40);
	glDisable(GL_BLEND);
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	glutil_draw_lined_arc(0.0, M_PI*2.0, rect->xmax, 40);
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	glutil_draw_lined_arc(0.0, M_PI*2.0, rect->xmax, 40);
	
	glDisable(GL_LINE_STIPPLE);
	glTranslatef((float)-rect->xmin, (float)-rect->ymin, 0.0f);
	
}

static void draw_filled_lasso(wmGesture *gt)
{
	EditVert *v=NULL, *lastv=NULL, *firstv=NULL;
	EditEdge *e;
	EditFace *efa;
	short *lasso= (short *)gt->customdata;
	int i;
	
	for (i=0; i<gt->points; i++, lasso+=2) {
		float co[3] = {(float)lasso[0], (float)lasso[1], 0.f};
		
		v = BLI_addfillvert(co);
		if (lastv)
			e = BLI_addfilledge(lastv, v);
		lastv = v;
		if (firstv==NULL) firstv = v;
	}
	
	/* highly unlikely this will fail, but could crash if (gt->points == 0) */
	if(firstv) {
		BLI_addfilledge(firstv, v);
		BLI_edgefill(0, 0);
	
		glEnable(GL_BLEND);
		glColor4f(1.0, 1.0, 1.0, 0.05);
		glBegin(GL_TRIANGLES);
		for (efa = fillfacebase.first; efa; efa=efa->next) {
			glVertex2f(efa->v1->co[0], efa->v1->co[1]);
			glVertex2f(efa->v2->co[0], efa->v2->co[1]);
			glVertex2f(efa->v3->co[0], efa->v3->co[1]);
		}
		glEnd();
		glDisable(GL_BLEND);
	
		BLI_end_edgefill();
	}
}

static void wm_gesture_draw_lasso(wmWindow *win, wmGesture *gt)
{
	short *lasso= (short *)gt->customdata;
	int i;

	draw_filled_lasso(gt);
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xAAAA);
	glBegin(GL_LINE_STRIP);
	lasso= (short *)gt->customdata;
	for(i=0; i<gt->points; i++, lasso+=2)
		glVertex2sv(lasso);
	if(gt->type==WM_GESTURE_LASSO)
		glVertex2sv((short *)gt->customdata);
	glEnd();
	
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x5555);
	glBegin(GL_LINE_STRIP);
	lasso= (short *)gt->customdata;
	for(i=0; i<gt->points; i++, lasso+=2)
		glVertex2sv(lasso);
	if(gt->type==WM_GESTURE_LASSO)
		glVertex2sv((short *)gt->customdata);
	glEnd();
	
	glDisable(GL_LINE_STIPPLE);
	
}

static void wm_gesture_draw_cross(wmWindow *win, wmGesture *gt)
{
	rcti *rect= (rcti *)gt->customdata;
	
	glEnable(GL_LINE_STIPPLE);
	glColor3ub(96, 96, 96);
	glLineStipple(1, 0xCCCC);
	sdrawline(rect->xmin - win->sizex, rect->ymin, rect->xmin + win->sizex, rect->ymin);
	sdrawline(rect->xmin, rect->ymin - win->sizey, rect->xmin, rect->ymin + win->sizey);
	
	glColor3ub(255, 255, 255);
	glLineStipple(1, 0x3333);
	sdrawline(rect->xmin - win->sizex, rect->ymin, rect->xmin + win->sizex, rect->ymin);
	sdrawline(rect->xmin, rect->ymin - win->sizey, rect->xmin, rect->ymin + win->sizey);
	glDisable(GL_LINE_STIPPLE);
}

/* called in wm_draw.c */
void wm_gesture_draw(wmWindow *win)
{
	wmGesture *gt= (wmGesture *)win->gesture.first;
	
	for(; gt; gt= gt->next) {
		/* all in subwindow space */
		wmSubWindowSet(win, gt->swinid);
		
		if(gt->type==WM_GESTURE_RECT)
			wm_gesture_draw_rect(win, gt);
		else if(gt->type==WM_GESTURE_TWEAK)
			wm_gesture_draw_line(win, gt);
		else if(gt->type==WM_GESTURE_CIRCLE)
			wm_gesture_draw_circle(win, gt);
		else if(gt->type==WM_GESTURE_CROSS_RECT) {
			if(gt->mode==1)
				wm_gesture_draw_rect(win, gt);
			else
				wm_gesture_draw_cross(win, gt);
		}
		else if(gt->type==WM_GESTURE_LINES) 
			wm_gesture_draw_lasso(win, gt);
		else if(gt->type==WM_GESTURE_LASSO) 
			wm_gesture_draw_lasso(win, gt);
		else if(gt->type==WM_GESTURE_STRAIGHTLINE)
			wm_gesture_draw_line(win, gt);
	}
}

void wm_gesture_tag_redraw(bContext *C)
{
	wmWindow *win= CTX_wm_window(C);
	bScreen *screen= CTX_wm_screen(C);
	ARegion *ar= CTX_wm_region(C);
	
	if(screen)
		screen->do_draw_gesture= 1;

	wm_tag_redraw_overlay(win, ar);
}


