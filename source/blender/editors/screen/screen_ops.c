/**
 * $Id:
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_area.h"
#include "ED_screen.h"
#include "ED_screen_types.h"

#include "screen_intern.h"	/* own module include */

/* ************** Exported Poll tests ********************** */

int ED_operator_areaactive(bContext *C)
{
	if(C->window==NULL) return 0;
	if(C->screen==NULL) return 0;
	if(C->area==NULL) return 0;
	return 1;
}

int ED_operator_screenactive(bContext *C)
{
	if(C->window==NULL) return 0;
	if(C->screen==NULL) return 0;
	return 1;
}

/* when mouse is over area-edge */
int ED_operator_screen_mainwinactive(bContext *C)
{
	if(C->window==NULL) return 0;
	if(C->screen==NULL) return 0;
	if (C->screen->subwinactive!=C->screen->mainwin) return 0;
	return 1;
}

/* *************************** action zone operator ************************** */

/* operator state vars used:  
	none

functions:

	apply() set actionzone event

	exit()	free customdata
	
callbacks:

	exec()	never used

	invoke() check if in zone  
		add customdata, put mouseco and area in it
		add modal handler

	modal()	accept modal events while doing it
		call apply() with gesture info, active window, nonactive window
		call exit() and remove handler when LMB confirm

*/

typedef struct sActionzoneData {
	ScrArea *sa1, *sa2;
	AZone *az;
	int x, y, gesture_dir;
} sActionzoneData;

/* used by other operators too */
static ScrArea *screen_areahascursor(bScreen *scr, int x, int y)
{
	ScrArea *sa= NULL;
	sa= scr->areabase.first;
	while(sa) {
		if(BLI_in_rcti(&sa->totrct, x, y)) break;
		sa= sa->next;
	}
	
	return sa;
}


static AZone *is_in_area_actionzone(ScrArea *sa, int x, int y)
{
	AZone *az= NULL;
	int i= 0;
	
	for(az= sa->actionzones.first, i= 0; az; az= az->next, i++) {
		if(az && az->type == AZONE_TRI) {
			if(IsPointInTri2DInts(az->x1, az->y1, az->x2, az->y2, x, y)) 
				break;
		}
		if(az->type == AZONE_QUAD) {
			if(az->x1 < x && x < az->x2 && az->y1 < y && y < az->y2) 
				break;
		}
	}
	
	return az;
}

