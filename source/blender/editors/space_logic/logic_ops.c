/*
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

/** \file blender/editors/space_logic/logic_ops.c
 *  \ingroup splogic
 */

#include <stddef.h>

#include "DNA_object_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_sca.h"

#include "ED_logic.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "logic_intern.h"

/* ************* Generic Operator Helpers ************* */
static int edit_sensor_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "sensor", &RNA_Sensor);

	if (ptr.data && ((ID *)ptr.id.data)->lib) return 0;
	return 1;
}

static int edit_controller_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "controller", &RNA_Controller);

	if (ptr.data && ((ID *)ptr.id.data)->lib) return 0;
	return 1;
}

static int edit_actuator_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "actuator", &RNA_Actuator);

	if (ptr.data && ((ID *)ptr.id.data)->lib) return 0;
	return 1;
}

static void edit_sensor_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "sensor", NULL, MAX_NAME, "Sensor", "Name of the sensor to edit");
	RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the object the sensor belongs to");
}

static int edit_sensor_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "sensor", &RNA_Sensor);
	
	if (RNA_struct_property_is_set(op->ptr, "sensor") && RNA_struct_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bSensor *sens = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "sensor", sens->name);
		RNA_string_set(op->ptr, "object", ob->id.name + 2);
		return 1;
	}
	
	return 0;
}

static Object *edit_object_property_get(bContext *C, wmOperator *op)
{
	char ob_name[MAX_NAME];
	Object *ob;

	RNA_string_get(op->ptr, "object", ob_name);

	/* if ob_name is valid try to find the object with this name
	 * otherwise gets the active object */
	if (*ob_name)
		ob = BLI_findstring(&(CTX_data_main(C)->object), ob_name, offsetof(ID, name) + 2);
	else
		ob = ED_object_active_context(C);

	return ob;
}

static bSensor *edit_sensor_property_get(bContext *C, wmOperator *op, Object **ob)
{
	char sensor_name[MAX_NAME];
	bSensor *sens;
	
	RNA_string_get(op->ptr, "sensor", sensor_name);

	*ob = edit_object_property_get(C, op);
	if (!*ob) return NULL;
	
	sens = BLI_findstring(&((*ob)->sensors), sensor_name, offsetof(bSensor, name));
	return sens;
}

static void edit_controller_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "controller", NULL, MAX_NAME, "Controller", "Name of the controller to edit");
	RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the object the controller belongs to");
}

static int edit_controller_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "controller", &RNA_Controller);
	
	if (RNA_struct_property_is_set(op->ptr, "controller") && RNA_struct_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bController *cont = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "controller", cont->name);
		RNA_string_set(op->ptr, "object", ob->id.name + 2);
		return 1;
	}
	
	return 0;
}

static bController *edit_controller_property_get(bContext *C, wmOperator *op, Object **ob)
{
	char controller_name[MAX_NAME];
	bController *cont;
	
	RNA_string_get(op->ptr, "controller", controller_name);

	*ob = edit_object_property_get(C, op);
	if (!*ob) return NULL;
	
	cont = BLI_findstring(&((*ob)->controllers), controller_name, offsetof(bController, name));
	return cont;
}

static void edit_actuator_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "actuator", NULL, MAX_NAME, "Actuator", "Name of the actuator to edit");
	RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the object the actuator belongs to");
}

static int edit_actuator_invoke_properties(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "actuator", &RNA_Actuator);
	
	if (RNA_struct_property_is_set(op->ptr, "actuator") && RNA_struct_property_is_set(op->ptr, "object") )
		return 1;
	
	if (ptr.data) {
		bActuator *act = ptr.data;
		Object *ob = ptr.id.data;
		
		RNA_string_set(op->ptr, "actuator", act->name);
		RNA_string_set(op->ptr, "object", ob->id.name + 2);
		return 1;
	}
	
	return 0;
}

