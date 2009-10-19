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

#ifdef RNA_RUNTIME

#else

void RNA_def_actuator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem actuator_type_items[] ={
		{ACT_OBJECT, "OBJECT", 0, "Object", ""},
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
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Actuator", NULL);
	RNA_def_struct_ui_text(srna, "Actuator", "Game engine logic brick to apply actions in the game engine.");
	
	RNA_def_struct_sdna(srna, "bActuator");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, actuator_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

}

#endif