static int actionzone_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	AZone *az= is_in_area_actionzone(C->area, event->x, event->y);
	sActionzoneData *sad;
	
	/* quick escape */
	if(az==NULL)
		return OPERATOR_PASS_THROUGH;
	
	/* ok we do the actionzone */
	sad= op->customdata= MEM_callocN(sizeof(sActionzoneData), "sActionzoneData");
	sad->sa1= C->area;
	sad->az= az;
	sad->x= event->x; sad->y= event->y;
	
	/* add modal handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static void actionzone_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
}

/* send EVT_ACTIONZONE event */
static void actionzone_apply(bContext *C, wmOperator *op)
{
	wmEvent event;
	
	event= *(C->window->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_ACTIONZONE;
	event.customdata= op->customdata;
	event.customdatafree= TRUE;
	op->customdata= NULL;
	
	wm_event_add(C->window, &event);
}

static int actionzone_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sActionzoneData *sad= op->customdata;
	int deltax, deltay;
	
	switch(event->type) {
		case MOUSEMOVE:
			/* calculate gesture direction */
			deltax= (event->x - sad->x);
			deltay= (event->y - sad->y);
			
			if(deltay > ABS(deltax))
				sad->gesture_dir= AZONE_N;
			else if(deltax > ABS(deltay))
				sad->gesture_dir= AZONE_E;
			else if(deltay < -ABS(deltax))
				sad->gesture_dir= AZONE_S;
			else
				sad->gesture_dir= AZONE_W;
			
			/* gesture is large enough? */
			if(ABS(deltax) > 12 || ABS(deltay) > 12) {
				
				/* second area, for join */
				sad->sa2= screen_areahascursor(C->screen, event->x, event->y);
				/* apply sends event */
				actionzone_apply(C, op);
				actionzone_exit(C, op);
				
				WM_event_remove_modal_handler(&C->window->handlers, op);
				
				return OPERATOR_FINISHED;
			}
				break;
		case ESCKEY:
		case LEFTMOUSE:
			actionzone_exit(C, op);
			WM_event_remove_modal_handler(&C->window->handlers, op);
			return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_actionzone(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Handle area action zones";
	ot->idname= "ED_SCR_OT_actionzone";
	
	ot->invoke= actionzone_invoke;
	ot->modal= actionzone_modal;
	
	ot->poll= ED_operator_areaactive;
}

/* ****************** cursor near edge operator ********************************* */

static int scredge_is_horizontal(ScrEdge *se)
{
	return (se->v1->vec.y == se->v2->vec.y);
}

static ScrEdge *screen_find_active_scredge(bScreen *sc, int mx, int my)
{
	ScrEdge *se;
	
	for (se= sc->edgebase.first; se; se= se->next) {
		if (scredge_is_horizontal(se)) {
			short min, max;
			min= MIN2(se->v1->vec.x, se->v2->vec.x);
			max= MAX2(se->v1->vec.x, se->v2->vec.x);
			
			if (abs(my-se->v1->vec.y)<=2 && mx>=min && mx<=max)
				return se;
		} 
		else {
			short min, max;
			min= MIN2(se->v1->vec.y, se->v2->vec.y);
			max= MAX2(se->v1->vec.y, se->v2->vec.y);
			
			if (abs(mx-se->v1->vec.x)<=2 && my>=min && my<=max)
				return se;
		}
	}
	
	return NULL;
}


/* operator cb */
static int screen_cursor_test(bContext *C, wmOperator *op, wmEvent *event)
{
	if (C->screen->subwinactive==C->screen->mainwin) {
		ScrEdge *actedge= screen_find_active_scredge(C->screen, event->x, event->y);
		
		if (actedge && scredge_is_horizontal(actedge)) {
			WM_set_cursor(C, CURSOR_Y_MOVE);
		} else {
			WM_set_cursor(C, CURSOR_X_MOVE);
		}
	} else {
		ScrArea *sa= NULL;
		AZone *az= NULL;
		for(sa= C->screen->areabase.first; sa; sa= sa->next) {
			az= is_in_area_actionzone(sa, event->x, event->y);
			if(az!=NULL) break;
		}
		if(az!=NULL) WM_set_cursor(C, CURSOR_EDIT);
		else WM_set_cursor(C, CURSOR_STD);
	}
	
	return OPERATOR_PASS_THROUGH;
}

static void ED_SCR_OT_cursor_type(wmOperatorType *ot)
{
    ot->name= "Cursor type";
    ot->idname= "ED_SCR_OT_cursor_type";
	
    ot->invoke= screen_cursor_test;
    ot->poll= ED_operator_screenactive;
}



/* *********** Rip area operator ****************** */


/* operator callback */
/* (ton) removed attempt to merge ripped area with another, don't think this is desired functionality.
conventions: 'atomic' and 'dont think for user' :) */
static int screen_area_rip_op(bContext *C, wmOperator *op)
{
	wmWindow *win;
	bScreen *newsc;
	rcti rect;
	
	/*  poll() checks area context, but we don't accept full-area windows */
	if(C->screen->full != SCREENNORMAL) 
		return OPERATOR_CANCELLED;
	
	/* adds window to WM */
	rect= C->area->totrct;
	BLI_translate_rcti(&rect, C->window->posx, C->window->posy);
	win= WM_window_open(C, &rect);
	
	/* allocs new screen and adds to newly created window, using window size */
	newsc= screen_add(win, C->screen->id.name+2);
	
	/* copy area to new screen */
	area_copy_data((ScrArea *)newsc->areabase.first, C->area, 0);
	
	/* screen, areas init */
	WM_event_add_notifier(C->wm, win, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	
	return OPERATOR_FINISHED;
}

void ED_SCR_OT_area_rip(wmOperatorType *ot)
{
	ot->name= "Rip Area into New Window";
	ot->idname= "ED_SCR_OT_area_rip";
	
	ot->invoke= NULL; //WM_operator_confirm;
	ot->exec= screen_area_rip_op;
	ot->poll= ED_operator_areaactive;
}


/* ************** move area edge operator *********************************** */

/* operator state vars used:  
           x, y   			mouse coord near edge
           delta            movement of edge

	functions:

	init()   set default property values, find edge based on mouse coords, test
            if the edge can be moved, select edges, calculate min and max movement

	apply()	apply delta on selection

	exit()	cleanup, send notifier

	cancel() cancel moving

	callbacks:

	exec()   execute without any user interaction, based on properties
            call init(), apply(), exit()

	invoke() gets called on mouse click near edge
            call init(), add handler

	modal()  accept modal events while doing it
			call apply() with delta motion
            call exit() and remove handler

*/

typedef struct sAreaMoveData {
	int bigger, smaller, origval;
	char dir;
} sAreaMoveData;

/* helper call to move area-edge, sets limits */
static void area_move_set_limits(bScreen *sc, int dir, int *bigger, int *smaller)
{
	ScrArea *sa;
	
	/* we check all areas and test for free space with MINSIZE */
	*bigger= *smaller= 100000;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(dir=='h') {
			int y1= sa->v2->vec.y - sa->v1->vec.y-AREAMINY;
			
			/* if top or down edge selected, test height */
			if(sa->v1->flag && sa->v4->flag)
				*bigger= MIN2(*bigger, y1);
			else if(sa->v2->flag && sa->v3->flag)
				*smaller= MIN2(*smaller, y1);
		}
		else {
			int x1= sa->v4->vec.x - sa->v1->vec.x-AREAMINX;
			
			/* if left or right edge selected, test width */
			if(sa->v1->flag && sa->v2->flag)
				*bigger= MIN2(*bigger, x1);
			else if(sa->v3->flag && sa->v4->flag)
				*smaller= MIN2(*smaller, x1);
		}
	}
}

static void select_connected_scredge(bScreen *sc, ScrEdge *edge)
{
	ScrEdge *se;
	ScrVert *sv;
	int oneselected;
	char dir;
	
	/* select connected, only in the right direction */
	/* 'dir' is the direction of EDGE */
	
	if(edge->v1->vec.x==edge->v2->vec.x) dir= 'v';
	else dir= 'h';
	
	sv= sc->vertbase.first;
	while(sv) {
		sv->flag = 0;
		sv= sv->next;
	}
	
	edge->v1->flag= 1;
	edge->v2->flag= 1;
	
	oneselected= 1;
	while(oneselected) {
		se= sc->edgebase.first;
		oneselected= 0;
		while(se) {
			if(se->v1->flag + se->v2->flag==1) {
				if(dir=='h') if(se->v1->vec.y==se->v2->vec.y) {
					se->v1->flag= se->v2->flag= 1;
					oneselected= 1;
				}
					if(dir=='v') if(se->v1->vec.x==se->v2->vec.x) {
						se->v1->flag= se->v2->flag= 1;
						oneselected= 1;
					}
			}
				se= se->next;
		}
	}
}

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int area_move_init (bContext *C, wmOperator *op)
{
	ScrEdge *actedge;
	sAreaMoveData *md;
	int x, y;

	/* required properties */
	if(!(OP_get_int(op, "x", &x) && OP_get_int(op, "y", &y)))
		return 0;

	/* default properties */
	OP_verify_int(op, "delta", 0, NULL);

	/* setup */
	actedge= screen_find_active_scredge(C->screen, x, y);
	if(actedge==NULL) return 0;

	md= MEM_callocN(sizeof(sAreaMoveData), "sAreaMoveData");
	op->customdata= md;

	md->dir= scredge_is_horizontal(actedge)?'h':'v';
	if(md->dir=='h') md->origval= actedge->v1->vec.y;
	else md->origval= actedge->v1->vec.x;
	
	select_connected_scredge(C->screen, actedge);
	/* now all vertices with 'flag==1' are the ones that can be moved. */

	area_move_set_limits(C->screen, md->dir, &md->bigger, &md->smaller);
	
	return 1;
}

/* moves selected screen edge amount of delta, used by split & move */
static void area_move_apply_do(bContext *C, int origval, int delta, int dir, int bigger, int smaller)
{
	ScrVert *v1;
	
	delta= CLAMPIS(delta, -smaller, bigger);
	
	for (v1= C->screen->vertbase.first; v1; v1= v1->next) {
		if (v1->flag) {
			/* that way a nice AREAGRID  */
			if((dir=='v') && v1->vec.x>0 && v1->vec.x<C->window->sizex-1) {
				v1->vec.x= origval + delta;
				if(delta != bigger && delta != -smaller) v1->vec.x-= (v1->vec.x % AREAGRID);
			}
			if((dir=='h') && v1->vec.y>0 && v1->vec.y<C->window->sizey-1) {
				v1->vec.y= origval + delta;

				v1->vec.y+= AREAGRID-1;
				v1->vec.y-= (v1->vec.y % AREAGRID);
				
				/* prevent too small top header */
				if(v1->vec.y > C->window->sizey-AREAMINY)
					v1->vec.y= C->window->sizey-AREAMINY;
			}
		}
	}

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
}

static void area_move_apply(bContext *C, wmOperator *op)
{
	sAreaMoveData *md= op->customdata;
	int delta;
	
	OP_get_int(op, "delta", &delta);
	area_move_apply_do(C, md->origval, delta, md->dir, md->bigger, md->smaller);
}

static void area_move_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
	
	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);
}

