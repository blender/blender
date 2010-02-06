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

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_actuator_types.h"
#include "DNA_scene_types.h" // for MAXFRAMEF

#include "WM_types.h"

#ifdef RNA_RUNTIME

static StructRNA* rna_Actuator_refine(struct PointerRNA *ptr)
{
	bActuator *actuator= (bActuator*)ptr->data;

	switch(actuator->type) {
		case ACT_OBJECT:
			return &RNA_ObjectActuator;
		case ACT_IPO:
			return &RNA_IpoActuator;
		case ACT_CAMERA:
			return &RNA_CameraActuator;
		case ACT_SOUND:
			return &RNA_SoundActuator;
		case ACT_PROPERTY:
			return &RNA_PropertyActuator;
		case ACT_CONSTRAINT:
			return &RNA_ConstraintActuator;
		case ACT_EDIT_OBJECT:
			return &RNA_EditObjectActuator;
		case ACT_SCENE:
			return &RNA_SceneActuator;
		case ACT_RANDOM:
			return &RNA_RandomActuator;
		case ACT_MESSAGE:
			return &RNA_MessageActuator;
		case ACT_ACTION:
			return &RNA_ActionActuator;
		case ACT_GAME:
			return &RNA_GameActuator;
		case ACT_VISIBILITY:
			return &RNA_VisibilityActuator;
		case ACT_2DFILTER:
			return &RNA_TwoDFilterActuator;
		case ACT_PARENT:
			return &RNA_ParentActuator;
		case ACT_SHAPEACTION:
			return &RNA_ShapeActionActuator;
		case ACT_STATE:
			return &RNA_StateActuator;
		case ACT_ARMATURE:
			return &RNA_ArmatureActuator;
		default:
			return &RNA_Actuator;
	}
}

#else

void rna_def_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem actuator_type_items[] ={
		{ACT_OBJECT, "OBJECT", 0, "Motion", ""},
		{ACT_IPO, "IPO", 0, "IPO", ""},
		{ACT_CAMERA, "CAMERA", 0, "Camera", ""},
		{ACT_SOUND, "SOUND", 0, "Sound", ""},
		{ACT_PROPERTY, "PROPERTY", 0, "Property", ""},
		{ACT_CONSTRAINT, "CONSTRAINT", 0, "Constraint", ""},
		{ACT_EDIT_OBJECT, "EDIT_OBJECT", 0, "Edit Object", ""},
		{ACT_SCENE, "SCENE", 0, "Scene", ""},
		{ACT_RANDOM, "RANDOM", 0, "Random", ""},
		{ACT_MESSAGE, "MESSAGE", 0, "Message", ""},
		{ACT_ACTION, "ACTION", 0, "Action", ""},
		{ACT_GAME, "GAME", 0, "Game", ""},
		{ACT_VISIBILITY, "VISIBILITY", 0, "Visibility", ""},
		{ACT_2DFILTER, "FILTER_2D", 0, "2D Filter", ""},
		{ACT_PARENT, "PARENT", 0, "Parent", ""},
		{ACT_SHAPEACTION, "SHAPE_ACTION", 0, "Shape Action", ""},
		{ACT_STATE, "STATE", 0, "State", ""},
		{ACT_ARMATURE, "ARMATURE", 0, "Armature", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Actuator", NULL);
	RNA_def_struct_ui_text(srna, "Actuator", "Game engine logic brick to apply actions in the game engine.");
	RNA_def_struct_sdna(srna, "bActuator");
	RNA_def_struct_refine_func(srna, "rna_Actuator_refine");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, actuator_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

}

static void rna_def_object_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ObjectActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Object Actuator", "Actuator to control the object movement.");
	RNA_def_struct_sdna_from(srna, "bObjectActuator", "data");
	
	//XXX
}

