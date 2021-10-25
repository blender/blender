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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_sensor.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_sensor_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_types.h"

/* Always keep in alphabetical order */
static EnumPropertyItem sensor_type_items[] = {
	{SENS_ACTUATOR, "ACTUATOR", 0, "Actuator", ""},
	{SENS_ALWAYS, "ALWAYS", 0, "Always", ""},
	{SENS_ARMATURE, "ARMATURE", 0, "Armature", ""},
	{SENS_COLLISION, "COLLISION", 0, "Collision", ""},
	{SENS_DELAY, "DELAY", 0, "Delay", ""},
	{SENS_JOYSTICK, "JOYSTICK", 0, "Joystick", ""},
	{SENS_KEYBOARD, "KEYBOARD", 0, "Keyboard", ""},
	{SENS_MESSAGE, "MESSAGE", 0, "Message", ""},
	{SENS_MOUSE, "MOUSE", 0, "Mouse", ""},
	{SENS_NEAR, "NEAR", 0, "Near", ""},
	{SENS_PROPERTY, "PROPERTY", 0, "Property", ""},
	{SENS_RADAR, "RADAR", 0, "Radar", ""},
	{SENS_RANDOM, "RANDOM", 0, "Random", ""},
	{SENS_RAY, "RAY", 0, "Ray", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "BKE_sca.h"
#include "DNA_controller_types.h"

static StructRNA *rna_Sensor_refine(struct PointerRNA *ptr)
{
	bSensor *sensor = (bSensor *)ptr->data;

	switch (sensor->type) {
		case SENS_ALWAYS:
			return &RNA_AlwaysSensor;
		case SENS_NEAR:
			return &RNA_NearSensor;
		case SENS_KEYBOARD:
			return &RNA_KeyboardSensor;
		case SENS_PROPERTY:
			return &RNA_PropertySensor;
		case SENS_ARMATURE:
			return &RNA_ArmatureSensor;
		case SENS_MOUSE:
			return &RNA_MouseSensor;
		case SENS_COLLISION:
			return &RNA_CollisionSensor;
		case SENS_RADAR:
			return &RNA_RadarSensor;
		case SENS_RANDOM:
			return &RNA_RandomSensor;
		case SENS_RAY:
			return &RNA_RaySensor;
		case SENS_MESSAGE:
			return &RNA_MessageSensor;
		case SENS_JOYSTICK:
			return &RNA_JoystickSensor;
		case SENS_ACTUATOR:
			return &RNA_ActuatorSensor;
		case SENS_DELAY:
			return &RNA_DelaySensor;
		default:
			return &RNA_Sensor;
	}
}

static void rna_Sensor_name_set(PointerRNA *ptr, const char *value)
{
	Object *ob = ptr->id.data;
	bSensor *sens = ptr->data;
	BLI_strncpy_utf8(sens->name, value, sizeof(sens->name));
	BLI_uniquename(&ob->sensors, sens, DATA_("Sensor"), '.', offsetof(bSensor, name), sizeof(sens->name));
}

static void rna_Sensor_type_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;
	if (value != sens->type) {
		sens->type = value;
		init_sensor(sens);
	}
}

/* Always keep in alphabetical order */

static void rna_Sensor_controllers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	bSensor *sens = (bSensor *)ptr->data;
	rna_iterator_array_begin(iter, sens->links, sizeof(bController *), (int)sens->totlinks, 0, NULL);
}

static int rna_Sensor_controllers_length(PointerRNA *ptr)
{
	bSensor *sens = (bSensor *)ptr->data;
	return (int) sens->totlinks;
}

EnumPropertyItem *rna_Sensor_type_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	Object *ob = NULL;
	int totitem = 0;

	if (ptr->type == &RNA_Sensor || RNA_struct_is_a(ptr->type, &RNA_Sensor)) {
		ob = (Object *)ptr->id.data;
	}
	else {
		/* can't use ob from ptr->id.data because that enum is also used by operators */
		ob = CTX_data_active_object(C);
	}
	
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_ACTUATOR);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_ALWAYS);

	if (ob != NULL) {
		if (ob->type == OB_ARMATURE) {
			RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_ARMATURE);
		}
	}
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_COLLISION);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_DELAY);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_JOYSTICK);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_KEYBOARD);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_MESSAGE);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_MOUSE);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_NEAR);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_PROPERTY);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_RADAR);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_RANDOM);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_RAY);
	RNA_enum_items_add_value(&item, &totitem, sensor_type_items, SENS_TOUCH);
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