static int area_move_exec(bContext *C, wmOperator *op)
{
	if(!area_move_init(C, op))
		return OPERATOR_CANCELLED;
	
	area_move_apply(C, op);
	area_move_exit(C, op);
	
	return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_move_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	OP_verify_int(op, "x", event->x, NULL);
	OP_verify_int(op, "y", event->y, NULL);

	if(!area_move_init(C, op)) 
		return OPERATOR_PASS_THROUGH;
	
	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static int area_move_cancel(bContext *C, wmOperator *op)
{
	WM_event_remove_modal_handler(&C->window->handlers, op);				

	OP_set_int(op, "delta", 0);
	area_move_apply(C, op);
	area_move_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback for while moving edges */
static int area_move_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaMoveData *md;
	int delta, x, y;

	md= op->customdata;

	OP_get_int(op, "x", &x);
	OP_get_int(op, "y", &y);

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			delta= (md->dir == 'v')? event->x - x: event->y - y;
			OP_set_int(op, "delta", delta);

			area_move_apply(C, op);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				area_move_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);				
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return area_move_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_area_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move area edges";
	ot->idname= "ED_SCR_OT_area_move";

	ot->exec= area_move_exec;
	ot->invoke= area_move_invoke;
	ot->cancel= area_move_cancel;
	ot->modal= area_move_modal;

	ot->poll= ED_operator_screen_mainwinactive; /* when mouse is over area-edge */
}

