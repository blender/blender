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

#include "DNA_windowmanager_types.h"

#ifdef RNA_RUNTIME

#include "BKE_idprop.h"

static wmOperator *rna_OperatorProperties_find_operator(PointerRNA *ptr)
{
	wmWindowManager *wm= ptr->id.data;
	IDProperty *properties= (IDProperty*)ptr->data;
	wmOperator *op;

	if(wm)
		for(op=wm->operators.first; op; op=op->next)
			if(op->properties == properties)
				return op;
	
	return NULL;
}

static StructRNA *rna_OperatorProperties_refine(PointerRNA *ptr)
{
	wmOperator *op= rna_OperatorProperties_find_operator(ptr);

	if(op)
		return op->type->srna;
	else
		return ptr->type;
}

IDProperty *rna_OperatorProperties_idproperties(PointerRNA *ptr, int create)
{
	if(create && !ptr->data) {
		IDPropertyTemplate val = {0};
		ptr->data= IDP_New(IDP_GROUP, val, "RNA_OperatorProperties group");
	}

	return ptr->data;
}

static void rna_Operator_name_get(PointerRNA *ptr, char *value)
{
	wmOperator *op= (wmOperator*)ptr->data;
	strcpy(value, op->type->name);
}

static int rna_Operator_name_length(PointerRNA *ptr)
{
	wmOperator *op= (wmOperator*)ptr->data;
	return strlen(op->type->name);
}

static PointerRNA rna_Operator_properties_get(PointerRNA *ptr)
{
	wmOperator *op= (wmOperator*)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_OperatorProperties, op->properties);
}

#else

static void rna_def_operator(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Operator", NULL);
	RNA_def_struct_ui_text(srna, "Operator", "Storage of an operator being executed, or registered after execution.");
	RNA_def_struct_sdna(srna, "wmOperator");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Operator_name_get", "rna_Operator_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "properties", PROP_POINTER, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "OperatorProperties");
	RNA_def_property_ui_text(prop, "Properties", "");
	RNA_def_property_pointer_funcs(prop, "rna_Operator_properties_get", NULL);

	srna= RNA_def_struct(brna, "OperatorProperties", NULL);
	RNA_def_struct_ui_text(srna, "Operator Properties", "Input properties of an Operator.");
	RNA_def_struct_refine_func(srna, "rna_OperatorProperties_refine");
	RNA_def_struct_idproperties_func(srna, "rna_OperatorProperties_idproperties");
}

static void rna_def_operator_utils(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "OperatorMousePath", "IDPropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator Mouse Path", "Mouse path values for operators that record such paths.");

	prop= RNA_def_property(srna, "loc", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_array(prop, 2);
	RNA_def_property_ui_text(prop, "Location", "Mouse location.");

	prop= RNA_def_property(srna, "time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Time", "Time of mouse location.");
}

static void rna_def_operator_filelist_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "OperatorFileListElement", "IDPropertyGroup");
	RNA_def_struct_ui_text(srna, "Operator File List Element", "");
	
	
	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Name", "the name of a file or directory within a file list");
}


static void rna_def_windowmanager(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "WindowManager", "ID");
	RNA_def_struct_ui_text(srna, "Window Manager", "Window manager datablock defining open windows and other user interface data.");
	RNA_def_struct_sdna(srna, "wmWindowManager");

	prop= RNA_def_property(srna, "operators", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "Operator");
	RNA_def_property_ui_text(prop, "Operators", "Operator registry.");
}

void RNA_def_wm(BlenderRNA *brna)
{
	rna_def_operator(brna);
	rna_def_operator_utils(brna);
	rna_def_operator_filelist_element(brna);
	rna_def_windowmanager(brna);
}

#endif

