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
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_idprop.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_window.h"
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
void WM_operatortypelist_append(ListBase *lb)
{
	addlisttolist(&global_ops, lb);
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

/* ************ window / screen border operator definitions ************** */
/*
 * This is and example of global operator working with
 * the gesture system.
 */
static int border_select_init(bContext *C, wmOperator *op)
{
	OP_set_int(op, "start_x", op->veci.x);
	OP_set_int(op, "start_y", op->veci.y);
	WM_gesture_init(C, GESTURE_RECT);
	return 1;
}

static int border_select_exec(bContext *C, wmOperator *op)
{
	wmGestureRect rect;
	int x, y;

	OP_get_int(op, "start_x", &x);
	OP_get_int(op, "start_y", &y);

	rect.gesture.next= rect.gesture.prev= NULL;
	rect.gesture.type= GESTURE_RECT;
	rect.x1= x;
	rect.y1= y;
	rect.x2= op->veci.x;
	rect.y2= op->veci.y;
	WM_gesture_update(C, (wmGesture *) &rect);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_GESTURE_CHANGED, GESTURE_RECT, NULL);
	return 1;
}

static int border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* operator arguments and storage. */
	op->properties= NULL;
	op->veci.x= event->x;
	op->veci.y= event->y;

	if(0==border_select_init(C, op))
		return 1;

	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	return 0;
}

static int border_select_exit(bContext *C, wmOperator *op)
{
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	OP_free_property(op);
	return 1;
}

static int border_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case MOUSEMOVE:
			op->veci.x= event->x;
			op->veci.y= event->y;
			border_select_exec(C, op);
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
			break;
		case LEFTMOUSE:
			if(event->val==0) {
				wmGestureRect rect;
				int x, y;

				OP_get_int(op, "start_x", &x);
				OP_get_int(op, "start_y", &y);

				rect.gesture.next= rect.gesture.prev= NULL;
				rect.gesture.type= GESTURE_RECT;
				rect.x1= x;
				rect.y1= y;
				rect.x2= op->veci.x;
				rect.y2= op->veci.y;
				WM_gesture_update(C, (wmGesture*)&rect);
				WM_gesture_end(C, GESTURE_RECT);

				border_select_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
			}
			break;
		case ESCKEY:
			WM_event_remove_modal_handler(&C->window->handlers, op);
			border_select_exit(C, op);
			break;
	}
	return 1;
}

void WM_OT_border_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border select";
	ot->idname= "WM_OT_border_select";

	ot->init= border_select_init;
	ot->invoke= border_select_invoke;
	ot->modal= border_select_modal;
	ot->exec= border_select_exec;
	ot->exit= border_select_exit;

	ot->poll= WM_operator_winactive;
}
 
#define ADD_OPTYPE(opfunc)	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype"); \
							opfunc(ot);  \
							BLI_addtail(&global_ops, ot)


/* called on initialize WM_exit() */
void wm_operatortype_free(void)
{
	BLI_freelistN(&global_ops);
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
	wmOperatorType *ot;
	
	ADD_OPTYPE(WM_OT_window_duplicate);
	ADD_OPTYPE(WM_OT_save_homefile);
	ADD_OPTYPE(WM_OT_window_fullscreen_toggle);
	ADD_OPTYPE(WM_OT_exit_blender);
	ADD_OPTYPE(WM_OT_border_select);
}

/* wrapped to get property from a operator. */
IDProperty *op_get_property(wmOperator *op, char *name)
{
	IDProperty *prop= IDP_GetPropertyFromGroup(op->properties, name);
	return(prop);
}

/*
 * We need create a "group" to store the operator properties.
 * We don't have a WM_operator_new or some thing like that,
 * so this function is called by all the OP_set_* function
 * in case that op->properties is equal to NULL.
 */
void op_init_property(wmOperator *op)
{
	IDPropertyTemplate val;
	val.i = 0; /* silence MSVC warning about uninitialized var when debugging */
	op->properties= IDP_New(IDP_GROUP, val, "property");
}

/* ***** Property API, exported ***** */
void OP_free_property(wmOperator *op)
{
	IDP_FreeProperty(op->properties);
	/*
	 * This need change, when the idprop code only
	 * need call IDP_FreeProperty. (check BKE_idprop.h)
	 */
	MEM_freeN(op->properties);
	op->properties= NULL;
}

void OP_set_int(wmOperator *op, char *name, int value)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.i= value;
	prop= IDP_New(IDP_INT, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_float(wmOperator *op, char *name, float value)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.f= value;
	prop= IDP_New(IDP_FLOAT, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_int_array(wmOperator *op, char *name, int *array, short len)
{
	IDPropertyTemplate val;
	IDProperty *prop;
	short i;
	int *pointer;

	if(!op->properties)
		op_init_property(op);

	val.array.len= len;
	val.array.type= IDP_INT;
	prop= IDP_New(IDP_ARRAY, val, name);

	pointer= (int *)prop->data.pointer;
	for(i= 0; i < len; i++)
		pointer[i]= array[i];
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_float_array(wmOperator *op, char *name, float *array, short len)
{
	IDPropertyTemplate val;
	IDProperty *prop;
	short i;
	float *pointer;

	if(!op->properties)
		op_init_property(op);

	val.array.len= len;
	val.array.type= IDP_FLOAT;
	prop= IDP_New(IDP_ARRAY, val, name);

	pointer= (float *) prop->data.pointer;
	for(i= 0; i < len; i++)
		pointer[i]= array[i];
	IDP_ReplaceInGroup(op->properties, prop);
}

void OP_set_string(wmOperator *op, char *name, char *str)
{
	IDPropertyTemplate val;
	IDProperty *prop;

	if(!op->properties)
		op_init_property(op);

	val.str= str;
	prop= IDP_New(IDP_STRING, val, name);
	IDP_ReplaceInGroup(op->properties, prop);
}

int OP_get_int(wmOperator *op, char *name, int *value)
{
	IDProperty *prop= op_get_property(op, name);
	int status= 1;

	if ((prop) && (prop->type == IDP_INT)) {
		(*value)= prop->data.val;
		status= 0;
	}
	return (status);
}

int OP_get_float(wmOperator *op, char *name, float *value)
{
	IDProperty *prop= op_get_property(op, name);
	int status= 1;

	if ((prop) && (prop->type == IDP_FLOAT)) {
		(*value)= *(float*)&prop->data.val;
		status= 0;
	}
	return (status);
}

int OP_get_int_array(wmOperator *op, char *name, int *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	int status= 1;
	int *pointer;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (int *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 0;
	}
	return (status);
}

int OP_get_float_array(wmOperator *op, char *name, float *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	float *pointer;
	int status= 1;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (float *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 0;
	}
	return (status);
}

char *OP_get_string(wmOperator *op, char *name)
{
	IDProperty *prop= op_get_property(op, name);
	if ((prop) && (prop->type == IDP_STRING))
		return ((char *) prop->data.pointer);
	return (NULL);
}