/* ************** split area operator *********************************** */

/* 
operator state vars:  
	fac              spit point
	dir              direction 'v' or 'h'

operator customdata:
	area   			pointer to (active) area
	x, y			last used mouse pos
	(more, see below)

functions:

	init()   set default property values, find area based on context

	apply()	split area based on state vars

	exit()	cleanup, send notifier

	cancel() remove duplicated area

callbacks:

	exec()   execute without any user interaction, based on state vars
            call init(), apply(), exit()

	invoke() gets called on mouse click in action-widget
            call init(), add modal handler
			call apply() with initial motion

	modal()  accept modal events while doing it
            call move-areas code with delta motion
            call exit() or cancel() and remove handler

*/

#define SPLIT_STARTED	1
#define SPLIT_PROGRESS	2

typedef struct sAreaSplitData
{
	int x, y;	/* last used mouse position */
	
	int origval;			/* for move areas */
	int bigger, smaller;	/* constraints for moving new edge */
	int delta;				/* delta move edge */
	
	ScrEdge *nedge;			/* new edge */
	ScrArea *sarea;			/* start area */
	ScrArea *narea;			/* new area */
} sAreaSplitData;

/* generic init, no UI stuff here */
static int area_split_init(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd;
	int dir;
	
	/* required context */
	if(C->area==NULL) return 0;
	
	/* required properties */
	OP_verify_float(op, "fac", 0.5f, NULL);
	OP_verify_int(op, "dir", 'h', &dir);
	
	/* minimal size */
	if(dir=='v' && C->area->winx < 2*AREAMINX) return 0;
	if(dir=='h' && C->area->winy < 2*AREAMINY) return 0;
	   
	/* custom data */
	sd= (sAreaSplitData*)MEM_callocN(sizeof (sAreaSplitData), "op_area_split");
	op->customdata= sd;
	
	sd->sarea= C->area;
	
	return 1;
}