static void rna_def_ipo_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] ={
		{ACT_IPO_PLAY, "PLAY", 0, "Play", ""},
		{ACT_IPO_PINGPONG, "PINGPONG", 0, "Ping Pong", ""},
		{ACT_IPO_FLIPPER, "FLIPPER", 0, "Flipper", ""},
		{ACT_IPO_LOOP_STOP, "STOP", 0, "Loop Stop", ""},
		{ACT_IPO_LOOP_END, "END", 0, "Loop End", ""},
//		{ACT_IPO_KEY2KEY, "IPOCHILD", 0, "Key to Key", ""},
		{ACT_IPO_FROM_PROP, "PROP", 0, "Property", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "IpoActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Ipo Actuator", "Actuator to animate the object.");
	RNA_def_struct_sdna_from(srna, "bIpoActuator", "data");

	prop= RNA_def_property(srna, "play_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Ipo Type", "Specify the way you want to play the animation.");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "sta");
	RNA_def_property_ui_range(prop, 1, MAXFRAME, 1, 1);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "end");
	RNA_def_property_ui_range(prop, 1, MAXFRAME, 1, 1);
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Property", "Use this property to define the Ipo position");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "frame_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "frameProp");
	RNA_def_property_ui_text(prop, "Frame Property", "Assign the action's current frame number to this property");

	/* booleans */
	prop= RNA_def_property(srna, "force", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOFORCE);
	RNA_def_property_ui_text(prop, "Force", "Apply Ipo as a global or local force depending on the local option (dynamic objects only)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
//	logic_window::change_ipo_actuator
//	RNA_def_property_boolean_funcs(prop, "rna_Actuator_Ipo_get", "rna_Actuator_Ipo_get", "rna_Actuator_Ipo_range");	
	
	prop= RNA_def_property(srna, "local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOLOCAL);
	RNA_def_property_ui_text(prop, "L", "Let the ipo acts in local coordinates, used in Force and Add mode.");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "child", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOCHILD);
	RNA_def_property_ui_text(prop, "Child", "Update IPO on all children Objects as well");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "add", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOADD);
	RNA_def_property_ui_text(prop, "Add", "Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
//	logic_window::change_ipo_actuator
//	RNA_def_property_boolean_funcs(prop, "rna_Actuator_Ipo_get", "rna_Actuator_Ipo_get", "rna_Actuator_Ipo_range");	
}

static void rna_def_camera_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_axis_items[] ={
		{(float)'x', "X", 0, "X", "Camera tries to get behind the X axis"},
		{(float)'y', "Y", 0, "Y", "Camera tries to get behind the Y axis"},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "CameraActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Camera Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bCameraActuator", "data");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Camera Object", "Look at this Object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* floats */
	prop= RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 0.1, 0.1);
	RNA_def_property_ui_text(prop, "Height", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 0.1, 0.1);
	RNA_def_property_ui_text(prop, "Min", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0.0, 20.0, 0.1, 0.1);
	RNA_def_property_ui_text(prop, "Max", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* x/y */
	// It could be changed to be a regular ENUM instead of this weird "(float)string enum"
	prop= RNA_def_property(srna, "axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "axis");
	RNA_def_property_enum_items(prop, prop_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "Specify the axy the Camera will try to get behind.");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_sound_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SoundActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Sound Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bSoundActuator", "data");

	//XXX
}

static void rna_def_property_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "PropertyActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Property Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bPropertyActuator", "data");

	//XXX
}

static void rna_def_constraint_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ConstraintActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Constraint Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bConstraintActuator", "data");

	//XXX
}

static void rna_def_edit_object_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "EditObjectActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Edit Object Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bEditObjectActuator", "data");

	//XXX
}

