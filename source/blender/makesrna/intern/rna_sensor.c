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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_sensor_types.h"

#ifdef RNA_RUNTIME

static struct StructRNA* rna_Sensor_data_type(struct PointerRNA *ptr)
{
	bSensor *sensor= (bSensor*)ptr->data;

	switch(sensor->type) {
		case SENS_ALWAYS:
			return &RNA_UnknownType;
		case SENS_TOUCH:
			return &RNA_TouchSensor;
		case SENS_NEAR:
			return &RNA_NearSensor;
		case SENS_KEYBOARD:
			return &RNA_KeyboardSensor;
		case SENS_PROPERTY:
			return &RNA_PropertySensor;
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
			return &RNA_UnknownType;
	}

	return &RNA_UnknownType;
}

#else

void rna_def_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem sensor_types_items[] ={
		{SENS_ALWAYS, "ALWAYS", "Always", ""},
		{SENS_TOUCH, "TOUCH", "Touch", ""},
		{SENS_NEAR, "NEAR", "Near", ""},
		{SENS_KEYBOARD, "KEYBOARD", "Keyboard", ""},
		{SENS_PROPERTY, "PROPERTY", "Property", ""},
		{SENS_MOUSE, "MOUSE", "Mouse", ""},
		{SENS_COLLISION, "COLLISION", "Collision", ""},
		{SENS_RADAR, "RADAR", "Radar", ""},
		{SENS_RANDOM, "RANDOM", "Random", ""},
		{SENS_RAY, "RAY", "Ray", ""},
		{SENS_MESSAGE, "MESSAGE", "Message", ""},
		{SENS_JOYSTICK, "JOYSTICK", "joystick", ""},
		{SENS_ACTUATOR, "ACTUATOR", "Actuator", ""},
		{SENS_DELAY, "DELAY", "Delay", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Sensor", NULL , "Sensor");
	RNA_def_struct_sdna(srna, "bSensor");

	prop= RNA_def_property(srna, "sensor_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Name", "Sensor name.");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, sensor_types_items);
	RNA_def_property_ui_text(prop, "Sensor Types", "Sensor types.");

	prop= RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Invert Output", "Invert the level(output) of this sensor.");

	prop= RNA_def_property(srna, "level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Level", "Level detector, trigger controllers of new states(only applicable upon logic state transition).");

	prop= RNA_def_property(srna, "pulse_true_level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pulse", SENS_PULSE_REPEAT);
	RNA_def_property_ui_text(prop, "Pulse True Level", "Activate TRUE level triggering (pulse mode).");

	prop= RNA_def_property(srna, "pulse_false_level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pulse", SENS_NEG_PULSE_MODE);
	RNA_def_property_ui_text(prop, "Pulse False Level", "Activate FALSE level triggering (pulse mode).");
	

	prop= RNA_def_property(srna, "freq", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Frequency", "Delay between repeated pulses(in logic tics, 0=no delay).");
	RNA_def_property_range(prop, 0, 10000);

	
	//This add data property to Sensor, and because data can be bMouseSensor, bNearSensor, bAlwaysSensor ...
	//rna_Sensor_data_type defines above in runtime section to get its type and proper structure for data
	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Data", "Sensor data.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Sensor_data_type", NULL);
}

void rna_def_near_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "NearSensor", NULL , "NearSensor");
	RNA_def_struct_sdna(srna, "bNearSensor");

	prop= RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Property", "Only look for objects with this property.");

	prop= RNA_def_property(srna, "distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dist");
	RNA_def_property_ui_text(prop, "Distance", "Trigger distance.");
	RNA_def_property_range(prop, 0, 10000);

	prop= RNA_def_property(srna, "reset_distance", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resetdist");
	RNA_def_property_ui_text(prop, "Reset", "Reset distance.");
	RNA_def_property_range(prop, 0, 10000);
}

