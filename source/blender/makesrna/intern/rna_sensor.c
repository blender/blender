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
			return NULL;
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
	}

	return NULL;
}

#else

void rna_def_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem pulse_modes[]={
		{SENS_PULSE_CONT, "CONTINUE", "Continue Pulse", ""},
		{SENS_PULSE_REPEAT, "REPEAT", "Repeat Pulse", ""},
		{SENS_NEG_PULSE_MODE, "NEGATIVE", "Negative Pulse", ""},
		{0, NULL, NULL, NULL}};

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
	RNA_def_property_ui_text(prop, "Name", "Sensor name.");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, sensor_types_items);
	RNA_def_property_ui_text(prop, "Sensor types", "Sensor Types.");

	prop= RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Invert Output", "Invert the level(output) of this sensor.");

	prop= RNA_def_property(srna, "level", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_ui_text(prop, "Level", "Level detector, trigger controllers of new states(only applicable upon logic state transition).");

	prop= RNA_def_property(srna, "pulse", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, pulse_modes);
	RNA_def_property_ui_text(prop, "Sensor pulse modes", "Sensor pulse modes.");

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
	RNA_def_property_ui_text(prop, "Property", "Only llok for objects with this property.");

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
}

void rna_def_property_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PropertySensor", NULL , "PropertySensor");
	RNA_def_struct_sdna(srna, "bPropertySensor");
}

void rna_def_actuator_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActuatorSensor", NULL , "ActuatorSensor");
	RNA_def_struct_sdna(srna, "bActuatorSensor");
}

void rna_def_delay_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "DelaySensor", NULL , "DelaySensor");
	RNA_def_struct_sdna(srna, "bDelaySensor");
}

void rna_def_collision_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CollisionSensor", NULL , "CollisionSensor");
	RNA_def_struct_sdna(srna, "bCollisionSensor");
}

void rna_def_radar_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "RadarSensor", NULL , "RadarSensor");
	RNA_def_struct_sdna(srna, "bRadarSensor");
}

void rna_def_random_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "RandomSensor", NULL , "RandomSensor");
	RNA_def_struct_sdna(srna, "bRandomSensor");
}

void rna_def_ray_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "RaySensor", NULL , "RaySensor");
	RNA_def_struct_sdna(srna, "bRaySensor");
}

void rna_def_message_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MessageSensor", NULL , "MessageSensor");
	RNA_def_struct_sdna(srna, "bMessageSensor");
}

void rna_def_joystick_sensor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "JoystickSensor", NULL , "JoystickSensor");
	RNA_def_struct_sdna(srna, "bJoystickSensor");
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

