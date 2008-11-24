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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_window.h"
#include "wm_subwindow.h"
#include "wm_event_system.h"

static ListBase global_ops= {NULL, NULL};

/* ************ operator API, exported ********** */

wmOperatorType *WM_operatortype_find(const char *idname)
{
	wmOperatorType *ot;
	
	for(ot= global_ops.first; ot; ot= ot->next) {
		if(strncmp(ot->idname, idname, OP_MAX_TYPENAME)==0)
		   return ot;
	}
	return NULL;
}

/* all ops in 1 list (for time being... needs evaluation later) */
void WM_operatortype_append(void (*opfunc)(wmOperatorType*))
{
	wmOperatorType *ot;
	
	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->rna= RNA_def_struct(&BLENDER_RNA, "", "Operator", "");
	opfunc(ot);
	RNA_def_struct_identifier(ot->rna, ot->idname, ot->name);
	BLI_addtail(&global_ops, ot);
}

/* ************ default ops, exported *********** */

int WM_operator_confirm(bContext *C, wmOperator *op, wmEvent *event)
{
//	if(okee(op->type->name)) {
//		return op->type->exec(C, op);
//	}
	return 0;
}
int WM_operator_winactive(bContext *C)
{
	if(C->window==NULL) return 0;
	return 1;
}

/* ************ window / screen operator definitions ************** */

static void WM_OT_window_duplicate(wmOperatorType *ot)
{
	ot->name= "Duplicate Window";
	ot->idname= "WM_OT_window_duplicate";
	
	ot->invoke= NULL; //WM_operator_confirm;
	ot->exec= wm_window_duplicate_op;
	ot->poll= WM_operator_winactive;
}

static void WM_OT_save_homefile(wmOperatorType *ot)
{
	ot->name= "Save User Settings";
	ot->idname= "WM_OT_save_homefile";
	
	ot->invoke= NULL; //WM_operator_confirm;
	ot->exec= WM_write_homefile;
	ot->poll= WM_operator_winactive;
	
	ot->flag= OPTYPE_REGISTER;
}

static void WM_OT_window_fullscreen_toggle(wmOperatorType *ot)
{
    ot->name= "Toggle Fullscreen";
    ot->idname= "WM_OT_window_fullscreen_toggle";

    ot->invoke= NULL;
    ot->exec= wm_window_fullscreen_toggle_op;
    ot->poll= WM_operator_winactive;
}

static void WM_OT_exit_blender(wmOperatorType *ot)
{
	ot->name= "Exit Blender";
	ot->idname= "WM_OT_exit_blender";

	ot->invoke= NULL; /* do confirm stuff */
	ot->exec= wm_exit_blender_op;
	ot->poll= WM_operator_winactive;
}

/* ************ window gesture operator-callback definitions ************** */
/*
 * These are default callbacks for use in operators requiring gesture input
 */

/* **************** Border gesture *************** */

/* Border gesture has two types:
   1) WM_GESTURE_CROSS_RECT: starts a cross, on mouse click it changes to border 
   2) WM_GESTURE_RECT: starts immediate as a border, on mouse click or release it ends

   It stores 4 values (xmin, xmax, ymin, ymax) and event it ended with (event_type)
*/

static void border_select_apply(bContext *C, wmOperator *op, int event_type)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	
	/* operator arguments and storage. */
	RNA_int_default(op->rna, "xmin", rect->xmin);
	RNA_int_default(op->rna, "ymin", rect->ymin);
	RNA_int_default(op->rna, "xmax", rect->xmax);
	RNA_int_default(op->rna, "ymax", rect->ymax);
	
	RNA_int_default(op->rna, "event_type", event_type);
	
	op->type->exec(C, op);
}

static void border_select_end(bContext *C, wmOperator *op)
{
	wmGesture *gesture= op->customdata;
	
	WM_gesture_end(C, gesture);	/* frees gesture itself, and unregisters from window */
	op->customdata= NULL;
	WM_event_remove_modal_handler(&C->window->handlers, op);
	WM_event_add_notifier(C->wm, C->window, gesture->swinid, WM_NOTE_AREA_REDRAW, 0, NULL);
	
}