static bActuator *edit_actuator_property_get(bContext *C, wmOperator *op, Object **ob)
{
	char actuator_name[MAX_NAME];
	bActuator *act;
	
	RNA_string_get(op->ptr, "actuator", actuator_name);

	*ob = edit_object_property_get(C, op);
	if (!*ob) return NULL;
	
	act = BLI_findstring(&((*ob)->actuators), actuator_name, offsetof(bActuator, name));
	return act;
}

static int logicbricks_move_property_get(wmOperator *op)
{
	int type = RNA_enum_get(op->ptr, "direction");

	if (type == 1)
		return true;
	else
		return false;
}

/* ************* Add/Remove Sensor Operator ************* */

static int sensor_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bSensor *sens = edit_sensor_property_get(C, op, &ob);
	
	if (!sens)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->sensors), sens);
	free_sensor(sens);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int sensor_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_sensor_invoke_properties(C, op))
		return sensor_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_sensor_remove(wmOperatorType *ot)
{
	ot->name = "Remove Sensor";
	ot->description = "Remove a sensor from the active object";
	ot->idname = "LOGIC_OT_sensor_remove";
	
	ot->invoke = sensor_remove_invoke;
	ot->exec = sensor_remove_exec;
	ot->poll = edit_sensor_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_sensor_properties(ot);
}

static int sensor_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	bSensor *sens;
	PointerRNA sens_ptr;
	PropertyRNA *prop;
	const char *sens_name;
	char name[MAX_NAME];
	int type = RNA_enum_get(op->ptr, "type");

	ob = edit_object_property_get(C, op);
	if (!ob)
		return OPERATOR_CANCELLED;

	sens = new_sensor(type);
	BLI_addtail(&(ob->sensors), sens);
	
	/* set the sensor name based on rna type enum */
	RNA_pointer_create((ID *)ob, &RNA_Sensor, sens, &sens_ptr);
	prop = RNA_struct_find_property(&sens_ptr, "type");

	RNA_string_get(op->ptr, "name", name);
	if (*name) {
		BLI_strncpy(sens->name, name, sizeof(sens->name));
	}
	else {
		RNA_property_enum_name(C, &sens_ptr, prop, RNA_property_enum_get(&sens_ptr, prop), &sens_name);
		BLI_strncpy(sens->name, sens_name, sizeof(sens->name));
	}

	make_unique_prop_names(C, sens->name);
	ob->scaflag |= OB_SHOWSENS;

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static void LOGIC_OT_sensor_add(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Add Sensor";
	ot->description = "Add a sensor to the active object";
	ot->idname = "LOGIC_OT_sensor_add";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = sensor_add_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	ot->prop = prop = RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, SENS_ALWAYS, "Type", "Type of sensor to add");
	RNA_def_enum_funcs(prop, rna_Sensor_type_itemf);
	prop = RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the Sensor to add");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the Object to add the Sensor to");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************* Add/Remove Controller Operator ************* */

static int controller_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bController *cont = edit_controller_property_get(C, op, &ob);
	
	if (!cont)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->controllers), cont);
	unlink_controller(cont);
	free_controller(cont);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int controller_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_controller_invoke_properties(C, op))
		return controller_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_controller_remove(wmOperatorType *ot)
{
	ot->name = "Remove Controller";
	ot->description = "Remove a controller from the active object";
	ot->idname = "LOGIC_OT_controller_remove";
	
	ot->invoke = controller_remove_invoke;
	ot->exec = controller_remove_exec;
	ot->poll = edit_controller_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_controller_properties(ot);
}