static void rna_Sensor_keyboard_key_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;
	bKeyboardSensor *ks = (bKeyboardSensor *)sens->data;
	
	if (ISKEYBOARD(value))
		ks->key = value;
	else
		ks->key = 0;
}

static void rna_Sensor_keyboard_modifier_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;
	bKeyboardSensor *ks = (bKeyboardSensor *)sens->data;
	
	if (ISKEYBOARD(value))
		ks->qual = value;
	else
		ks->qual = 0;
}
		
static void rna_Sensor_keyboard_modifier2_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;
	bKeyboardSensor *ks = (bKeyboardSensor *)sens->data;
	
	if (ISKEYBOARD(value))
		ks->qual2 = value;
	else
		ks->qual2 = 0;
}

static void rna_Sensor_tap_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;

	sens->tap = value;
	if (sens->tap == 1)
		sens->level = 0;
}

static void rna_Sensor_level_set(struct PointerRNA *ptr, int value)
{
	bSensor *sens = (bSensor *)ptr->data;

	sens->level = value;
	if (sens->level == 1)
		sens->tap = 0;
}

static void rna_Sensor_Armature_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	bSensor *sens = (bSensor *)ptr->data;
	bArmatureSensor *as = sens->data;
	Object *ob = (Object *)ptr->id.data;

	char *posechannel = as->posechannel;
	char *constraint = as->constraint;

	/* check that bone exist in the active object */
	if (ob->type == OB_ARMATURE && ob->pose) {
		bPoseChannel *pchan;
		bPose *pose = ob->pose;
		for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
			if (STREQ(pchan->name, posechannel)) {
				/* found it, now look for constraint channel */
				bConstraint *con;
				for (con = pchan->constraints.first; con; con = con->next) {
					if (STREQ(con->name, constraint)) {
						/* found it, all ok */
						return;
					}
				}
				/* didn't find constraint, make empty */
				constraint[0] = 0;
				return;
			}
		}
	}
	/* didn't find any */
	posechannel[0] = 0;
	constraint[0] = 0;
}
#else

static void rna_def_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Sensor", NULL);
	RNA_def_struct_ui_text(srna, "Sensor", "Game engine logic brick to detect events");
	RNA_def_struct_sdna(srna, "bSensor");
	RNA_def_struct_refine_func(srna, "rna_Sensor_refine");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Sensor name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Sensor_name_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, sensor_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Sensor_type_set", "rna_Sensor_type_itemf");
	RNA_def_property_ui_text(prop, "Type", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "pin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_PIN);
	RNA_def_property_ui_text(prop, "Pinned", "Display when not linked to a visible states controller");
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SENS_DEACTIVATE);
	RNA_def_property_ui_text(prop, "Active", "Set active state of the sensor");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_SHOW);
	RNA_def_property_ui_text(prop, "Expanded", "Set sensor expanded in the user interface");
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Invert Output", "Invert the level(output) of this sensor");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "level", 1);
	RNA_def_property_ui_text(prop, "Level",
	                         "Level detector, trigger controllers of new states "
	                         "(only applicable upon logic state transition)");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sensor_level_set");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_pulse_true_level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pulse", SENS_PULSE_REPEAT);
	RNA_def_property_ui_text(prop, "Pulse True Level", "Activate TRUE level triggering (pulse mode)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_pulse_false_level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pulse", SENS_NEG_PULSE_MODE);
	RNA_def_property_ui_text(prop, "Pulse False Level", "Activate FALSE level triggering (pulse mode)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "tick_skip", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "freq");
	RNA_def_property_ui_text(prop, "Skip",
	                         "Number of logic ticks skipped between 2 active pulses "
	                         "(0 = pulse every logic tick, 1 = skip 1 logic tick between pulses, etc.)");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_tap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "tap", 1);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Sensor_tap_set");
	RNA_def_property_ui_text(prop, "Tap",
	                         "Trigger controllers only for an instant, even while the sensor remains true");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "controllers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "links", NULL);
	RNA_def_property_struct_type(prop, "Controller");
	RNA_def_property_ui_text(prop, "Controllers", "The list containing the controllers connected to the sensor");
	RNA_def_property_collection_funcs(prop, "rna_Sensor_controllers_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_dereference_get",
	                                  "rna_Sensor_controllers_length", NULL, NULL, NULL);


	RNA_api_sensor(srna);
}

