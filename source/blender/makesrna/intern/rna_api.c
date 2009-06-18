/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "UI_interface.h"


void RNA_api_main(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "add_mesh", "RNA_api_main_add_mesh");
	RNA_def_function_ui_description(func, "Add a new mesh.");
	prop= RNA_def_string(func, "name", "", 0, "", "New name for the datablock.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "mesh", "Mesh", "", "A new mesh.");
	RNA_def_function_return(func, prop);

	func= RNA_def_function(srna, "remove_mesh", "RNA_api_main_remove_mesh");
	RNA_def_function_ui_description(func, "Remove a mesh if it has only one user.");
	prop= RNA_def_pointer(func, "mesh", "Mesh", "", "A mesh to remove.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
}

void RNA_api_mesh(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	/*
	func= RNA_def_function(srna, "copy", "RNA_api_mesh_copy");
	RNA_def_function_ui_description(func, "Copy mesh data.");
	prop= RNA_def_pointer(func, "src", "Mesh", "", "A mesh to copy data from.");
	RNA_def_property_flag(prop, PROP_REQUIRED);*/

	func= RNA_def_function(srna, "make_rendermesh", "RNA_api_mesh_make_rendermesh");
	RNA_def_function_ui_description(func, "Copy mesh data from object with all modifiers applied.");
	prop= RNA_def_pointer(func, "sce", "Scene", "", "Scene.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "ob", "Object", "", "Object to copy data from.");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	/*
	func= RNA_def_function(srna, "add_geom", "RNA_api_mesh_add_geom");
	RNA_def_function_ui_description(func, "Add geometry data to mesh.");
	prop= RNA_def_collection(func, "verts", "?", "", "Vertices.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_collection(func, "faces", "?", "", "Faces.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	*/
}

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "add_fileselect", "RNA_api_wm_add_fileselect");
	RNA_def_function_ui_description(func, "Show up the file selector.");
	prop= RNA_def_pointer(func, "context", "Context", "", "Context.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop= RNA_def_pointer(func, "op", "Operator", "", "Operator to call.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
}

static void api_ui_item_common(FunctionRNA *func)
{
	RNA_def_string(func, "text", "", 0, "", "Override automatic text of the item.");
	RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "Override automatic icon of the item.", 0, INT_MAX);
}

static void api_ui_item_op_common(FunctionRNA *func)
{
	PropertyRNA *parm;

	api_ui_item_common(func);
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_api_ui_layout(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem curve_type_items[] = {
		{0, "NONE", "None", ""},
		{'v', "VECTOR", "Vector", ""},
		{'c', "COLOR", "Color", ""},
		{0, NULL, NULL, NULL}};

	/* simple layout specifiers */
	func= RNA_def_function(srna, "row", "uiLayoutRow");
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other.");

	func= RNA_def_function(srna, "column", "uiLayoutColumn");
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other.");

	func= RNA_def_function(srna, "column_flow", "uiLayoutColumnFlow");
	parm= RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns, 0 is automatic.", 0, INT_MAX);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other.");

	/* box layout */
	func= RNA_def_function(srna, "box", "uiLayoutBox");
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	/* split layout */
	func= RNA_def_function(srna, "split", "uiLayoutSplit");
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);
	RNA_def_float(func, "percentage", 0.5f, 0.0f, 1.0f, "Percentage", "Percentage of width to split at.", 0.0f, 1.0f);

	/* items */
	func= RNA_def_function(srna, "itemR", "uiItemR");
	api_ui_item_common(func);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");
	RNA_def_boolean(func, "slider", 0, "", "Use slider widget for numeric values.");
	RNA_def_boolean(func, "toggle", 0, "", "Use toggle widget for boolean values.");

	func= RNA_def_function(srna, "items_enumR", "uiItemsEnumR");
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_menu_enumR", "uiItemMenuEnumR");
	api_ui_item_common(func);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/*func= RNA_def_function(srna, "item_enumR", "uiItemEnumR");
	api_ui_item_common(func);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value.");
	RNA_def_property_flag(parm, PROP_REQUIRED);*/

	func= RNA_def_function(srna, "itemO", "uiItemO");
	api_ui_item_op_common(func);

	func= RNA_def_function(srna, "item_enumO", "uiItemEnumO_string");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "items_enumO", "uiItemsEnumO");
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_menu_enumO", "uiItemMenuEnumO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_booleanO", "uiItemBooleanO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_boolean(func, "value", 0, "", "Value of the property to call the operator with.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_intO", "uiItemIntO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "", "Value of the property to call the operator with.", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_floatO", "uiItemFloatO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float(func, "value", 0, -FLT_MAX, FLT_MAX, "", "Value of the property to call the operator with.", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_stringO", "uiItemStringO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Value of the property to call the operator with.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemL", "uiItemL");
	api_ui_item_common(func);

	func= RNA_def_function(srna, "itemM", "uiItemM");
	parm= RNA_def_pointer(func, "context", "Context", "", "Current context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);
	parm= RNA_def_string(func, "menu", "", 0, "", "Identifier of the menu.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemS", "uiItemS");

	/* context */
	func= RNA_def_function(srna, "set_context_pointer", "uiLayoutSetContextPointer");
	parm= RNA_def_string(func, "name", "", 0, "Name", "Name of entry in the context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Pointer to put in context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* templates */
	func= RNA_def_function(srna, "template_header", "uiTemplateHeader");
	parm= RNA_def_pointer(func, "context", "Context", "", "Current context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "template_ID", "uiTemplateID");
	parm= RNA_def_pointer(func, "context", "Context", "", "Current context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of pointer property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_string(func, "new", "", 0, "", "Operator identifier to create a new ID block.");
	RNA_def_string(func, "open", "", 0, "", "Operator identifier to open a new ID block.");
	RNA_def_string(func, "unlink", "", 0, "", "Operator identifier to unlink the ID block.");

	func= RNA_def_function(srna, "template_modifier", "uiTemplateModifier");
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Modifier data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_constraint", "uiTemplateConstraint");
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Constraint data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_preview", "uiTemplatePreview");
	parm= RNA_def_pointer(func, "id", "ID", "", "ID datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "template_curve_mapping", "uiTemplateCurveMapping");
	parm= RNA_def_pointer(func, "curvemap", "CurveMapping", "", "Curve mapping pointer.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "type", curve_type_items, 0, "Type", "Type of curves to display.");

	func= RNA_def_function(srna, "template_color_ramp", "uiTemplateColorRamp");
	parm= RNA_def_pointer(func, "ramp", "ColorRamp", "", "Color ramp pointer.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");
	
	func= RNA_def_function(srna, "template_layers", "uiTemplateLayers");
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of pointer property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}
