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
/*
static struct StructRNA* rna_Sensor_data_type(struct PointerRNA *ptr)
{
// This function should reture the type of 
// void* data; 
// in bSensor structure
	switch( ((bSensor*)ptr)->type ){
		case SENS_MOUSE:
			return &RNA_MouseSensor;
	}
	return NULL;
}
*/
#else
void RNA_def_sensor(BlenderRNA *brna)
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

	srna= RNA_def_struct(brna, "Sensor", NULL , "Sensor");
	RNA_def_struct_sdna(srna, "bSensor");

	prop= RNA_def_property(srna, "sensor_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Sensor name.");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
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

	/*
	//This add data property to Sensor, and because data can be bMouseSensor, bNearSensor, bAlwaysSensor ...
	//rna_Sensor_data_type defines above in runtime section to get its type and proper structure for data
	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Data", "Sensor data.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Sensor_data_type", NULL);

	srna= RNA_def_struct(brna, "MouseSensor", NULL , "MouseSensor");
	RNA_def_struct_sdna(srna, "bMouseSensor");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, mouse_event_items);
	RNA_def_property_ui_text(prop, "MouseEvent", "Specify the type of event this mouse sensor should trigger on.");
	*/
}

#endif