int WM_border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_CROSS_RECT);

	/* add modal handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	WM_event_add_notifier(C->wm, C->window, C->screen->subwinactive, WM_NOTE_GESTURE_REDRAW, 0, NULL);

	return OPERATOR_RUNNING_MODAL;
}

int WM_border_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	int sx, sy;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_subwindow_getorigin(C->window, gesture->swinid, &sx, &sy);
			
			if(gesture->type==WM_GESTURE_CROSS_RECT && gesture->mode==0) {
				rect->xmin= rect->xmax= event->x - sx;
				rect->ymin= rect->ymax= event->y - sy;
			}
			else {
				rect->xmax= event->x - sx;
				rect->ymax= event->y - sy;
			}
			
			WM_event_add_notifier(C->wm, C->window, gesture->swinid, WM_NOTE_GESTURE_REDRAW, 0, NULL);

			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if(event->val==1) {
				if(gesture->type==WM_GESTURE_CROSS_RECT && gesture->mode==0) {
					gesture->mode= 1;
					WM_event_add_notifier(C->wm, C->window, gesture->swinid, WM_NOTE_GESTURE_REDRAW, 0, NULL);
				}
			}
			else {
				border_select_apply(C, op, event->type);
				border_select_end(C, op);
				return OPERATOR_FINISHED;
			}
			break;
		case ESCKEY:
			border_select_end(C, op);
			return OPERATOR_CANCELLED;
	}
	return OPERATOR_RUNNING_MODAL;
}

/* **************** Tweak gesture *************** */

static int tweak_gesture_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_TWEAK);
	
	/* add modal handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	WM_event_add_notifier(C->wm, C->window, C->screen->subwinactive, WM_NOTE_GESTURE_REDRAW, 0, NULL);
	
	return OPERATOR_RUNNING_MODAL;
}

static void tweak_gesture_end(bContext *C, wmOperator *op)
{
	wmGesture *gesture= op->customdata;
	
	WM_gesture_end(C, gesture);	/* frees gesture itself, and unregisters from window */
	op->customdata= NULL;
	WM_event_remove_modal_handler(&C->window->handlers, op);
	WM_event_add_notifier(C->wm, C->window, gesture->swinid, WM_NOTE_AREA_REDRAW, 0, NULL);
	
}

static int tweak_gesture_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	int sx, sy, val;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_subwindow_getorigin(C->window, gesture->swinid, &sx, &sy);
			
			rect->xmax= event->x - sx;
			rect->ymax= event->y - sy;
			
			if((val= wm_gesture_evaluate(C, gesture))) {
				wmEvent event;
					
				event= *(C->window->eventstate);
				event.type= EVT_TWEAK;
				event.val= val;
				/* mouse coords! */
				wm_event_add(C->window, &event);
				
				tweak_gesture_end(C, op);
				return OPERATOR_FINISHED;
			}
			else
				WM_event_add_notifier(C->wm, C->window, gesture->swinid, WM_NOTE_GESTURE_REDRAW, 0, NULL);
			
			break;
			
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			if(gesture->event_type==event->type) {
				wm_gesture_evaluate(C, gesture);
				tweak_gesture_end(C, op);
				return OPERATOR_FINISHED;
			}
			break;
	}
	return OPERATOR_RUNNING_MODAL;
}

void WM_OT_tweak_gesture(wmOperatorType *ot)
{
	ot->name= "Tweak Gesture";
	ot->idname= "WM_OT_tweak_gesture";
	
	ot->invoke= tweak_gesture_invoke;
	ot->modal= tweak_gesture_modal;

	ot->poll= WM_operator_winactive;
}


/* ******************************************************* */
 
/* called on initialize WM_exit() */
void wm_operatortype_free(void)
{
	BLI_freelistN(&global_ops);
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
	WM_operatortype_append(WM_OT_window_duplicate);
	WM_operatortype_append(WM_OT_save_homefile);
	WM_operatortype_append(WM_OT_window_fullscreen_toggle);
	WM_operatortype_append(WM_OT_exit_blender);
	WM_operatortype_append(WM_OT_tweak_gesture);
}

/* default keymap for windows and screens, only call once per WM */
void wm_window_keymap(wmWindowManager *wm)
{
	/* note, this doesn't replace existing keymap items */
	WM_keymap_verify_item(&wm->windowkeymap, "WM_OT_window_duplicate", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(&wm->windowkeymap, "WM_OT_save_homefile", UKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(&wm->windowkeymap, "WM_OT_window_fullscreen_toggle", FKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(&wm->windowkeymap, "WM_OT_exit_blender", QKEY, KM_PRESS, KM_CTRL, 0);
}