static int controller_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	bController *cont;
	PointerRNA cont_ptr;
	PropertyRNA *prop;
	const char *cont_name;
	int bit;
	char name[MAX_NAME];
	int type = RNA_enum_get(op->ptr, "type");

	ob = edit_object_property_get(C, op);
	if (!ob)
		return OPERATOR_CANCELLED;
	
	cont = new_controller(type);
	BLI_addtail(&(ob->controllers), cont);
	
	/* set the controller name based on rna type enum */
	RNA_pointer_create((ID *)ob, &RNA_Controller, cont, &cont_ptr);
	prop = RNA_struct_find_property(&cont_ptr, "type");

	RNA_string_get(op->ptr, "name", name);
	if (*name) {
		BLI_strncpy(cont->name, name, sizeof(cont->name));
	}
	else {
		RNA_property_enum_name(C, &cont_ptr, prop, RNA_property_enum_get(&cont_ptr, prop), &cont_name);
		BLI_strncpy(cont->name, cont_name, sizeof(cont->name));
	}

	make_unique_prop_names(C, cont->name);
	/* set the controller state mask from the current object state.
	 * A controller is always in a single state, so select the lowest bit set
	 * from the object state */
	for (bit = 0; bit < OB_MAX_STATES; bit++) {
		if (ob->state & (1 << bit))
			break;
	}
	cont->state_mask = (1 << bit);
	if (cont->state_mask == 0) {
		/* shouldn't happen, object state is never 0 */
		cont->state_mask = 1;
	}
	
	ob->scaflag |= OB_SHOWCONT;
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static void LOGIC_OT_controller_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Controller";
	ot->description = "Add a controller to the active object";
	ot->idname = "LOGIC_OT_controller_add";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = controller_add_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", controller_type_items, CONT_LOGIC_AND, "Type", "Type of controller to add");
	prop = RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the Controller to add");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the Object to add the Controller to");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************* Add/Remove Actuator Operator ************* */

static int actuator_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bActuator *act = edit_actuator_property_get(C, op, &ob);
	
	if (!act)
		return OPERATOR_CANCELLED;
	
	BLI_remlink(&(ob->actuators), act);
	unlink_actuator(act);
	free_actuator(act);
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int actuator_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_actuator_invoke_properties(C, op))
		return actuator_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_actuator_remove(wmOperatorType *ot)
{
	ot->name = "Remove Actuator";
	ot->description = "Remove an actuator from the active object";
	ot->idname = "LOGIC_OT_actuator_remove";
	
	ot->invoke = actuator_remove_invoke;
	ot->exec = actuator_remove_exec;
	ot->poll = edit_actuator_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_actuator_properties(ot);
}

static int actuator_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	bActuator *act;
	PointerRNA act_ptr;
	PropertyRNA *prop;
	const char *act_name;
	char name[MAX_NAME];
	int type = RNA_enum_get(op->ptr, "type");
		
	ob = edit_object_property_get(C, op);
	if (!ob)
		return OPERATOR_CANCELLED;

	act = new_actuator(type);
	BLI_addtail(&(ob->actuators), act);
	
	/* set the actuator name based on rna type enum */
	RNA_pointer_create((ID *)ob, &RNA_Actuator, act, &act_ptr);
	prop = RNA_struct_find_property(&act_ptr, "type");

	RNA_string_get(op->ptr, "name", name);
	if (*name) {
		BLI_strncpy(act->name, name, sizeof(act->name));
	}
	else {
		RNA_property_enum_name(C, &act_ptr, prop, RNA_property_enum_get(&act_ptr, prop), &act_name);
		BLI_strncpy(act->name, act_name, sizeof(act->name));
	}

	make_unique_prop_names(C, act->name);
	ob->scaflag |= OB_SHOWACT;
	
	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static void LOGIC_OT_actuator_add(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Add Actuator";
	ot->description = "Add an actuator to the active object";
	ot->idname = "LOGIC_OT_actuator_add";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actuator_add_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	ot->prop = prop = RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, CONT_LOGIC_AND, "Type", "Type of actuator to add");
	RNA_def_enum_funcs(prop, rna_Actuator_type_itemf);
	prop = RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the Actuator to add");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "object", NULL, MAX_NAME, "Object", "Name of the Object to add the Actuator to");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ************* Move Logic Bricks Operator ************* */
static EnumPropertyItem logicbricks_move_direction[] = {
	{1, "UP", 0, "Move Up", ""},
	{2, "DOWN", 0, "Move Down", ""},
	{0, NULL, 0, NULL, NULL}
};