/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* used with split operator */
static ScrEdge *area_findsharededge(bScreen *screen, ScrArea *sa, ScrArea *sb)
{
	ScrVert *sav1= sa->v1;
	ScrVert *sav2= sa->v2;
	ScrVert *sav3= sa->v3;
	ScrVert *sav4= sa->v4;
	ScrVert *sbv1= sb->v1;
	ScrVert *sbv2= sb->v2;
	ScrVert *sbv3= sb->v3;
	ScrVert *sbv4= sb->v4;
	
	if(sav1==sbv4 && sav2==sbv3) { /* sa to right of sb = W */
		return screen_findedge(screen, sav1, sav2);
	}
	else if(sav2==sbv1 && sav3==sbv4) { /* sa to bottom of sb = N */
		return screen_findedge(screen, sav2, sav3);
	}
	else if(sav3==sbv2 && sav4==sbv1) { /* sa to left of sb = E */
		return screen_findedge(screen, sav3, sav4);
	}
	else if(sav1==sbv2 && sav4==sbv3) { /* sa on top of sb = S*/
		return screen_findedge(screen, sav1, sav4);
	}

	return NULL;
}


/* do the split */
static void area_split_apply(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	float fac;
	int dir;
	
	OP_get_float(op, "fac", &fac);
	OP_get_int(op, "dir", &dir);

	sd->narea= area_split(C->window, C->screen, sd->sarea, dir, fac);
	
	if(sd->narea) {
		ScrVert *sv;
		
		sd->nedge= area_findsharededge(C->screen, sd->sarea, sd->narea);
	
		/* select newly created edge, prepare for moving edge */
		for(sv= C->screen->vertbase.first; sv; sv= sv->next)
			sv->flag = 0;
		
		sd->nedge->v1->flag= 1;
		sd->nedge->v2->flag= 1;

		if(dir=='h') sd->origval= sd->nedge->v1->vec.y;
		else sd->origval= sd->nedge->v1->vec.x;

	}		
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	
}

static void area_split_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);
}


/* UI callback, adds new handler */
static int area_split_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd;
	
	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;
		int dir;
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->az==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas not equal it should be passed on */
		if(C->area!=sad->sa1 || sad->sa1!=sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		if(sad->gesture_dir==AZONE_N || sad->gesture_dir==AZONE_S) {
			dir= 'h';
			OP_set_float(op, "fac", ((float)(event->x - sad->sa1->v1->vec.x)) / (float)sad->sa1->winx);
		}
		else {
			dir= 'v';
			OP_set_float(op, "fac", ((float)(event->y - sad->sa1->v1->vec.y)) / (float)sad->sa1->winy);
		}
		OP_set_int(op, "dir", dir);

		/* general init, also non-UI case, adds customdata, sets area and defaults */
		if(!area_split_init(C, op))
			return OPERATOR_PASS_THROUGH;
		
		sd= (sAreaSplitData *)op->customdata;
		
		sd->x= event->x;
		sd->y= event->y;
		
		/* do the split */
		area_split_apply(C, op);
		area_move_set_limits(C->screen, dir, &sd->bigger, &sd->smaller);
		
		/* add temp handler for edge move or cancel */
		WM_event_add_modal_handler(&C->window->handlers, op);
		
		return OPERATOR_RUNNING_MODAL;
		
	}
	return OPERATOR_PASS_THROUGH;
}

/* function to be called outside UI context, or for redo */
static int area_split_exec(bContext *C, wmOperator *op)
{
	
	if(!area_split_init(C, op))
		return OPERATOR_CANCELLED;
	
	area_split_apply(C, op);
	area_split_exit(C, op);
	
	return OPERATOR_FINISHED;
}


static int area_split_cancel(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;

	WM_event_remove_modal_handler(&C->window->handlers, op);

	if (screen_area_join(C->screen,sd->sarea, sd->narea)) {
		if (C->area == sd->narea) {
			C->area = NULL;
		}
		sd->narea = NULL;
	}
	area_split_exit(C, op);

	return OPERATOR_CANCELLED;
}