void rna_def_mouse_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem mouse_event_items[] ={
		{BL_SENS_MOUSE_LEFT_BUTTON, "LEFTCLICK", "Left Button", ""},
		{BL_SENS_MOUSE_MIDDLE_BUTTON, "MIDDLECLICK", "Middle Button", ""},
		{BL_SENS_MOUSE_RIGHT_BUTTON, "RIGHTCLICK", "Right Button", ""},
		{BL_SENS_MOUSE_WHEEL_UP, "WHEELUP", "Wheel Up", ""},
		{BL_SENS_MOUSE_WHEEL_DOWN, "WHEELDOWN", "Wheel Down", ""},
		{BL_SENS_MOUSE_MOVEMENT, "MOVEMENT", "Movement", ""},
		{BL_SENS_MOUSE_MOUSEOVER, "MOUSEOVER", "Mouse Over", ""},
		{BL_SENS_MOUSE_MOUSEOVER_ANY, "MOUSEOVERANY", "Mouse Over Any", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "MouseSensor", NULL , "MouseSensor");
	RNA_def_struct_sdna(srna, "bMouseSensor");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, mouse_event_items);
	RNA_def_property_ui_text(prop, "Mouse Event", "Specify the type of event this mouse sensor should trigger on.");
}

void rna_def_touch_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "TouchSensor", NULL , "TouchSensor");
	RNA_def_struct_sdna(srna, "bTouchSensor");

	prop= RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ma");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Material", "only look for floors with this material.");

}

void rna_def_keyboard_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "KeyboardSensor", NULL , "KeyboardSensor");
	RNA_def_struct_sdna(srna, "bKeyboardSensor");

	prop= RNA_def_property(srna, "key", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Input Key", "Input key code.");
	RNA_def_property_range(prop, 0, 255);

	prop= RNA_def_property(srna, "modifier_key1", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "qual");
	RNA_def_property_ui_text(prop, "First Modifier Key", "Modifier key code.");
	RNA_def_property_range(prop, 0, 255);

	prop= RNA_def_property(srna, "modifier_key2", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "qual2");
	RNA_def_property_ui_text(prop, "Second Modifier Key", "Modifier key code.");
	RNA_def_property_range(prop, 0, 255);

	prop= RNA_def_property(srna, "target_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "targetName");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Target Name", "Property that indicates whether to log keystrokes as a string.");

	prop= RNA_def_property(srna, "toggle_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "toggleName");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Toggle Name", "Property that receive the keystrokes in case a string is logged.");

	prop= RNA_def_property(srna, "type", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "All Keys", "Trigger this sensor on any keystroke.");

}

void rna_def_property_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_items[] ={
		{SENS_PROP_EQUAL, "PROPEQUAL", "Equal", ""},
		{SENS_PROP_NEQUAL, "PROPNEQUAL", "Not Equal", ""},
		{SENS_PROP_INTERVAL, "PROPINTERVAL", "Interval", ""},
		{SENS_PROP_CHANGED, "PROPCHANGED", "Changed", ""},
		/* {SENS_PROP_EXPRESSION, "PROPEXPRESSION", "Expression", ""},  NOT_USED_IN_UI */
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "PropertySensor", NULL , "PropertySensor");
	RNA_def_struct_sdna(srna, "bPropertySensor");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_items);
	RNA_def_property_ui_text(prop, "Property Type", "Specify the type of property evaluation type.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Property Name", "Property name.");

	prop= RNA_def_property(srna, "first_value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "value");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "First Value", "Specify the value that checked, or minimum value in Interval Type.");

	prop= RNA_def_property(srna, "second_value", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "maxvalue");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Second Value", "Specify maximum value in Interval type.");
}

void rna_def_actuator_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActuatorSensor", NULL , "ActuatorSensor");
	RNA_def_struct_sdna(srna, "bActuatorSensor");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Actuator Name", "Actuator name.");
}

