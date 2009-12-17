/**
 * $Id: interface_ops.c 24699 2009-11-20 10:21:31Z aligorith $
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

/* Copy to Clipboard Button Operator ------------------------ */

static int copy_clipboard_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop= NULL;
	char *path;
	short success= 0;
	int index;

	/* try to create driver using property retrieved from UI */
	memset(&ptr, 0, sizeof(PointerRNA));
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

void UI_OT_copy_clipboard_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Data Path";
	ot->idname= "UI_OT_copy_clipboard_button";
	ot->description= "Copy the RNA data path for this property to the clipboard.";

	/* callbacks */
	ot->exec= copy_clipboard_button_exec;
	//op->poll= ??? // TODO: need to have some valid property before this can be done

	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

/* Reset to Default Values Button Operator ------------------------ */

static int reset_default_button_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop= NULL;
	short success= 0;
	int index, len;
	int all = RNA_boolean_get(op->ptr, "all");

	/* try to reset the nominated setting to its default value */
	memset(&ptr, 0, sizeof(PointerRNA));
	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	/* if there is a valid property that is editable... */
	if (ptr.data && prop && RNA_property_editable(&ptr, prop)) {
		/* get the length of the array to work with */
		len= RNA_property_array_length(&ptr, prop);
		
		/* get and set the default values as appropriate for the various types */
		switch (RNA_property_type(prop)) {
			case PROP_BOOLEAN:
				if (len) {
					if (all) {
						int *tmparray= MEM_callocN(sizeof(int)*len, "reset_defaults - boolean");
						
						RNA_property_boolean_get_default_array(&ptr, prop, tmparray);
						RNA_property_boolean_set_array(&ptr, prop, tmparray);
						
						MEM_freeN(tmparray);
					}
					else {
						int value= RNA_property_boolean_get_default_index(&ptr, prop, index);
						RNA_property_boolean_set_index(&ptr, prop, index, value);
					}
				}
				else {
					int value= RNA_property_boolean_get_default(&ptr, prop);
					RNA_property_boolean_set(&ptr, prop, value);
				}
				break;
			case PROP_INT:
				if (len) {
					if (all) {
						int *tmparray= MEM_callocN(sizeof(int)*len, "reset_defaults - int");
						
						RNA_property_int_get_default_array(&ptr, prop, tmparray);
						RNA_property_int_set_array(&ptr, prop, tmparray);
						
						MEM_freeN(tmparray);
					}
					else {
						int value= RNA_property_int_get_default_index(&ptr, prop, index);
						RNA_property_int_set_index(&ptr, prop, index, value);
					}
				}
				else {
					int value= RNA_property_int_get_default(&ptr, prop);
					RNA_property_int_set(&ptr, prop, value);
				}
				break;
			case PROP_FLOAT:
				if (len) {
					if (all) {
						float *tmparray= MEM_callocN(sizeof(float)*len, "reset_defaults - float");
						
						RNA_property_float_get_default_array(&ptr, prop, tmparray);
						RNA_property_float_set_array(&ptr, prop, tmparray);
						
						MEM_freeN(tmparray);
					}
					else {
						float value= RNA_property_float_get_default_index(&ptr, prop, index);
						RNA_property_float_set_index(&ptr, prop, index, value);
					}
				}
				else {
					float value= RNA_property_float_get_default(&ptr, prop);
					RNA_property_float_set(&ptr, prop, value);
				}
				break;
			case PROP_ENUM:
			{
				int value= RNA_property_enum_get_default(&ptr, prop);
				RNA_property_enum_set(&ptr, prop, value);
			}
				break;
			
			//case PROP_POINTER:
			//case PROP_STRING:
			default: 
				// FIXME: many of the other types such as strings and pointers need this implemented too!
				break;
		}
		
		/* perform updates required for this property */
		RNA_property_update(C, &ptr, prop);
		
		success= 1;
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void UI_OT_reset_default_button(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset to Default Value";
	ot->idname= "UI_OT_reset_default_button";
	ot->description= "Copy the RNA data path for this property to the clipboard.";

	/* callbacks */
	ot->exec= reset_default_button_exec;
	//op->poll= ??? // TODO: need to have some valid property before this can be done

	/* flags */
	ot->flag= OPTYPE_REGISTER;
	
	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Reset to default values all elements of the array.");
}
 
/* ********************************************************* */
/* Registration */

void ui_buttons_operatortypes(void)
{
	WM_operatortype_append(UI_OT_copy_clipboard_button);
	WM_operatortype_append(UI_OT_reset_default_button);
}