static int sensor_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bSensor *sens = edit_sensor_property_get(C, op, &ob);
	int move_up = logicbricks_move_property_get(op);
	
	if (!sens)
		return OPERATOR_CANCELLED;

	sca_move_sensor(sens, ob, move_up);

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int sensor_move_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_sensor_invoke_properties(C, op)) {
		return sensor_move_exec(C, op);
	}
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_sensor_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Sensor";
	ot->description = "Move Sensor";
	ot->idname = "LOGIC_OT_sensor_move";
	
	/* api callbacks */
	ot->invoke = sensor_move_invoke;
	ot->exec = sensor_move_exec;
	ot->poll = edit_sensor_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	edit_sensor_properties(ot);
	RNA_def_enum(ot->srna, "direction", logicbricks_move_direction, 1, "Direction", "Move Up or Down");
}

static int controller_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bController *cont = edit_controller_property_get(C, op, &ob);
	int move_up = logicbricks_move_property_get(op);
	
	if (!cont)
		return OPERATOR_CANCELLED;

	sca_move_controller(cont, ob, move_up);

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int controller_move_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_controller_invoke_properties(C, op)) {
		return controller_move_exec(C, op);
	}
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_controller_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Controller";
	ot->description = "Move Controller";
	ot->idname = "LOGIC_OT_controller_move";
	
	/* api callbacks */
	ot->invoke = controller_move_invoke;
	ot->exec = controller_move_exec;
	ot->poll = edit_controller_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	edit_controller_properties(ot);
	RNA_def_enum(ot->srna, "direction", logicbricks_move_direction, 1, "Direction", "Move Up or Down");
}

static int actuator_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = NULL;
	bActuator *act = edit_actuator_property_get(C, op, &ob);
	int move_up = logicbricks_move_property_get(op);

	if (!act)
		return OPERATOR_CANCELLED;

	sca_move_actuator(act, ob, move_up);

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	
	return OPERATOR_FINISHED;
}

static int actuator_move_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_actuator_invoke_properties(C, op)) {
		return actuator_move_exec(C, op);
	}
	else
		return OPERATOR_CANCELLED;
}

static void LOGIC_OT_actuator_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Actuator";
	ot->description = "Move Actuator";
	ot->idname = "LOGIC_OT_actuator_move";
	
	/* api callbacks */
	ot->invoke = actuator_move_invoke;
	ot->exec = actuator_move_exec;
	ot->poll = edit_actuator_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	/* properties */
	edit_actuator_properties(ot);
	RNA_def_enum(ot->srna, "direction", logicbricks_move_direction, 1, "Direction", "Move Up or Down");
}

/* ************************ view ********************* */

static int logic_view_all_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	rctf cur_new = ar->v2d.tot;
	float aspect = BLI_rctf_size_y(&ar->v2d.cur) / BLI_rctf_size_x(&ar->v2d.cur);
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
	
	/* force the view2d code to zoom to width, not height */
	cur_new.ymin = cur_new.ymax - BLI_rctf_size_x(&cur_new) * aspect;
	
	UI_view2d_smooth_view(C, ar, &cur_new, smooth_viewtx);

	return OPERATOR_FINISHED;
}

static void LOGIC_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "LOGIC_OT_view_all";
	ot->description = "Resize view so you can see all logic bricks";
	
	/* api callbacks */
	ot->exec = logic_view_all_exec;
	ot->poll = ED_operator_logic_active;
	
	/* flags */
	ot->flag = 0;
}

/* ************************* */

void ED_operatortypes_logic(void)
{
	WM_operatortype_append(LOGIC_OT_sensor_remove);
	WM_operatortype_append(LOGIC_OT_sensor_add);
	WM_operatortype_append(LOGIC_OT_sensor_move);
	WM_operatortype_append(LOGIC_OT_controller_remove);
	WM_operatortype_append(LOGIC_OT_controller_add);
	WM_operatortype_append(LOGIC_OT_controller_move);
	WM_operatortype_append(LOGIC_OT_actuator_remove);
	WM_operatortype_append(LOGIC_OT_actuator_add);
	WM_operatortype_append(LOGIC_OT_actuator_move);
	WM_operatortype_append(LOGIC_OT_view_all);
}
