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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"

#include "UI_resources.h"
#include "UI_view2d.h"

/* ********************************************************* */

/* ********************************************************* */
/* View Panning Operator */

/* 
operator state vars:  
	(currently none) // XXX must figure out some vars to expose to user! 

operator customdata:
	area   			pointer to (active) area
	x, y				last used mouse pos
	(more, see below)

functions:

	init()   set default property values, find v2d based on context

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

typedef struct v2dViewPanData {
	ARegion *region;		/* region we're operating in */
	View2D *v2d;			/* view2d we're operating in */
	
	float facx, facy;		/* amount to move view relative to zoom */
	
		/* mouse stuff... */
	int lastx, lasty;		/* previous x/y values of mouse in area */
	int x, y;				/* current x/y values of mosue in area */
} v2dViewPanData;


static int pan_view_init(bContext *C, wmOperator *op)
{
	v2dViewPanData *vpd;
	ARegion *ar;
	View2D *v2d;
	float winx, winy;
	
	/* regions now have v2d-data by default, so check for region */
	if (C->region == NULL)
		return 0;
	
	/* set custom-data for operator */
	vpd= MEM_callocN(sizeof(v2dViewPanData), "v2dViewPanData");
	op->customdata= vpd;
	
	/* set pointers to owners */
	vpd->region= ar= C->region;
	vpd->v2d= v2d= &C->region->v2d;
	
	/* calculate translation factor - based on size of view */
	winx= (float)(ar->winrct.xmax - ar->winrct.xmin);
	winy= (float)(ar->winrct.ymax - ar->winrct.ymin);
	vpd->facx= (v2d->cur.xmax - v2d->cur.xmin) / winx;
	vpd->facy= (v2d->cur.ymax - v2d->cur.ymin) / winy;
		
	return 1;
}

static void pan_view_apply(bContext *C, wmOperator *op)
{
	v2dViewPanData *vpd= op->customdata;
	View2D *v2d= vpd->v2d;
	float dx, dy;
	
	/* calculate amount to move view by */
	dx= vpd->facx * (vpd->lastx - vpd->x);
	dy= vpd->facy * (vpd->lasty - vpd->y);
	
	/* only move view on an axis if change is allowed */
	if ((v2d->keepofs & V2D_LOCKOFS_X)==0) {
		v2d->cur.xmin += dx;
		v2d->cur.xmax += dx;
	}
	if ((v2d->keepofs & V2D_LOCKOFS_Y)==0) {
		v2d->cur.ymin += dy;
		v2d->cur.ymax += dy;
	}
	
	vpd->lastx= vpd->x;
	vpd->lasty= vpd->y;

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	/* XXX: add WM_NOTE_TIME_CHANGED? */
}

static void pan_view_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata= NULL;
		WM_event_remove_modal_handler(&C->window->handlers, op);				
	}
}

static int pan_view_exec(bContext *C, wmOperator *op)
{
	if (!pan_view_init(C, op))
		return OPERATOR_CANCELLED;
	
	pan_view_apply(C, op);
	pan_view_exit(C, op);
	return OPERATOR_FINISHED;
}

static int pan_view_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	v2dViewPanData *vpd= op->customdata;
	
	pan_view_init(C, op);
	
	vpd= op->customdata;
	vpd->lastx= vpd->x= event->x;
	vpd->lasty= vpd->y= event->y;
	
	pan_view_apply(C, op);

	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);

	return OPERATOR_RUNNING_MODAL;
}

static int pan_view_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	v2dViewPanData *vpd= op->customdata;
	
	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			vpd->x= event->x;
			vpd->y= event->y;
			
			pan_view_apply(C, op);
			break;
			
		case MIDDLEMOUSE:
			if (event->val==0) {
				pan_view_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);				
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

void ED_View2D_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pan View";
	ot->idname= "ED_View2D_OT_view_pan";
	
	/* api callbacks */
	ot->exec= pan_view_exec;
	ot->invoke= pan_view_invoke;
	ot->modal= pan_view_modal;
}

/* ********************************************************* */
/* Registration */

void ui_view2d_operatortypes(void)
{
	WM_operatortype_append(ED_View2D_OT_view_pan);
}

void UI_view2d_keymap(wmWindowManager *wm)
{
	ui_view2d_operatortypes();
	
	/* pan/scroll operators */
	WM_keymap_add_item(&wm->view2dkeymap, "ED_View2D_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);
	
	//WM_keymap_add_item(&wm->view2dkeymap, "ED_View2D_OT_view_scrollright", WHEELDOWNMOUSE, KM_PRESS, KM_CTRL, 0);
	//WM_keymap_add_item(&wm->view2dkeymap, "ED_View2D_OT_view_scrollleft", WHEELUPMOUSE, KM_CTRL, 0, 0);
	
	//WM_keymap_add_item(&wm->view2dkeymap, "ED_View2D_OT_view_scrolldown", WHEELDOWNMOUSE, KM_PRESS, KM_SHIFT, 0);
	//WM_keymap_add_item(&wm->view2dkeymap, "ED_View2D_OT_view_scrollup", WHEELUPMOUSE, KM_SHIFT, 0, 0);
	
	/* zoom */
	
	/* scrollbars */
	//WM_keymap_add_item(&wm->view2dkeymap, "ED_V2D_OT_scrollbar_activate", MOUSEMOVE, 0, 0, 0);
}