static int area_split_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	int dir;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			
			OP_get_int(op, "dir", &dir);
			
			sd->delta= (dir == 'v')? event->x - sd->origval: event->y - sd->origval;
			area_move_apply_do(C, sd->origval, sd->delta, dir, sd->bigger, sd->smaller);
			
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) { /* mouse up */
				area_split_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
				return OPERATOR_FINISHED;
			}
			break;
		case RIGHTMOUSE: /* cancel operation */
		case ESCKEY:
			return area_split_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_area_split(wmOperatorType *ot)
{
	ot->name = "Split area";
	ot->idname = "ED_SCR_OT_area_split";
	
	ot->exec= area_split_exec;
	ot->invoke= area_split_invoke;
	ot->modal= area_split_modal;
	
	ot->poll= ED_operator_screenactive; /* XXX should be area active */
}

/* ************** join area operator ********************************************** */

/* operator state vars used:  
			x1, y1     mouse coord in first area, which will disappear
			x2, y2     mouse coord in 2nd area, which will become joined

functions:

   init()   find edge based on state vars 
			test if the edge divides two areas, 
			store active and nonactive area,
            
   apply()  do the actual join

   exit()	cleanup, send notifier

callbacks:

   exec()	calls init, apply, exit 
   
   invoke() sets mouse coords in x,y
            call init()
            add modal handler

   modal()	accept modal events while doing it
			call apply() with active window and nonactive window
            call exit() and remove handler when LMB confirm

*/

typedef struct sAreaJoinData
{
	ScrArea *sa1;	/* first area to be considered */
	ScrArea *sa2;	/* second area to be considered */
	ScrArea *scr;	/* designed for removal */

} sAreaJoinData;


/* validate selection inside screen, set variables OK */
/* return 0: init failed */
/* XXX todo: find edge based on (x,y) and set other area? */
static int area_join_init(bContext *C, wmOperator *op)
{
	ScrArea *sa1, *sa2;
	sAreaJoinData* jd= NULL;
	int x1, y1;
	int x2, y2;

	/* required properties, make negative to get return 0 if not set by caller */
	OP_verify_int(op, "x1", -100, &x1);
	OP_verify_int(op, "y1", -100, &y1);
	OP_verify_int(op, "x2", -100, &x2);
	OP_verify_int(op, "y2", -100, &y2);
	
	sa1 = screen_areahascursor(C->screen, x1, y1);
	sa2 = screen_areahascursor(C->screen, x2, y2);
	if(sa1==NULL || sa2==NULL || sa1==sa2)
		return 0;

	jd = (sAreaJoinData*)MEM_callocN(sizeof (sAreaJoinData), "op_area_join");
		
	jd->sa1 = sa1;
	jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
	jd->sa2 = sa2;
	jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
	
	op->customdata= jd;
	
	return 1;
}

/* apply the join of the areas (space types) */
static int area_join_apply(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	if (!jd) return 0;

	if(!screen_area_join(C->screen,jd->sa1,jd->sa2)){
		return 0;
	}
	if (C->area == jd->sa2) {
		C->area = NULL;
	}

	return 1;
}

/* finish operation */
static void area_join_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scredges(C->screen);
	removenotused_scredges(C->screen);
	removenotused_scrverts(C->screen);
}

static int area_join_exec(bContext *C, wmOperator *op)
{
	if(!area_join_init(C, op)) 
		return OPERATOR_CANCELLED;
	
	area_join_apply(C, op);
	area_join_exit(C, op);

	return OPERATOR_FINISHED;
}

/* interaction callback */
static int area_join_invoke(bContext *C, wmOperator *op, wmEvent *event)
{

	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->sa2==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas equal it should be passed on */
		if(sad->sa1==sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		OP_set_int(op, "x1", sad->x);
		OP_set_int(op, "y1", sad->y);
		OP_set_int(op, "x2", event->x);
		OP_set_int(op, "y2", event->y);

		if(!area_join_init(C, op)) 
			return OPERATOR_PASS_THROUGH;
	
		/* add temp handler */
		WM_event_add_modal_handler(&C->window->handlers, op);
	
		return OPERATOR_RUNNING_MODAL;
	}
	
	return OPERATOR_PASS_THROUGH;
}

