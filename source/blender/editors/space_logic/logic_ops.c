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
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"

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

static int edit_controller_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "controller", &RNA_Controller);

	if (ptr.data && ((ID*)ptr.id.data)->lib) return 0;
	return 1;
}

static int edit_actuator_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "actuator", &RNA_Actuator);

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
/*
static void edit_controller_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "controller", "", 32, "Controller", "Name of the controller to edit");
	RNA_def_string(ot->srna, "object", "", 32, "Object", "Name of the object the controller belongs to");
}

static int edit_controller_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "controller", &RNA_Controller);
	
	if (RNA_property_is_set(op->ptr, "controller") && RNA_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bController *cont = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "controller", cont->name);
		RNA_string_set(op->ptr, "object", ob->id.name+2);
		return 1;
	}
	
	return 0;
}

static bController *edit_controller_property_get(bContext *C, wmOperator *op, Object *ob)
{
	char controller_name[32];
	char ob_name[32];
	bController *cont;
	
	RNA_string_get(op->ptr, "controller", controller_name);
	RNA_string_get(op->ptr, "object", ob_name);
	
	ob = BLI_findstring(&(CTX_data_main(C)->object), ob_name, offsetof(ID, name) + 2);
	if (!ob)
		return NULL;
	
	cont = BLI_findstring(&(ob->controllers), controller_name, offsetof(bController, name));	
	return cont;
}
 */

/*
static void edit_actuator_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "actuator", "", 32, "Actuator", "Name of the actuator to edit");
	RNA_def_string(ot->srna, "object", "", 32, "Object", "Name of the object the actuator belongs to");
}

static int edit_actuator_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "actuator", &RNA_Actuator);
	
	if (RNA_property_is_set(op->ptr, "actuator") && RNA_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bActuator *act = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "actuator",act->name);
		RNA_string_set(op->ptr, "object", ob->id.name+2);
		return 1;
	}
	
	return 0;
}

static bController *edit_actuator_property_get(bContext *C, wmOperator *op, Object *ob)
{
	char actuator_name[32];
	char ob_name[32];
	bActuator *act;
	
	RNA_string_get(op->ptr, "actuator", actuator_name);
	RNA_string_get(op->ptr, "object", ob_name);
	
	ob = BLI_findstring(&(CTX_data_main(C)->object), ob_name, offsetof(ID, name) + 2);
	if (!ob)
		return NULL;
	
	cont = BLI_findstring(&(ob->actuators), actuator_name, offsetof(bActuator, name));	
	return act;
}


/* ************* Add/Remove Sensor Operator ************* */

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

/* ************* Add/Remove Controller Operator ************* */

static int controller_remove_exec(bContext *C, wmOperator *op)
{
	/*	Object *ob;
	bController *cont = edit_controller_property_get(C, op, ob);	*/
	PointerRNA ptr = CTX_data_pointer_get_type(C, "controller", &RNA_Controller);
	Object *ob= ptr.id.data;
	bController *cont= ptr.data;
	
	if (!cont)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->controllers), cont);
	free_controller(cont);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}


/* commented along with above stuff
 static int controller_remove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (edit_controller_invoke_properties(C, op))
		return controller_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}
 */

void LOGIC_OT_controller_remove(wmOperatorType *ot)
{
	ot->name= "Remove Controller";
	ot->description= "Remove a controller from the active object";
	ot->idname= "LOGIC_OT_controller_remove";
	
	//ot->invoke= controller_remove_invoke;
	ot->exec= controller_remove_exec;
	ot->poll= edit_controller_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	//edit_controller_properties(ot);
}

static int controller_add_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	bController *cont;
	int type= RNA_enum_get(op->ptr, "type");

	cont= new_controller(type);
	BLI_addtail(&(ob->controllers), cont);
	make_unique_prop_names(C, cont->name);
	ob->scaflag |= OB_SHOWCONT;

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

void LOGIC_OT_controller_add(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Add Controller";
	ot->description = "Add a controller to the active object";
	ot->idname= "LOGIC_OT_controller_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= controller_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", controller_type_items, CONT_LOGIC_AND, "Type", "Type of controller to add");
}

/* ************* Add/Remove Actuator Operator ************* */

static int actuator_remove_exec(bContext *C, wmOperator *op)
{
	/*	Object *ob;
	bActuator *cont = edit_actuator_property_get(C, op, ob);	*/
	PointerRNA ptr = CTX_data_pointer_get_type(C, "actuator", &RNA_Actuator);
	Object *ob= ptr.id.data;
	bActuator *act= ptr.data;
	
	if (!act)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->actuators), act);
	free_actuator(act);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}


/* commented along with above stuff
 static int actuator_remove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (edit_actuator_invoke_properties(C, op))
		return actuator_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}
 */

void LOGIC_OT_actuator_remove(wmOperatorType *ot)
{
	ot->name= "Remove Actuator";
	ot->description= "Remove a actuator from the active object";
	ot->idname= "LOGIC_OT_actuator_remove";
	
	//ot->invoke= actuator_remove_invoke;
	ot->exec= actuator_remove_exec;
	ot->poll= edit_actuator_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	//edit_controller_properties(ot);
}

static int actuator_add_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	bActuator *act;
	int type= RNA_enum_get(op->ptr, "type");

	act= new_actuator(type);
	BLI_addtail(&(ob->actuators), act);
	make_unique_prop_names(C, act->name);
	ob->scaflag |= OB_SHOWCONT;

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

void LOGIC_OT_actuator_add(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Add Actuator";
	ot->description = "Add a actuator to the active object";
	ot->idname= "LOGIC_OT_actuator_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actuator_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", actuator_type_items, CONT_LOGIC_AND, "Type", "Type of actuator to add");
}

void ED_operatortypes_logic(void)
{
	WM_operatortype_append(LOGIC_OT_sensor_remove);
	WM_operatortype_append(LOGIC_OT_sensor_add);
	WM_operatortype_append(LOGIC_OT_controller_remove);
	WM_operatortype_append(LOGIC_OT_controller_add);
	WM_operatortype_append(LOGIC_OT_actuator_remove);
	WM_operatortype_append(LOGIC_OT_actuator_add);
}