static void rna_def_scene_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] ={
		{ACT_SCENE_RESTART, "RESTART", 0, "Restart", ""},
		{ACT_SCENE_SET, "SET", 0, "Set Scene", ""},
		{ACT_SCENE_CAMERA, "CAMERA", 0, "Set Camera", ""},
		{ACT_SCENE_ADD_FRONT, "ADDFRONT", 0, "Add OverlayScene", ""},
		{ACT_SCENE_ADD_BACK, "ADDBACK", 0, "Add BackgroundScene", ""},
		{ACT_SCENE_REMOVE, "REMOVE", 0, "Remove Scene", ""},
		{ACT_SCENE_SUSPEND, "SUSPEND", 0, "Suspend Scene", ""},
		{ACT_SCENE_RESUME, "RESUME", 0, "Resume Scene", ""},
		{0, NULL, 0, NULL, NULL}};	
		
	srna= RNA_def_struct(brna, "SceneActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Scene Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bSceneActuator", "data");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Scene", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Camera Object", "Set this Camera. Leave empty to refer to self object");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Scene", "Set the Scene to be added/removed/paused/resumed");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* XXX
	Originally we had different 'scene' tooltips for different values of 'type'.
	They were:
	ACT_SCENE_RESTART	""
	ACT_SCENE_CAMERA	""
	ACT_SCENE_SET		"Set this Scene"
	ACT_SCENE_ADD_FRONT	"Add an Overlay Scene"
	ACT_SCENE_ADD_BACK	"Add a Background Scene"
	ACT_SCENE_REMOVE	"Remove a Scene"
	ACT_SCENE_SUSPEND	"Pause a Scene"
	ACT_SCENE_RESUME	"Unpause a Scene"

	It can be done in the ui script if still needed.
	*/
	
}

