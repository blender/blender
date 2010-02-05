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

#include "DNA_controller_types.h"

#ifdef RNA_RUNTIME

static struct StructRNA* rna_Controller_refine(struct PointerRNA *ptr)
{
	bController *controller= (bController*)ptr->data;

	switch(controller->type) {
		case CONT_LOGIC_AND:
			return &RNA_AndController;
		case CONT_LOGIC_OR:
			return &RNA_OrController;
		case CONT_LOGIC_NAND:
			return &RNA_NandController;
		case CONT_LOGIC_NOR:
			return &RNA_NorController;
		case CONT_LOGIC_XOR:
			return &RNA_XorController;
		case CONT_LOGIC_XNOR:
			return &RNA_XnorController;
 		case CONT_EXPRESSION:
			return &RNA_ExpressionController;
		case CONT_PYTHON:
			return &RNA_PythonController;
		default:
			return &RNA_Controller;
	}
}

#else

void RNA_def_controller(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem controller_type_items[] ={
		{CONT_LOGIC_AND, "LOGIC_AND", 0, "Logic And", ""},
		{CONT_LOGIC_OR, "LOGIC_OR", 0, "Logic Or", ""},
		{CONT_LOGIC_NAND, "LOGIC_NAND", 0, "Logic Nand", ""},
		{CONT_LOGIC_NOR, "LOGIC_NOR", 0, "Logic Nor", ""},
		{CONT_LOGIC_XOR, "LOGIC_XOR", 0, "Logic Xor", ""},
		{CONT_LOGIC_XNOR, "LOGIC_XNOR", 0, "Logic Xnor", ""},
		{CONT_EXPRESSION, "EXPRESSION", 0, "Expression", ""},
		{CONT_PYTHON, "PYTHON", 0, "Python Script", ""},
		{0, NULL, 0, NULL, NULL}};

	/* Controller */
	srna= RNA_def_struct(brna, "Controller", NULL);
	RNA_def_struct_sdna(srna, "bController");
	RNA_def_struct_refine_func(srna, "rna_Controller_refine");
	RNA_def_struct_ui_text(srna, "Controller", "Game engine logic brick to process events, connecting sensors to actuators.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_enum_items(prop, controller_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	/* Expression Controller */
	srna= RNA_def_struct(brna, "ExpressionController", "Controller");
	RNA_def_struct_sdna_from(srna, "bExpressionCont", "data");
	RNA_def_struct_ui_text(srna, "Expression Controller", "Controller passing on events based on the evaluation of an expression.");

	prop= RNA_def_property(srna, "expression", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "str");
	RNA_def_property_string_maxlength(prop, 127);
	RNA_def_property_ui_text(prop, "Expression", "");

	/* Python Controller */
	srna= RNA_def_struct(brna, "PythonController", "Controller" );
	RNA_def_struct_sdna_from(srna, "bPythonCont", "data");
	RNA_def_struct_ui_text(srna, "Python Controller", "Controller executing a python script.");

	prop= RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Text", "Text datablock with the python script.");

	prop= RNA_def_property(srna, "module", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Module", "Module name and function to run e.g. \"someModule.main\". Internal texts and external python files can be used");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "debug", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CONT_PY_DEBUG);
	RNA_def_property_ui_text(prop, "D", "Continuously reload the module from disk for editing external modules without restarting.");

	/* Other Controllers */
	srna= RNA_def_struct(brna, "AndController", "Controller");
	RNA_def_struct_ui_text(srna, "And Controller", "Controller passing on events based on a logical AND operation.");
	
	srna= RNA_def_struct(brna, "OrController", "Controller");
	RNA_def_struct_ui_text(srna, "Or Controller", "Controller passing on events based on a logical OR operation.");
	
	srna= RNA_def_struct(brna, "NorController", "Controller");
	RNA_def_struct_ui_text(srna, "Nor Controller", "Controller passing on events based on a logical NOR operation.");
	
	srna= RNA_def_struct(brna, "NandController", "Controller");
	RNA_def_struct_ui_text(srna, "Nand Controller", "Controller passing on events based on a logical NAND operation.");
	
	srna= RNA_def_struct(brna, "XorController", "Controller");
	RNA_def_struct_ui_text(srna, "Xor Controller", "Controller passing on events based on a logical XOR operation.");
	
	srna= RNA_def_struct(brna, "XnorController", "Controller");
	RNA_def_struct_ui_text(srna, "Xnor Controller", "Controller passing on events based on a logical XNOR operation.");
}

#endif

