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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <math.h>
#include <string.h>


#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_view2d_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

/* ********************************************************** */

typedef struct Eyedropper {
	PointerRNA ptr;
	PropertyRNA *prop;
	int index;
} Eyedropper;

static int eyedropper_init(bContext *C, wmOperator *op)
{
	Eyedropper *eye;
	
	op->customdata= eye= MEM_callocN(sizeof(Eyedropper), "Eyedropper");
	
	uiAnimContextProperty(C, &eye->ptr, &eye->prop, &eye->index);
	
	return (eye->ptr.data && eye->prop && RNA_property_editable(&eye->ptr, eye->prop));
}
 
static void eyedropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_restore(CTX_wm_window(C));
	
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
}

static int eyedropper_cancel(bContext *C, wmOperator *op)
{
	eyedropper_exit(C, op);
	return OPERATOR_CANCELLED;
}

static void eyedropper_sample(bContext *C, Eyedropper *eye, short mx, short my)
{
	const int color_manage = CTX_data_scene(C)->r.color_mgt_flag & R_COLOR_MANAGEMENT;
	float col[3];
		
	glReadBuffer(GL_FRONT);
	glReadPixels(mx, my, 1, 1, GL_RGB, GL_FLOAT, col);
	glReadBuffer(GL_BACK);
	
	if(RNA_property_type(eye->prop) == PROP_FLOAT) {
		
		if (RNA_property_array_length(&eye->ptr, eye->prop) < 3) return;

		/* convert from screen (srgb) space to linear rgb space */
		if (color_manage && RNA_property_subtype(eye->prop) == PROP_COLOR)
			srgb_to_linearrgb_v3_v3(col, col);
		
		RNA_property_float_set_array(&eye->ptr, eye->prop, col);
		
		RNA_property_update(C, &eye->ptr, eye->prop);
	}
}

/* main modal status check */
static int eyedropper_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	Eyedropper *eye = (Eyedropper *)op->customdata;
	
	switch(event->type) {
		case ESCKEY:
		case RIGHTMOUSE:
			return eyedropper_cancel(C, op);
		case LEFTMOUSE:
			if(event->val==KM_RELEASE) {
				eyedropper_sample(C, eye, event->x, event->y);
				eyedropper_exit(C, op);
				return OPERATOR_FINISHED;
			}
			break;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* init */
	if (eyedropper_init(C, op)) {
		WM_cursor_modal(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

		/* add temp handler */
		WM_event_add_modal_handler(C, op);
		
		return OPERATOR_RUNNING_MODAL;
	} else {
		eyedropper_exit(C, op);
		return OPERATOR_CANCELLED;
	}
}

/* Repeat operator */
static int eyedropper_exec (bContext *C, wmOperator *op)
{
	/* init */
	if (eyedropper_init(C, op)) {
		
		/* do something */
		
		/* cleanup */
		eyedropper_exit(C, op);
		
		return OPERATOR_FINISHED;
	} else {
		return OPERATOR_CANCELLED;
	}
}

static int eyedropper_poll(bContext *C)
{
	if (!CTX_wm_window(C)) return 0;
	else return 1;
}

void UI_OT_eyedropper(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Eyedropper";
	ot->idname= "UI_OT_eyedropper";
	ot->description= "Sample a color from the Blender Window to store in a property";
	
	/* api callbacks */
	ot->invoke= eyedropper_invoke;
	ot->modal= eyedropper_modal;
	ot->cancel= eyedropper_cancel;
	ot->exec= eyedropper_exec;
	ot->poll= eyedropper_poll;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;
	
	/* properties */
}


/* Copy Data Path Operator ------------------------ */

static int copy_data_path_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	char *path;
	int success= 0;
	int index;

	/* try to create driver using property retrieved from UI */
	uiAnimContextProperty(C, &ptr, &prop, &index);

	if (ptr.data && prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			WM_clipboard_text_set(path, FALSE);
			MEM_freeN(path);
		}
	}

	/* since we're just copying, we don't really need to do anything else...*/
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void UI_OT_copy_data_path_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Data Path";
	ot->idname= "UI_OT_copy_data_path_button";
	ot->description= "Copy the RNA data path for this property to the clipboard";

	/* callbacks */
	ot->exec= copy_data_path_button_exec;
	//op->poll= ??? // TODO: need to have some valid property before this can be done

	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