static void rna_def_always_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	srna = RNA_def_struct(brna, "AlwaysSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Always Sensor", "Sensor to generate continuous pulses");
}

static void rna_def_near_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "NearSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Near Sensor", "Sensor to detect nearby objects");
	RNA_def_struct_sdna_from(srna, "bNearSensor", "data");

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_ui_text(prop, "Distance", "Trigger distance");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "reset_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "resetdist");
	RNA_def_property_ui_text(prop, "Reset Distance", "The distance where the sensor forgets the actor");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_mouse_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem mouse_event_items[] = {
		{BL_SENS_MOUSE_LEFT_BUTTON, "LEFTCLICK", 0, "Left Button", ""},
		{BL_SENS_MOUSE_MIDDLE_BUTTON, "MIDDLECLICK", 0, "Middle Button", ""},
		{BL_SENS_MOUSE_RIGHT_BUTTON, "RIGHTCLICK", 0, "Right Button", ""},
		{BL_SENS_MOUSE_WHEEL_UP, "WHEELUP", 0, "Wheel Up", ""},
		{BL_SENS_MOUSE_WHEEL_DOWN, "WHEELDOWN", 0, "Wheel Down", ""},
		{BL_SENS_MOUSE_MOVEMENT, "MOVEMENT", 0, "Movement", ""},
		{BL_SENS_MOUSE_MOUSEOVER, "MOUSEOVER", 0, "Mouse Over", ""},
		{BL_SENS_MOUSE_MOUSEOVER_ANY, "MOUSEOVERANY", 0, "Mouse Over Any", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem prop_mouse_type_items[] = {
		{SENS_COLLISION_PROPERTY, "PROPERTY", ICON_LOGIC, "Property", "Use a property for ray intersections"},
		{SENS_COLLISION_MATERIAL, "MATERIAL", ICON_MATERIAL_DATA, "Material", "Use a material for ray intersections"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "MouseSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Mouse Sensor", "Sensor to detect mouse events");
	RNA_def_struct_sdna_from(srna, "bMouseSensor", "data");

	prop = RNA_def_property(srna, "mouse_event", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, mouse_event_items);
	RNA_def_property_ui_text(prop, "Mouse Event", "Type of event this mouse sensor should trigger on");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_pulse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_MOUSE_FOCUS_PULSE);
	RNA_def_property_ui_text(prop, "Pulse", "Moving the mouse over a different object generates a pulse");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "use_material", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_mouse_type_items);
	RNA_def_property_ui_text(prop, "M/P", "Toggle collision on material or property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "propname");
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "material", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "matname");
	RNA_def_property_ui_text(prop, "Material", "Only look for objects with this material (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);	

	prop = RNA_def_property(srna, "use_x_ray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_RAY_XRAY);
	RNA_def_property_ui_text(prop, "X-Ray", "Toggle X-Ray option (see through objects that don't have the property)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_keyboard_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "KeyboardSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Keyboard Sensor", "Sensor to detect keyboard events");
	RNA_def_struct_sdna_from(srna, "bKeyboardSensor", "data");

	prop = RNA_def_property(srna, "key", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "key");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Sensor_keyboard_key_set", NULL);
	RNA_def_property_ui_text(prop, "Key",  "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_WINDOWMANAGER);
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "modifier_key_1", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "qual");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Sensor_keyboard_modifier_set", NULL);
	RNA_def_property_ui_text(prop, "Modifier Key", "Modifier key code");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop = RNA_def_property(srna, "modifier_key_2", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "qual2");
	RNA_def_property_enum_items(prop, rna_enum_event_type_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Sensor_keyboard_modifier2_set", NULL);
	RNA_def_property_ui_text(prop, "Second Modifier Key", "Modifier key code");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "target", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "targetName");
	RNA_def_property_ui_text(prop, "Target", "Property that receives the keystrokes in case a string is logged");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "log", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "toggleName");
	RNA_def_property_ui_text(prop, "Log Toggle", "Property that indicates whether to log keystrokes as a string");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_all_keys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type", 1);
	RNA_def_property_ui_text(prop, "All Keys", "Trigger this sensor on any keystroke");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_property_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{SENS_PROP_EQUAL, "PROPEQUAL", 0, "Equal", ""},
		{SENS_PROP_NEQUAL, "PROPNEQUAL", 0, "Not Equal", ""},
		{SENS_PROP_INTERVAL, "PROPINTERVAL", 0, "Interval", ""},
		{SENS_PROP_CHANGED, "PROPCHANGED", 0, "Changed", ""},
		/* {SENS_PROP_EXPRESSION, "PROPEXPRESSION", 0, "Expression", ""},  NOT_USED_IN_UI */
		{SENS_PROP_LESSTHAN, "PROPLESSTHAN", 0, "Less Than", ""},
		{SENS_PROP_GREATERTHAN, "PROPGREATERTHAN", 0, "Greater Than", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "PropertySensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Property Sensor", "Sensor to detect values and changes in values of properties");
	RNA_def_struct_sdna_from(srna, "bPropertySensor", "data");

	prop = RNA_def_property(srna, "evaluation_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Evaluation Type", "Type of property evaluation");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Value", "Check for this value in types in Equal, Not Equal, Less Than and Greater Than types");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "value_min", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Minimum Value", "Minimum value in Interval type");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "value_max", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "maxvalue");
	RNA_def_property_ui_text(prop, "Maximum Value", "Maximum value in Interval type");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_armature_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{SENS_ARM_STATE_CHANGED, "STATECHG", 0, "State Changed", ""},
		{SENS_ARM_LIN_ERROR_BELOW, "LINERRORBELOW", 0, "Lin error below", ""},
		{SENS_ARM_LIN_ERROR_ABOVE, "LINERRORABOVE", 0, "Lin error above", ""},
		{SENS_ARM_ROT_ERROR_BELOW, "ROTERRORBELOW", 0, "Rot error below", ""},
		{SENS_ARM_ROT_ERROR_ABOVE, "ROTERRORABOVE", 0, "Rot error above", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "ArmatureSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Armature Sensor", "Sensor to detect values and changes in values of IK solver");
	RNA_def_struct_sdna_from(srna, "bArmatureSensor", "data");

	prop = RNA_def_property(srna, "test_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Test", "Type of value and test");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "bone", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "posechannel");
	RNA_def_property_ui_text(prop, "Bone Name", "Identify the bone to check value from");
	RNA_def_property_update(prop, NC_LOGIC, "rna_Sensor_Armature_update");

	prop = RNA_def_property(srna, "constraint", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "constraint");
	RNA_def_property_ui_text(prop, "Constraint Name", "Identify the bone constraint to check value from");
	RNA_def_property_update(prop, NC_LOGIC, "rna_Sensor_Armature_update");

	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Compare Value", "Value to be used in comparison");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_actuator_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ActuatorSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Actuator Sensor", "Sensor to detect state modifications of actuators");
	RNA_def_struct_sdna_from(srna, "bActuatorSensor", "data");

	/* XXX if eventually have Logics using RNA 100%, we could use the actuator data-block isntead of its name */
	prop = RNA_def_property(srna, "actuator", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Actuator", "Actuator name, actuator active state modifications will be detected");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_delay_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "DelaySensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Delay Sensor", "Sensor to send delayed events");
	RNA_def_struct_sdna_from(srna, "bDelaySensor", "data");

	prop = RNA_def_property(srna, "delay", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Delay",
	                         "Delay in number of logic tics before the positive trigger (default 60 per second)");
	RNA_def_property_range(prop, 0, 5000);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "duration", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Duration",
	                         "If >0, delay in number of logic tics before the negative trigger following "
	                         "the positive trigger");
	RNA_def_property_range(prop, 0, 5000);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_repeat", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_DELAY_REPEAT);
	RNA_def_property_ui_text(prop, "Repeat",
	                         "Toggle repeat option (if selected, the sensor restarts after Delay+Duration "
	                         "logic tics)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_collision_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "CollisionSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Collision Sensor",
	                       "Sensor to detect objects colliding with the current object, with more settings than "
	                       "the Touch sensor");
	RNA_def_struct_sdna_from(srna, "bCollisionSensor", "data");

	prop = RNA_def_property(srna, "use_pulse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_COLLISION_PULSE);
	RNA_def_property_ui_text(prop, "Pulse", "Change to the set of colliding objects generates pulse");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_material", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_COLLISION_MATERIAL);
	RNA_def_property_ui_text(prop, "M/P", "Toggle collision on material or property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/*XXX to make a setFunction to create a lookup with all materials in Blend File (not only this object mat.) */
	prop = RNA_def_property(srna, "material", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "materialName");
	RNA_def_property_ui_text(prop, "Material", "Only look for objects with this material (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

#if 0
	/* XXX either use a data-block look up to store the string name (material)
	 * or to do a doversion and use a material pointer. */
	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "ma");
	RNA_def_property_ui_text(prop, "Material", "Only look for objects with this material (blank = all objects)");
#endif
}

static void rna_def_radar_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem axis_items[] = {
		{SENS_RADAR_X_AXIS, "XAXIS", 0, "+X axis", ""},
		{SENS_RADAR_Y_AXIS, "YAXIS", 0, "+Y axis", ""},
		{SENS_RADAR_Z_AXIS, "ZAXIS", 0, "+Z axis", ""},
		{SENS_RADAR_NEG_X_AXIS, "NEGXAXIS", 0, "-X axis", ""},
		{SENS_RADAR_NEG_Y_AXIS, "NEGYAXIS", 0, "-Y axis", ""},
		{SENS_RADAR_NEG_Z_AXIS, "NEGZAXIS", 0, "-Z axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "RadarSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Radar Sensor",
	                       "Sensor to detect objects in a cone shaped radar emanating from the current object");
	RNA_def_struct_sdna_from(srna, "bRadarSensor", "data");

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Along which axis the radar cone is cast");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_range(prop, 0.0, DEG2RADF(179.9f));
	RNA_def_property_ui_text(prop, "Angle", "Opening angle of the radar cone");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "range");
	RNA_def_property_range(prop, 0.0, 10000.0);
	RNA_def_property_ui_text(prop, "Distance", "Depth of the radar cone");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_random_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "RandomSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Random Sensor", "Sensor to send random events");
	RNA_def_struct_sdna_from(srna, "bRandomSensor", "data");

	prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "Seed", "Initial seed of the generator (choose 0 for not random)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_ray_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem axis_items[] = {
		{SENS_RAY_X_AXIS, "XAXIS", 0, "+X axis", ""},
		{SENS_RAY_Y_AXIS, "YAXIS", 0, "+Y axis", ""},
		{SENS_RAY_Z_AXIS, "ZAXIS", 0, "+Z axis", ""},
		{SENS_RAY_NEG_X_AXIS, "NEGXAXIS", 0, "-X axis", ""},
		{SENS_RAY_NEG_Y_AXIS, "NEGYAXIS", 0, "-Y axis", ""},
		{SENS_RAY_NEG_Z_AXIS, "NEGZAXIS", 0, "-Z axis", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	static const EnumPropertyItem prop_ray_type_items[] = {
		{SENS_COLLISION_PROPERTY, "PROPERTY", ICON_LOGIC, "Property", "Use a property for ray intersections"},
		{SENS_COLLISION_MATERIAL, "MATERIAL", ICON_MATERIAL_DATA, "Material", "Use a material for ray intersections"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "RaySensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Ray Sensor",
	                       "Sensor to detect intersections with a ray emanating from the current object");
	RNA_def_struct_sdna_from(srna, "bRaySensor", "data");
	
	prop = RNA_def_property(srna, "ray_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_ray_type_items);
	RNA_def_property_ui_text(prop, "Ray Type", "Toggle collision on material or property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "propname");
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "material", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "matname");
	RNA_def_property_ui_text(prop, "Material", "Only look for objects with this material (blank = all objects)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

#if 0
	/* XXX either use a data-block look up to store the string name (material)
	 * or to do a doversion and use a material pointer. */
	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "ma");
	RNA_def_property_ui_text(prop, "Material", "Only look for objects with this material (blank = all objects)");
#endif

	prop = RNA_def_property(srna, "use_x_ray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_RAY_XRAY);
	RNA_def_property_ui_text(prop, "X-Ray Mode",
	                         "Toggle X-Ray option (see through objects that don't have the property)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.01, 10000.0);
	RNA_def_property_ui_text(prop, "Range", "Sense objects no farther than this distance");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axisflag");
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Along which axis the ray is cast");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_message_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MessageSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Message Sensor", "Sensor to detect incoming messages");
	RNA_def_struct_sdna_from(srna, "bMessageSensor", "data");

	prop = RNA_def_property(srna, "subject", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Subject",
	                         "Optional subject filter: only accept messages with this subject, "
	                         "or empty to accept all");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_joystick_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem event_type_joystick_items[] = {
		{SENS_JOY_BUTTON, "BUTTON", 0, "Button", ""},
		{SENS_JOY_AXIS, "AXIS", 0, "Axis", ""},
		{SENS_JOY_HAT, "HAT", 0, "Hat", ""},
		{SENS_JOY_AXIS_SINGLE, "AXIS_SINGLE", 0, "Single Axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem axis_direction_items[] = {
		{SENS_JOY_X_AXIS, "RIGHTAXIS", 0, "Right Axis", ""},
		{SENS_JOY_Y_AXIS, "UPAXIS", 0, "Up Axis", ""},
		{SENS_JOY_NEG_X_AXIS, "LEFTAXIS", 0, "Left Axis", ""},
		{SENS_JOY_NEG_Y_AXIS, "DOWNAXIS", 0, "Down Axis", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem hat_direction_items[] = {
		{SENS_JOY_HAT_UP, "UP", 0, "Up", ""},
		{SENS_JOY_HAT_DOWN, "DOWN", 0, "Down", ""},
		{SENS_JOY_HAT_LEFT, "LEFT", 0, "Left", ""},
		{SENS_JOY_HAT_RIGHT, "RIGHT", 0, "Right", ""},

		{SENS_JOY_HAT_UP_RIGHT, "UPRIGHT", 0, "Up/Right", ""},
		{SENS_JOY_HAT_DOWN_LEFT, "DOWNLEFT", 0, "Down/Left", ""},
		{SENS_JOY_HAT_UP_LEFT, "UPLEFT", 0, "Up/Left", ""},
		{SENS_JOY_HAT_DOWN_RIGHT, "DOWNRIGHT", 0, "Down/Right", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "JoystickSensor", "Sensor");
	RNA_def_struct_ui_text(srna, "Joystick Sensor", "Sensor to detect joystick events");
	RNA_def_struct_sdna_from(srna, "bJoystickSensor", "data");
	
	prop = RNA_def_property(srna, "joystick_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "joyindex");
	RNA_def_property_ui_text(prop, "Index", "Which joystick to use");
	RNA_def_property_range(prop, 0, SENS_JOY_MAXINDEX - 1);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "event_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, event_type_joystick_items);
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UI_EVENTS);
	RNA_def_property_ui_text(prop, "Event Type", "The type of event this joystick sensor is triggered on");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "use_all_events", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_JOY_ANY_EVENT);
	RNA_def_property_ui_text(prop, "All Events",
	                         "Triggered by all events on this joystick's current type (axis/button/hat)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Button */
	prop = RNA_def_property(srna, "button_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "button");
	RNA_def_property_ui_text(prop, "Button Number", "Which button to use");
	RNA_def_property_range(prop, 0, 18);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Axis */
	prop = RNA_def_property(srna, "axis_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "axis");
	RNA_def_property_ui_text(prop, "Axis Number", "Which axis pair to use, 1 is usually the main direction input");
	RNA_def_property_range(prop, 1, 8);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "axis_threshold", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "precision");
	RNA_def_property_ui_text(prop, "Axis Threshold", "Precision of the axis");
	RNA_def_property_range(prop, 0, 32768);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "axis_direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axisf");
	RNA_def_property_enum_items(prop, axis_direction_items);
	RNA_def_property_ui_text(prop, "Axis Direction", "The direction of the axis");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Single Axis */
	prop = RNA_def_property(srna, "single_axis_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "axis_single");
	RNA_def_property_ui_text(prop, "Axis Number", "Single axis (vertical/horizontal/other) to detect");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* Hat */
	prop = RNA_def_property(srna, "hat_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hat");
	RNA_def_property_ui_text(prop, "Hat Number", "Which hat to use");
	RNA_def_property_range(prop, 1, 2);
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop = RNA_def_property(srna, "hat_direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "hatf");
	RNA_def_property_enum_items(prop, hat_direction_items);
	RNA_def_property_ui_text(prop, "Hat Direction", "Hat direction");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

void RNA_def_sensor(BlenderRNA *brna)
{
	rna_def_sensor(brna);

	rna_def_always_sensor(brna);
	rna_def_near_sensor(brna);
	rna_def_mouse_sensor(brna);
	rna_def_keyboard_sensor(brna);
	rna_def_property_sensor(brna);
	rna_def_armature_sensor(brna);
	rna_def_actuator_sensor(brna);
	rna_def_delay_sensor(brna);
	rna_def_collision_sensor(brna);
	rna_def_radar_sensor(brna);
	rna_def_random_sensor(brna);
	rna_def_ray_sensor(brna);
	rna_def_message_sensor(brna);
	rna_def_joystick_sensor(brna);
}

#endif