static int area_join_cancel(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

	if (jd->sa1) {
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINTO;
	}
	if (jd->sa2) {
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
	}

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
	WM_event_remove_modal_handler(&C->window->handlers, op);			
	
	area_join_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback while selecting area (space) that will be removed */
static int area_join_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	
	/* execute the events */
	switch(event->type) {
			
		case MOUSEMOVE: 
			{
				ScrArea *sa = screen_areahascursor(C->screen, event->x, event->y);
				int dir;
				
				if (sa) {					
					if (jd->sa1 != sa) {
						dir = area_getorientation(C->screen, jd->sa1, sa);
						if (dir >= 0) {
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa2 = sa;
							jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
						} 
						else {
							/* we are not bordering on the previously selected area 
							   we check if area has common border with the one marked for removal
							   in this case we can swap areas.
							*/
							dir = area_getorientation(C->screen, sa, jd->sa2);
							if (dir >= 0) {
								if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa1 = jd->sa2;
								jd->sa2 = sa;
								if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							} 
							else {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = NULL;
							}
						}
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					} 
					else {
						/* we are back in the area previously selected for keeping 
						 * we swap the areas if possible to allow user to choose */
						if (jd->sa2 != NULL) {
							if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa1 = jd->sa2;
							jd->sa2 = sa;
							if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							dir = area_getorientation(C->screen, jd->sa1, jd->sa2);
							if (dir < 0) {
								printf("oops, didn't expect that!\n");
							}
						} 
						else {
							dir = area_getorientation(C->screen, jd->sa1, sa);
							if (dir >= 0) {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = sa;
								jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							}
						}
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					}
				}
			}
			break;
		case LEFTMOUSE:
			if(event->val==0) {
				area_join_apply(C, op);
				WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
				area_join_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return area_join_cancel(C, op);
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Operator for joining two areas (space types) */
void ED_SCR_OT_area_join(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Join area";
	ot->idname= "ED_SCR_OT_area_join";
	
	/* api callbacks */
	ot->exec= area_join_exec;
	ot->invoke= area_join_invoke;
	ot->modal= area_join_modal;

	ot->poll= ED_operator_screenactive;
}

/* ************** border select operator (test only) ***************************** */

/* operator state vars used: (added by default WM callbacks)   
	xmin, ymin     
	xmax, ymax     

	customdata: the wmGesture pointer

callbacks:

	exec()	has to be filled in by user

	invoke() default WM function
			 adds modal handler

	modal()	default WM function 
			accept modal events while doing it, calls exec(), handles ESC and border drawing
	
	poll()	has to be filled in by user for context
*/

static int border_select_do(bContext *C, wmOperator *op)
{
	printf("border select do\n");
	return 1;
}

void ED_SCR_OT_border_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border select";
	ot->idname= "ED_SCR_OT_border_select";
	
	/* api callbacks */
	ot->exec= border_select_do;
	ot->invoke= WM_border_select_invoke;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
}


/* ****************  Assigning operatortypes to global list, adding handlers **************** */

/* called in spacetypes.c */
void ED_operatortypes_screen(void)
{
	/* generic UI stuff */
	WM_operatortype_append(ED_SCR_OT_cursor_type);
	WM_operatortype_append(ED_SCR_OT_actionzone);
	
	/* tools */
	WM_operatortype_append(ED_SCR_OT_area_move);
	WM_operatortype_append(ED_SCR_OT_area_split);
	WM_operatortype_append(ED_SCR_OT_area_join);
	WM_operatortype_append(ED_SCR_OT_area_rip);
	
	/* for test only */
	WM_operatortype_append(ED_SCR_OT_border_select);
}

/* called in spacetypes.c */
void ED_keymap_screen(wmWindowManager *wm)
{
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_cursor_type", MOUSEMOVE, 0, 0, 0);
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_actionzone", LEFTMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_area_move", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_area_split", EVT_ACTIONZONE, 0, 0, 0);	/* action tria */
	WM_keymap_verify_item(&wm->screenkeymap, "ED_SCR_OT_area_join", EVT_ACTIONZONE, 0, 0, 0);	/* action tria */ 
	WM_keymap_verify_item(&wm->windowkeymap, "ED_SCR_OT_area_rip", RKEY, KM_PRESS, KM_ALT, 0);

	/* for test only */
	WM_keymap_verify_item(&wm->windowkeymap, "ED_SCR_OT_border_select", BKEY, KM_PRESS, 0, 0);

}