static void rna_def_random_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_distribution_items[] ={
		{ACT_RANDOM_BOOL_CONST, "RESTART", 0, "Bool Constant", ""},
		{ACT_RANDOM_BOOL_UNIFORM, "SET", 0, "Bool Uniform", ""},
		{ACT_RANDOM_BOOL_BERNOUILLI, "CAMERA", 0, "Bool Bernoulli", ""},
		{ACT_RANDOM_INT_CONST, "ADDFRONT", 0, "Int Constant", ""},
		{ACT_RANDOM_INT_UNIFORM, "ADDBACK", 0, "Int Uniform", ""},
		{ACT_RANDOM_INT_POISSON, "REMOVE", 0, "Int Poisson", ""},
		{ACT_RANDOM_FLOAT_CONST, "SUSPEND", 0, "Float Constant", ""},
		{ACT_RANDOM_FLOAT_UNIFORM, "RESUME", 0, "Float Uniform", ""},
		{ACT_RANDOM_FLOAT_NORMAL, "RESUME", 0, "Float Normal", ""},
		{ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL, "RESUME", 0, "Float Neg. Exp.", ""},
		{0, NULL, 0, NULL, NULL}};	

	srna= RNA_def_struct(brna, "RandomActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Random Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bRandomActuator", "data");

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_ui_range(prop, 0, 1000, 1, 1);
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_text(prop, "Seed", "Initial seed of the random generator. Use Python for more freedom (choose 0 for not random)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "propname");
	RNA_def_property_ui_text(prop, "Property", "Assign the random value to this property");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_distribution_items);
	RNA_def_property_ui_text(prop, "Distribution", "Choose the type of distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* arguments for the distribution */
	/* int_arg_1, int_arg_2, float_arg_1, float_arg_2 */

	/* ACT_RANDOM_BOOL_CONST */
	prop= RNA_def_property(srna, "always_true", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "int_arg_1", 1);
	RNA_def_property_ui_text(prop, "Always true", "Always false or always true");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_BOOL_UNIFORM */
	// label => "Choose between true and false, 50% chance each"

	/* ACT_RANDOM_BOOL_BERNOUILLI */
	prop= RNA_def_property(srna, "chance", PROP_FLOAT, PROP_PERCENTAGE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Chance", "Pick a number between 0 and 1. Success if you stay below this value");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_CONST */
	prop= RNA_def_property(srna, "int_value", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_1");
	RNA_def_property_ui_range(prop, -1000, 1000, 1, 1);
	RNA_def_property_ui_text(prop, "Value", "Always return this number");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_UNIFORM */
	prop= RNA_def_property(srna, "int_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_1");
	RNA_def_property_range(prop, -1000, 1000);
	RNA_def_property_ui_text(prop, "Min", "Choose a number from a range. Lower boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "int_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg_2");
	RNA_def_property_range(prop, -1000, 1000);
	RNA_def_property_ui_text(prop, "Max", "Choose a number from a range. Upper boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_INT_POISSON */
	prop= RNA_def_property(srna, "int_mean", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.01, 100.0);
	RNA_def_property_ui_text(prop, "Mean", "Expected mean value of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_CONST */
	prop= RNA_def_property(srna, "float_value", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Value", "Always return this number");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_UNIFORM */
	prop= RNA_def_property(srna, "float_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Min", "Choose a number from a range. Lower boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "float_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_2");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Max", "Choose a number from a range. Upper boundary of the range");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_NORMAL */
	prop= RNA_def_property(srna, "float_mean", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Mean", "A normal distribution. Mean of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "standard_derivation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_2");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "SD", "A normal distribution. Standard deviation of the distribution");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* ACT_RANDOM_FLOAT_NEGATIVE_EXPONENTIAL */
	prop= RNA_def_property(srna, "half_life_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg_1");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Half-life time", "Negative exponential dropoff");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_message_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_body_type_items[] ={
		{ACT_MESG_MESG, "TEXT", 0, "Text", ""},
		{ACT_MESG_PROP, "PROPERTY", 0, "Property", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "MessageActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Message Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bMessageActuator", "data");

	prop= RNA_def_property(srna, "to_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "toPropName");
	RNA_def_property_ui_text(prop, "To", "Optional send message to objects with this name only, or empty to broadcast");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "subject", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Subject", "Optional message subject. This is what can be filtered on");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "body_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bodyType");
	RNA_def_property_enum_items(prop, prop_body_type_items);
	RNA_def_property_ui_text(prop, "Body Type", "Toggle message type: either Text or a PropertyName");

	/* ACT_MESG_MESG */
	prop= RNA_def_property(srna, "body_message", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "body");
	RNA_def_property_ui_text(prop, "Body", "Optional message body Text");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* ACT_MESG_PROP */
	prop= RNA_def_property(srna, "body_property", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "body");
	RNA_def_property_ui_text(prop, "Propname", "The message body will be set by the Property Value");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Action Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bActionActuator", "data");

	//XXX
}

static void rna_def_game_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] ={
//		{ACT_GAME_LOAD, "LOAD", 0, "Load game", ""},
//		{ACT_GAME_START, "START", 0, "Start loaded game", ""},	
//		keeping the load/start hacky for compatibility with 2.49
//		ideally we could use ACT_GAME_START again and do a do_version()

		{ACT_GAME_LOAD, "START", 0, "Start new game", ""},
		{ACT_GAME_RESTART, "RESTART", 0, "Restart this game", ""},
		{ACT_GAME_QUIT, "QUIT", 0, "Quit this game", ""},
		{ACT_GAME_SAVECFG, "SAVECFG", 0, "Save GameLogic.globalDict", ""},
		{ACT_GAME_LOADCFG, "LOADCFG", 0, "Load GameLogic.globalDict", ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "GameActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Game Actuator", "");
	RNA_def_struct_sdna_from(srna, "bGameActuator", "data");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Game", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "File", "Load this blend file, use the \"//\" prefix for a path relative to the current blend file");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_visibility_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "VisibilityActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Visibility Actuator", "Actuator to set visibility and occlusion of the object");
	RNA_def_struct_sdna_from(srna, "bVisibilityActuator", "data");

	prop= RNA_def_property(srna, "visible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_VISIBILITY_INVISIBLE);
	RNA_def_property_ui_text(prop, "Visible", "Set the objects visible. Initialized from the objects render restriction toggle (access in the outliner)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "occlusion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_VISIBILITY_OCCLUSION);
	RNA_def_property_ui_text(prop, "Occlusion", "Set the object to occlude objects behind it. Initialized from the object type in physics button");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "children", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_VISIBILITY_RECURSIVE);
	RNA_def_property_ui_text(prop, "Children", "Sets all the children of this object to the same visibility/occlusion recursively");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_twodfilter_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_type_items[] ={
		{ACT_2DFILTER_ENABLED, "ENABLE", 0, "Enable Filter", ""},
		{ACT_2DFILTER_DISABLED, "DISABLE", 0, "Disable Filter", ""},
		{ACT_2DFILTER_NOFILTER, "REMOVE", 0, "Remove Filter", ""},
		{ACT_2DFILTER_MOTIONBLUR, "MOTIONBLUR", 0, "Motion Blur", ""},
		{ACT_2DFILTER_BLUR, "BLUR", 0, "Blur", ""},
		{ACT_2DFILTER_SHARPEN, "SHARPEN", 0, "Sharpen", ""},
		{ACT_2DFILTER_DILATION, "DILATION", 0, "Dilation", ""},
		{ACT_2DFILTER_EROSION, "EROSION", 0, "Erosion", ""},
		{ACT_2DFILTER_LAPLACIAN, "LAPLACIAN", 0, "Laplacian", ""},
		{ACT_2DFILTER_SOBEL, "SOBEL", 0, "Sobel", ""},
		{ACT_2DFILTER_PREWITT, "PREWITT", 0, "Prewitt", ""},
		{ACT_2DFILTER_GRAYSCALE, "GRAYSCALE", 0, "Gray Scale", ""},
		{ACT_2DFILTER_SEPIA, "SEPIA", 0, "Sepia", ""},
		{ACT_2DFILTER_INVERT, "INVERT", 0, "Invert", ""},
		{ACT_2DFILTER_CUSTOMFILTER, "CUSTOMFILTER", 0, "Custom Filter", ""},
//		{ACT_2DFILTER_NUMBER_OF_FILTERS, "", 0, "Do not use it. Sentinel", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "TwoDFilterActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "2D Filter Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bTwoDFilterActuator", "data");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "2D Filter Type", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "glsl_shader", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "text");
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Script", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "filter_pass", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "int_arg");
	RNA_def_property_ui_text(prop, "Pass Number", "Set filter order");
	RNA_def_property_range(prop, 0, 99); //MAX_RENDER_PASS-1
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "motion_blur_value", PROP_FLOAT, PROP_PERCENTAGE);
	RNA_def_property_float_sdna(prop, NULL, "float_arg");
	RNA_def_property_ui_text(prop, "Value", "Set motion blur value");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	/* booleans */
	// it must be renamed to enable_motion_blur.
	// it'll require code change and do_version()
	// or RNA_def_property_boolean_funcs() to flip the boolean value
	prop= RNA_def_property(srna, "disable_motion_blur", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 1);
	RNA_def_property_ui_text(prop, "D", "Enable/Disable Motion Blur");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_parent_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] ={
		{ACT_PARENT_SET, "SETPARENT", 0, "Set Parent", ""},
		{ACT_PARENT_REMOVE, "REMOVEPARENT", 0, "Remove Parent", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "ParentActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Parent Actuator", "");
	RNA_def_struct_sdna_from(srna, "bParentActuator", "data");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Scene", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "ob");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Parent Object", "Set this object as parent");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* booleans */
	prop= RNA_def_property(srna, "compound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_PARENT_COMPOUND);
	RNA_def_property_ui_text(prop, "Compound", "Add this object shape to the parent shape (only if the parent shape is already compound)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "ghost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_PARENT_GHOST);
	RNA_def_property_ui_text(prop, "Ghost", "Make this object ghost while parented (only if not compound)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
}

static void rna_def_shape_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ShapeActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Shape Action Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bShapeActionActuator", "data");

	//XXX
}

static void rna_def_state_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "StateActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "State Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bStateActuator", "data");

	//XXX
}

static void rna_def_armature_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ArmatureActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Armature Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bArmatureActuator", "data");

	//XXX
}

void RNA_def_actuator(BlenderRNA *brna)
{
	rna_def_actuator(brna);

	rna_def_object_actuator(brna);		// to be done
	rna_def_ipo_actuator(brna);
	rna_def_camera_actuator(brna);
	rna_def_sound_actuator(brna);		// to be done
	rna_def_property_actuator(brna);	// to be done
	rna_def_constraint_actuator(brna);	// to be done
	rna_def_edit_object_actuator(brna);	// to be done
	rna_def_scene_actuator(brna);
	rna_def_random_actuator(brna);
	rna_def_message_actuator(brna);
	rna_def_action_actuator(brna);		// to be done
	rna_def_game_actuator(brna);
	rna_def_visibility_actuator(brna);
	rna_def_twodfilter_actuator(brna);
	rna_def_parent_actuator(brna);
	rna_def_shape_action_actuator(brna);// to be done
	rna_def_state_actuator(brna);		// to be done
	rna_def_armature_actuator(brna);	// to be done

}

#endif

