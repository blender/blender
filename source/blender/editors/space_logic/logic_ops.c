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
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stddef.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sensor_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_sca.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "logic_intern.h"

/* ************* Generic Operator Helpers ************* */

static int edit_sensor_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "sensor", &RNA_Sensor);

	if (ptr.data && ((ID*)ptr.id.data)->lib) return 0;
	return 1;
}

/* this is the nice py-api-compatible way to do it, like modifiers, 
   but not entirely working yet..
 
static void edit_sensor_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "sensor", "", 32, "Sensor", "Name of the sensor to edit");
	RNA_def_string(ot->srna, "object", "", 32, "Object", "Name of the object the sensor belongs to");
}

static int edit_sensor_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "sensor", &RNA_Sensor);
	
	if (RNA_property_is_set(op->ptr, "sensor") && RNA_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bSensor *sens = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "sensor", sens->name);
		RNA_string_set(op->ptr, "object", ob->id.name+2);
		return 1;
	}
	
	return 0;
}

static bSensor *edit_sensor_property_get(bContext *C, wmOperator *op, Object *ob)
{
	char sensor_name[32];
	char ob_name[32];
	bSensor *sens;
	
	RNA_string_get(op->ptr, "sensor", sensor_name);
	RNA_string_get(op->ptr, "object", ob_name);
	
	ob = BLI_findstring(&(CTX_data_main(C)->object), ob_name, offsetof(ID, name) + 2);
	if (!ob)
		return NULL;
	
	sens = BLI_findstring(&(ob->sensors), sensor_name, offsetof(bSensor, name));	
	return sens;
}
 */

/* ************* Remove Sensor Operator ************* */

static int sensor_remove_exec(bContext *C, wmOperator *op)
{
	/*	Object *ob;
	bSensor *sens = edit_sensor_property_get(C, op, ob);	*/
	PointerRNA ptr = CTX_data_pointer_get_type(C, "sensor", &RNA_Sensor);
	Object *ob= ptr.id.data;
	bSensor *sens= ptr.data;
	
	if (!sens)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->sensors), sens);
	free_sensor(sens);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}


/* commented along with above stuff
 static int sensor_remove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (edit_sensor_invoke_properties(C, op))
		return sensor_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}
 */

void LOGIC_OT_sensor_remove(wmOperatorType *ot)
{
	ot->name= "Remove Sensor";
	ot->description= "Remove a sensor from the active object";
	ot->idname= "LOGIC_OT_sensor_remove";
	
	//ot->invoke= sensor_remove_invoke;
	ot->exec= sensor_remove_exec;
	ot->poll= edit_sensor_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	//edit_sensor_properties(ot);
}

static int sensor_add_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	bSensor *sens;
	int type= RNA_enum_get(op->ptr, "type");

	sens= new_sensor(type);
	BLI_addtail(&(ob->sensors), sens);
	make_unique_prop_names(C, sens->name);
	ob->scaflag |= OB_SHOWSENS;

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

void LOGIC_OT_sensor_add(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Add Sensor";
	ot->description = "Add a sensor to the active object";
	ot->idname= "LOGIC_OT_sensor_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= sensor_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", sensor_type_items, SENS_ALWAYS, "Type", "Type of sensor to add");
}

void ED_operatortypes_logic(void)
{
	WM_operatortype_append(LOGIC_OT_sensor_remove);
	WM_operatortype_append(LOGIC_OT_sensor_add);
}