/* Reset to Default Values Button Operator ------------------------ */

static int reset_default_button_poll(bContext *C)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	int index;

	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	return (ptr.data && prop && RNA_property_editable(&ptr, prop));
}

static int reset_default_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	int success= 0;
	int index, all = RNA_boolean_get(op->ptr, "all");

	/* try to reset the nominated setting to its default value */
	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	/* if there is a valid property that is editable... */
	if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
		if(RNA_property_reset(&ptr, prop, (all)? -1: index)) {
			/* perform updates required for this property */
			RNA_property_update(C, &ptr, prop);
			success= 1;
		}
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void UI_OT_reset_default_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset to Default Value";
	ot->idname= "UI_OT_reset_default_button";
	ot->description= "Reset this property's value to its default value";

	/* callbacks */
	ot->poll= reset_default_button_poll;
	ot->exec= reset_default_button_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array.");
}

/* Copy To Selected Operator ------------------------ */

static int copy_to_selected_list(bContext *C, PointerRNA *ptr, ListBase *lb)
{
	if(RNA_struct_is_a(ptr->type, &RNA_Object))
		*lb = CTX_data_collection_get(C, "selected_editable_objects");
	else if(RNA_struct_is_a(ptr->type, &RNA_EditBone))
		*lb = CTX_data_collection_get(C, "selected_editable_bones");
	else if(RNA_struct_is_a(ptr->type, &RNA_PoseBone))
		*lb = CTX_data_collection_get(C, "selected_pose_bones");
	else
		return 0;
	
	return 1;
}

static int copy_to_selected_button_poll(bContext *C)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	int index, success= 0;

	uiAnimContextProperty(C, &ptr, &prop, &index);

	if (ptr.data && prop) {
		CollectionPointerLink *link;
		ListBase lb;

		if(copy_to_selected_list(C, &ptr, &lb)) {
			for(link= lb.first; link; link=link->next)
				if(link->ptr.data != ptr.data && RNA_property_editable(&link->ptr, prop))
					success= 1;

			BLI_freelistN(&lb);
		}
	}

	return success;
}

static int copy_to_selected_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	int success= 0;
	int index, all = RNA_boolean_get(op->ptr, "all");

	/* try to reset the nominated setting to its default value */
	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	/* if there is a valid property that is editable... */
	if (ptr.data && prop) {
		CollectionPointerLink *link;
		ListBase lb;

		if(copy_to_selected_list(C, &ptr, &lb)) {
			for(link= lb.first; link; link=link->next) {
				if(link->ptr.data != ptr.data && RNA_property_editable(&link->ptr, prop)) {
					if(RNA_property_copy(&link->ptr, &ptr, prop, (all)? -1: index)) {
						RNA_property_update(C, &link->ptr, prop);
						success= 1;
					}
				}
			}

			BLI_freelistN(&lb);
		}
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void UI_OT_copy_to_selected_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy To Selected";
	ot->idname= "UI_OT_copy_to_selected_button";
	ot->description= "Copy property from this object to selected objects or bones";

	/* callbacks */
	ot->poll= copy_to_selected_button_poll;
	ot->exec= copy_to_selected_button_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array.");
}
 
/* ********************************************************* */
/* Registration */

void UI_buttons_operatortypes(void)
{
	WM_operatortype_append(UI_OT_eyedropper);
	WM_operatortype_append(UI_OT_copy_data_path_button);
	WM_operatortype_append(UI_OT_reset_default_button);
	WM_operatortype_append(UI_OT_copy_to_selected_button);
}

