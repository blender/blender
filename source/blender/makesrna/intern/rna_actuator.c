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
	RNA_def_property_ui_range(prop, 1, MAXFRAMEF, 1, 1);
	RNA_def_property_ui_text(prop, "Start Frame", "");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "end");
	RNA_def_property_ui_range(prop, 1, MAXFRAMEF, 1, 1);
	RNA_def_property_ui_text(prop, "End Frame", "");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "prop", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Prop", "Use this property to define the Ipo position.");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "frame_prop", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "frameProp");
	RNA_def_property_ui_text(prop, "FrameProp", "Assign the action's current frame number to this property");

	/* booleans */
	prop= RNA_def_property(srna, "ipo_force", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOFORCE);
	RNA_def_property_ui_text(prop, "Force", "Apply Ipo as a global or local force depending on the local option (dynamic objects only)");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	//logic_window::change_ipo_actuator
	
	prop= RNA_def_property(srna, "ipo_local", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOLOCAL);
	RNA_def_property_ui_text(prop, "L", "Let the ipo acts in local coordinates, used in Force and Add mode.");
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "ipo_child", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOCHILD);
	RNA_def_property_ui_text(prop, "Child", "Update IPO on all children Objects as well");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "ipo_add", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", ACT_IPOADD);
	RNA_def_property_ui_text(prop, "Add", "Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag");
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	//logic_window::change_ipo_actuator
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
	RNA_def_property_update(prop, NC_OBJECT|ND_DRAW, NULL);

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
	srna= RNA_def_struct(brna, "SoundActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Sound Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bSoundActuator", "data");

	//XXX
}

static void rna_def_property_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "PropertyActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Property Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bPropertyActuator", "data");

	//XXX
}

static void rna_def_constraint_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "ConstraintActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Constraint Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bConstraintActuator", "data");

	//XXX
}

static void rna_def_edit_object_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "EditObjectActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Edit Object Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bEditObjectActuator", "data");

	//XXX
}

static void rna_def_scene_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "SceneActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Scene Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bSceneActuator", "data");

	//XXX
}

static void rna_def_random_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "RandomActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Random Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bRandomActuator", "data");

	//XXX
}

static void rna_def_message_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "MessageActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Message Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bMessageActuator", "data");

	//XXX
}

static void rna_def_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "ActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Action Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bActionActuator", "data");

	//XXX
}

static void rna_def_game_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "GameActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Game Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bGameActuator", "data");

	//XXX
}

static void rna_def_visibility_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "VisibilityActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Visibility Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bVisibilityActuator", "data");

	//XXX
}

static void rna_def_twodfilter_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "TwoDFilterActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "2D Filter Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bTwoDFilterActuator", "data");

	//XXX
}

static void rna_def_parent_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "ParentActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Parent Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bParentActuator", "data");

	//XXX
}

static void rna_def_shape_action_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "ShapeActionActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "Shape Action Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bShapeActionActuator", "data");

	//XXX
}

static void rna_def_state_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	srna= RNA_def_struct(brna, "StateActuator", "Actuator");
	RNA_def_struct_ui_text(srna, "State Actuator", "Actuator to ...");
	RNA_def_struct_sdna_from(srna, "bStateActuator", "data");

	//XXX
}

static void rna_def_armature_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
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
	rna_def_scene_actuator(brna);		// to be done
	rna_def_random_actuator(brna);		// to be done
	rna_def_message_actuator(brna);		// to be done
	rna_def_action_actuator(brna);		// to be done
	rna_def_game_actuator(brna);		// to be done
	rna_def_visibility_actuator(brna);	// to be done
	rna_def_twodfilter_actuator(brna);	// to be done
	rna_def_parent_actuator(brna);		// to be done
	rna_def_shape_action_actuator(brna);// to be done
	rna_def_state_actuator(brna);		// to be done
	rna_def_armature_actuator(brna);	// to be done

}

#endif

