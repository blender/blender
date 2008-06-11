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
void WM_operatortype_append(void (*opfunc)(wmOperatorType*))
{
	wmOperatorType *ot;
	
	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype");
	opfunc(ot);
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

/* ************ window / screen border operator definitions ************** */
/*
 * This is and example of global operator working with
 * the gesture system.
 */
static int border_select_init(bContext *C, wmOperator *op)
{
	int x, y;

	if(!(OP_get_int(op, "start_x", &x) && OP_get_int(op, "start_y", &y)))
		return 0;

	WM_gesture_init(C, GESTURE_RECT);
	return 1;
}

static int border_select_apply(bContext *C, wmOperator *op)
{
	wmGestureRect rect;
	int x, y, endx, endy;

	OP_get_int(op, "start_x", &x);
	OP_get_int(op, "start_y", &y);
	OP_get_int(op, "end_x", &endx);
	OP_get_int(op, "end_y", &endy);

	rect.gesture.next= rect.gesture.prev= NULL;
	rect.gesture.type= GESTURE_RECT;
	rect.x1= x;
	rect.y1= y;
	rect.x2= endx;
	rect.y2= endy;
	WM_gesture_update(C, (wmGesture *) &rect);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_GESTURE_CHANGED, GESTURE_RECT, NULL);

	return 1;
}

static int border_select_exit(bContext *C, wmOperator *op)
{
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	OP_free_property(op);
	return 1;
}

static int border_select_exec(bContext *C, wmOperator *op)
{
	if(!border_select_init(C, op))
		return OPERATOR_CANCELLED;
	
	border_select_apply(C, op);
	border_select_exit(C, op);

	return OPERATOR_FINISHED;
}

static int border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* operator arguments and storage. */
	OP_verify_int(op, "start_x", event->x, NULL);
	OP_verify_int(op, "start_y", event->y, NULL);

	if(!border_select_init(C, op))
		return OPERATOR_CANCELLED;

	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	return OPERATOR_RUNNING_MODAL;
}

static int border_select_cancel(bContext *C, wmOperator *op)
{
	WM_event_remove_modal_handler(&C->window->handlers, op);
	border_select_exit(C, op);
	return OPERATOR_CANCELLED;
}

static int border_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case MOUSEMOVE:
			OP_set_int(op, "end_x", event->x);
			OP_set_int(op, "end_y", event->y);
			border_select_apply(C, op);
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_GESTURE_CHANGED, GESTURE_RECT, NULL);
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
			break;
		case LEFTMOUSE:
			if(event->val==0) {
				border_select_apply(C, op);
				WM_gesture_end(C, GESTURE_RECT);
				border_select_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
				return OPERATOR_FINISHED;
			}
			break;
		case ESCKEY:
			return border_select_cancel(C, op);
	}
	return OPERATOR_RUNNING_MODAL;
}

void WM_OT_border_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border select";
	ot->idname= "WM_OT_border_select";

	ot->exec= border_select_exec;
	ot->invoke= border_select_invoke;
	ot->cancel= border_select_cancel;
	ot->modal= border_select_modal;

	ot->poll= WM_operator_winactive;
}
 
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
	WM_operatortype_append(WM_OT_border_select);
}

/* wrapped to get property from a operator. */
IDProperty *op_get_property(wmOperator *op, char *name)
{
	IDProperty *prop;
	
	if(!op->properties)
		return NULL;

	prop= IDP_GetPropertyFromGroup(op->properties, name);
	return prop;
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
	if(op->properties) {
		IDP_FreeProperty(op->properties);
		/*
		 * This need change, when the idprop code only
		 * need call IDP_FreeProperty. (check BKE_idprop.h)
		 */
		MEM_freeN(op->properties);
		op->properties= NULL;
	}
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
	int status= 0;

	if ((prop) && (prop->type == IDP_INT)) {
		(*value)= prop->data.val;
		status= 1;
	}
	return (status);
}

int OP_get_float(wmOperator *op, char *name, float *value)
{
	IDProperty *prop= op_get_property(op, name);
	int status= 0;

	if ((prop) && (prop->type == IDP_FLOAT)) {
		(*value)= *(float*)&prop->data.val;
		status= 1;
	}
	return (status);
}

int OP_get_int_array(wmOperator *op, char *name, int *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	int status= 0;
	int *pointer;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (int *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 1;
	}
	return (status);
}

int OP_get_float_array(wmOperator *op, char *name, float *array, short *len)
{
	IDProperty *prop= op_get_property(op, name);
	short i;
	float *pointer;
	int status= 0;

	if ((prop) && (prop->type == IDP_ARRAY)) {
		pointer= (float *) prop->data.pointer;

		for(i= 0; (i < prop->len) && (i < *len); i++)
			array[i]= pointer[i];

		(*len)= i;
		status= 1;
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

void OP_verify_int(wmOperator *op, char *name, int value, int *result)
{
	int rvalue;

	if(OP_get_int(op, name, &rvalue))
		value= rvalue;
	else
		OP_set_int(op, name, value);

	if(result)
		*result= value;
}

void OP_verify_float(wmOperator *op, char *name, float value, int *result)
{
	float rvalue;

	if(OP_get_float(op, name, &rvalue))
		value= rvalue;
	else
		OP_set_float(op, name, value);
	
	if(result)
		*result= value;
}

char *OP_verify_string(wmOperator *op, char *name, char *str)
{
	char *result= OP_get_string(op, name);

	if(!result) {
		OP_set_string(op, name, str);
		result= OP_get_string(op, name);
	}

	return result;
}

void OP_verify_int_array(wmOperator *op, char *name, int *array, short len, int *resultarray, short *resultlen)
{
	int rarray[1];
	short rlen= 1;

	if(resultarray && resultlen) {
		if(!OP_get_int_array(op, name, resultarray, &rlen)) {
			OP_set_int_array(op, name, array, len);
			OP_get_int_array(op, name, resultarray, resultlen);
		}
	}
	else {
		if(!OP_get_int_array(op, name, rarray, &rlen))
			OP_set_int_array(op, name, array, len);
	}
}

void OP_verify_float_array(wmOperator *op, char *name, float *array, short len, float *resultarray, short *resultlen)
{
	float rarray[1];
	short rlen= 1;

	if(resultarray && resultlen) {
		if(!OP_get_float_array(op, name, resultarray, &rlen)) {
			OP_set_float_array(op, name, array, len);
			OP_get_float_array(op, name, resultarray, resultlen);
		}
	}
	else {
		if(!OP_get_float_array(op, name, rarray, &rlen))
			OP_set_float_array(op, name, array, len);
	}
}