void rna_def_delay_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "DelaySensor", NULL , "DelaySensor");
	RNA_def_struct_sdna(srna, "bDelaySensor");

	prop= RNA_def_property(srna, "delay", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Delay", "Delay in number of logic tics before the positive trigger (default 60 per second).");
	RNA_def_property_range(prop, 0, 5000);

	prop= RNA_def_property(srna, "duration", PROP_INT, PROP_NONE);
	RNA_def_property_ui_text(prop, "Duration", "If >0, delay in number of logic tics before the negative trigger following the positive trigger.");
	RNA_def_property_range(prop, 0, 5000);

	prop= RNA_def_property(srna, "repeat", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_DELAY_REPEAT);
	RNA_def_property_ui_text(prop, "Repeat", "Toggle repeat option. If selected, the sensor restarts after Delay+Dur logic tics.");
}

void rna_def_collision_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CollisionSensor", NULL , "CollisionSensor");
	RNA_def_struct_sdna(srna, "bCollisionSensor");

	prop= RNA_def_property(srna, "property_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Property Name", "Only look for Objects with this property.");

	prop= RNA_def_property(srna, "material_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "materialName");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Material Name", "Only look for Objects with this material.");

	prop= RNA_def_property(srna, "type", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_COLLISION_MATERIAL);
	RNA_def_property_ui_text(prop, "Material/Property", "Toggle collision on material or property.");
}

void rna_def_radar_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem axis_items[] ={
		{SENS_RAY_X_AXIS, "RAYX", "+X axis", ""},
		{SENS_RAY_Y_AXIS, "RAYY", "+Y axis", ""},
		{SENS_RAY_Z_AXIS, "RAYZ", "+Z axis", ""},
		{SENS_RAY_NEG_X_AXIS, "RAYNX", "-X axis", ""},
		{SENS_RAY_NEG_Y_AXIS, "RAYNY", "-Y axis", ""},
		{SENS_RAY_NEG_Z_AXIS, "RAYNZ", "-Z axis", ""},
		{0, NULL, NULL, NULL}};
	srna= RNA_def_struct(brna, "RadarSensor", NULL , "RadarSensor");
	RNA_def_struct_sdna(srna, "bRadarSensor");

	prop= RNA_def_property(srna, "property_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Property Name", "Only look for Objects with this property.");

	prop= RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Specify along which axis the radar cone is cast.");

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 179.9);
	RNA_def_property_ui_text(prop, "Angle", "Opening angle of the radar cone.");

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "range");
	RNA_def_property_range(prop, 0.0, 10000.0);
	RNA_def_property_ui_text(prop, "Distance", "Depth of the radar cone.");
}

void rna_def_random_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "RandomSensor", NULL , "RandomSensor");
	RNA_def_struct_sdna(srna, "bRandomSensor");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "Seed", "Initial seed of the generator. (Choose 0 for not random).");

}

void rna_def_ray_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem axis_items[] ={
		{SENS_RAY_X_AXIS, "RAYX", "+X axis", ""},
		{SENS_RAY_Y_AXIS, "RAYY", "+Y axis", ""},
		{SENS_RAY_Z_AXIS, "RAYZ", "+Z axis", ""},
		{SENS_RAY_NEG_X_AXIS, "RAYNX", "-X axis", ""},
		{SENS_RAY_NEG_Y_AXIS, "RAYNY", "-Y axis", ""},
		{SENS_RAY_NEG_Z_AXIS, "RAYNZ", "-Z axis", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "RaySensor", NULL , "RaySensor");
	RNA_def_struct_sdna(srna, "bRaySensor");

	prop= RNA_def_property(srna, "property_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "propname");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Property Name", "Only look for Objects with this property.");

	prop= RNA_def_property(srna, "material_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "matname");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Material Name", "Only look for Objects with this material.");

	prop= RNA_def_property(srna, "type", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_COLLISION_MATERIAL);
	RNA_def_property_ui_text(prop, "Material/Property", "Toggle collision on material or property.");

	prop= RNA_def_property(srna, "x_ray_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", SENS_RAY_XRAY);
	RNA_def_property_ui_text(prop, "X-Ray Mode", "Toggle X-Ray option (see through objects that don't have the property).");

	prop= RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 10000.0);
	RNA_def_property_ui_text(prop, "Range", "Sense objects no farther than this distance.");

	prop= RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axisflag", PROP_DEF_ENUM_BITFLAGS);
	RNA_def_property_enum_items(prop, axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Specify along which axis the ray is cast.");

}

