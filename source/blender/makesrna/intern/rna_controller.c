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

#include "DNA_controller_types.h"

#ifdef RNA_RUNTIME
static struct StructRNA* rna_Controller_data_type(struct PointerRNA *ptr)
{
	bController *controller= (bController*)ptr->data;
	switch(controller->type){
		case CONT_LOGIC_AND:
		case CONT_LOGIC_OR:
		case CONT_LOGIC_NAND:
		case CONT_LOGIC_NOR:
		case CONT_LOGIC_XOR:
		case CONT_LOGIC_XNOR:
			return &RNA_UnknownType;
 		case CONT_EXPRESSION:
			return &RNA_ExpressionCont;
		case CONT_PYTHON:
			return &RNA_PythonCont;
	}
	return &RNA_UnknownType;
}
#else

void RNA_def_controller(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem controller_types_items[] ={
		{CONT_LOGIC_AND, "LOGICAND", "Logic And", ""},
		{CONT_LOGIC_OR, "LOGICOR", "Logic Or", ""},
		{CONT_LOGIC_NAND, "LOGICNAND", "Logic Nand", ""},
		{CONT_LOGIC_NOR, "LOGICNOR", "Logic Nor", ""},
		{CONT_LOGIC_XOR, "LOGICXOR", "Logic Xor", ""},
		{CONT_LOGIC_XNOR, "LOGICXNOR", "Logic Xnor", ""},
		{CONT_EXPRESSION, "EXPRESSION", "Expression", ""},
		{CONT_PYTHON, "PYTHON", "Python Script", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Controller", NULL , "Controller");
	RNA_def_struct_sdna(srna, "bController");

	prop= RNA_def_property(srna, "controller_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Name", "Controller name.");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, controller_types_items);
	RNA_def_property_ui_text(prop, "Controller Types", "Controller types.");

	prop= RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Data", "Controller data.");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Controller_data_type", NULL);

	srna= RNA_def_struct(brna, "ExpressionCont", NULL , "ExpressionCont");
	RNA_def_struct_sdna(srna, "bExpressionCont");

	prop= RNA_def_property(srna, "expression", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "str");
	RNA_def_property_string_maxlength(prop, 127);
	RNA_def_property_ui_text(prop, "Expression", "Expression.");

	srna= RNA_def_struct(brna, "PythonCont", NULL , "PythonCont");
	RNA_def_struct_sdna(srna, "bPythonCont");

	prop= RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "Python Text", "Python text.");
}

#endif