void rna_def_message_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MessageSensor", NULL , "MessageSensor");
	RNA_def_struct_sdna(srna, "bMessageSensor");

	prop= RNA_def_property(srna, "subject", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Subject", "Optional subject filter: only accept messages with this subject, or empty for all.");
}

void rna_def_joystick_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem event_type_items[] ={
		{SENS_JOY_BUTTON, "BUTTON", "Button", ""},
		{SENS_JOY_AXIS, "AXIS", "Axis", ""},
		{SENS_JOY_HAT, "HAT", "Hat", ""},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem axis_direction_items[] ={
		{SENS_JOY_X_AXIS, "RIGHTAXIS", "Right Axis", ""},
		{SENS_JOY_Y_AXIS, "UPAXIS", "Up Axis", ""},
		{SENS_JOY_NEG_X_AXIS, "LEFTAXIS", "Left Axis", ""},
		{SENS_JOY_NEG_Y_AXIS, "DOWNAXIS", "Down Axis", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "JoystickSensor", NULL , "JoystickSensor");
	RNA_def_struct_sdna(srna, "bJoystickSensor");
	
	prop= RNA_def_property(srna, "joystick_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "joyindex");
	RNA_def_property_ui_text(prop, "Index", "Specify which joystick to use.");
	RNA_def_property_range(prop, 0, SENS_JOY_MAXINDEX-1);


	prop= RNA_def_property(srna, "event_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type", PROP_DEF_ENUM_BITFLAGS);
	RNA_def_property_enum_items(prop, event_type_items);
	RNA_def_property_ui_text(prop, "Event Type", "The type of event this joystick sensor is triggered on.");

	prop= RNA_def_property(srna, "any_event", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SENS_JOY_ANY_EVENT);
	RNA_def_property_ui_text(prop, "All (Button/Axis/Hat) Events", "Triggered by all events on this joysticks current type (axis/button/hat).");

	/* Button */
	prop= RNA_def_property(srna, "button_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "button");
	RNA_def_property_ui_text(prop, "Button Number", "Specify which button to use.");
	RNA_def_property_range(prop, 0, 18);

	/* Axis */
	prop= RNA_def_property(srna, "axis_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "axis");
	RNA_def_property_ui_text(prop, "Axis Number", "Specify which axis pair to use, 1 is useually the main direction input.");
	RNA_def_property_range(prop, 1, 2);

	prop= RNA_def_property(srna, "axis_threshold", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "precision");
	RNA_def_property_ui_text(prop, "Axis Threshold", "Specify the precision of the axis.");
	RNA_def_property_range(prop, 0, 32768);

	prop= RNA_def_property(srna, "axis_direction", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axisf", PROP_DEF_ENUM_BITFLAGS);
	RNA_def_property_enum_items(prop, axis_direction_items);
	RNA_def_property_ui_text(prop, "Axis Direction", "The direction of the axis.");

	/* Hat */
	prop= RNA_def_property(srna, "hat_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hat");
	RNA_def_property_ui_text(prop, "Hat Number", "Specify which hat to use.");
	RNA_def_property_range(prop, 1, 2);

	prop= RNA_def_property(srna, "hat_direction", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hatf");
	RNA_def_property_ui_text(prop, "Hat Direction", "Specify hat direction.");
	RNA_def_property_range(prop, 0, 12);
}


void RNA_def_sensor(BlenderRNA *brna)
{
	rna_def_sensor(brna);

	rna_def_near_sensor(brna);
	rna_def_mouse_sensor(brna);
	rna_def_touch_sensor(brna);
	rna_def_keyboard_sensor(brna);
	rna_def_property_sensor(brna);
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

