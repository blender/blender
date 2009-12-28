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
#include "UI_resources.h"

#ifdef RNA_RUNTIME

static void rna_uiItemR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, char *propname, int expand, int slider, int toggle, int icon_only, int event, int full_event, int no_bg, int index)
{
	PropertyRNA *prop= RNA_struct_find_property(ptr, propname);
	int flag= 0;

	if(!prop) {
		printf("rna_uiItemR: property not found: %s\n", propname);
		return;
	}

	flag |= (slider)? UI_ITEM_R_SLIDER: 0;
	flag |= (expand)? UI_ITEM_R_EXPAND: 0;
	flag |= (toggle)? UI_ITEM_R_TOGGLE: 0;
	flag |= (icon_only)? UI_ITEM_R_ICON_ONLY: 0;
	flag |= (event)? UI_ITEM_R_EVENT: 0;
	flag |= (full_event)? UI_ITEM_R_FULL_EVENT: 0;
	flag |= (no_bg)? UI_ITEM_R_NO_BG: 0;

	uiItemFullR(layout, name, icon, ptr, prop, index, 0, flag);
}

static PointerRNA rna_uiItemO(uiLayout *layout, char *name, int icon, char *opname)
{
	return uiItemFullO(layout, name, icon, opname, NULL, uiLayoutGetOperatorContext(layout), UI_ITEM_O_RETURN_PROPS);
}

#else

#define DEF_ICON(name) {name, (#name)+5, 0, (#name)+5, ""},
static EnumPropertyItem icon_items[] = {
#include "UI_icons.h"
		{0, NULL, 0, NULL, NULL}};
#undef DEF_ICON

static void api_ui_item_common(FunctionRNA *func)
{
	PropertyRNA *prop;

	RNA_def_string(func, "text", "", 0, "", "Override automatic text of the item.");

	prop= RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, icon_items);
	RNA_def_property_ui_text(prop, "Icon", "Override automatic icon of the item.");

}

static void api_ui_item_op_common(FunctionRNA *func)
{
	PropertyRNA *parm;

	api_ui_item_common(func);
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void api_ui_item_rna_common(FunctionRNA *func)
{
	PropertyRNA *parm;

	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_api_ui_layout(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem curve_type_items[] = {
		{0, "NONE", 0, "None", ""},
		{'v', "VECTOR", 0, "Vector", ""},
		{'c', "COLOR", 0, "Color", ""},
		{0, NULL, 0, NULL, NULL}};
	
	static EnumPropertyItem list_type_items[] = {
		{0, "DEFAULT", 0, "None", ""},
		{'c', "COMPACT", 0, "Compact", ""},
		{'i', "ICONS", 0, "Icons", ""},
		{0, NULL, 0, NULL, NULL}};

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
	RNA_def_float(func, "percentage", 0.0f, 0.0f, 1.0f, "Percentage", "Percentage of width to split at.", 0.0f, 1.0f);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other.");

	/* items */
	func= RNA_def_function(srna, "prop", "rna_uiItemR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");
	RNA_def_boolean(func, "slider", 0, "", "Use slider widget for numeric values.");
	RNA_def_boolean(func, "toggle", 0, "", "Use toggle widget for boolean values.");
	RNA_def_boolean(func, "icon_only", 0, "", "Draw only icons in buttons, no text.");
	RNA_def_boolean(func, "event", 0, "", "Use button to input key events.");
	RNA_def_boolean(func, "full_event", 0, "", "Use button to input full events including modifiers.");
	RNA_def_boolean(func, "no_bg", 0, "", "Don't draw the button itself, just the icon/text.");
	RNA_def_int(func, "index", -1, -2, INT_MAX, "", "The index of this button, when set a single member of an array can be accessed, when set to -1 all array members are used.", -2, INT_MAX); /* RNA_NO_INDEX == -1 */

	func= RNA_def_function(srna, "props_enum", "uiItemsEnumR");
	api_ui_item_rna_common(func);

	func= RNA_def_function(srna, "prop_menu_enum", "uiItemMenuEnumR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);

	func= RNA_def_function(srna, "prop_enum", "uiItemEnumR_string");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "prop_object", "uiItemPointerR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	parm= RNA_def_pointer(func, "search_data", "AnyType", "", "Data from which to take collection to search in.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "search_property", "", 0, "", "Identifier of search collection property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "operator", "rna_uiItemO");
	api_ui_item_op_common(func);
	parm= RNA_def_pointer(func, "properties", "OperatorProperties", "", "Operator properties to fill in, return when 'properties' is set to true.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	RNA_def_function_return(func, parm);

/*	func= RNA_def_function(srna, "operator_enum", "uiItemEnumO_string");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value.");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func= RNA_def_function(srna, "operator_enums", "uiItemsEnumO");
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "operator_menu_enum", "uiItemMenuEnumO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

/*	func= RNA_def_function(srna, "operator_boolean", "uiItemBooleanO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_boolean(func, "value", 0, "", "Value of the property to call the operator with.");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_int", "uiItemIntO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "", "Value of the property to call the operator with.", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_float", "uiItemFloatO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float(func, "value", 0, -FLT_MAX, FLT_MAX, "", "Value of the property to call the operator with.", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_string", "uiItemStringO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Value of the property to call the operator with.");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func= RNA_def_function(srna, "label", "uiItemL");
	api_ui_item_common(func);

	func= RNA_def_function(srna, "menu", "uiItemM");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_common(func);
	parm= RNA_def_string(func, "menu", "", 0, "", "Identifier of the menu.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "separator", "uiItemS");

	/* context */
	func= RNA_def_function(srna, "set_context_pointer", "uiLayoutSetContextPointer");
	parm= RNA_def_string(func, "name", "", 0, "Name", "Name of entry in the context.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Pointer to put in context.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	
	/* templates */
	func= RNA_def_function(srna, "template_header", "uiTemplateHeader");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_boolean(func, "menus", 1, "", "The header has menus, and should show menu expander.");

	func= RNA_def_function(srna, "template_dopesheet_filter", "uiTemplateDopeSheetFilter");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "dopesheet", "DopeSheet", "", "DopeSheet settings holding filter options.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);

	func= RNA_def_function(srna, "template_ID", "uiTemplateID");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_string(func, "new", "", 0, "", "Operator identifier to create a new ID block.");
	RNA_def_string(func, "open", "", 0, "", "Operator identifier to open a file for creating a new ID block.");
	RNA_def_string(func, "unlink", "", 0, "", "Operator identifier to unlink the ID block.");
	
	func= RNA_def_function(srna, "template_any_ID", "uiTemplateAnyID");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "type_property", "", 0, "", "Identifier of property in data giving the type of the ID-blocks to use.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "text", "", 0, "", "Custom label to display in UI.");
	
	func= RNA_def_function(srna, "template_path_builder", "uiTemplatePathBuilder");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "root", "ID", "", "ID-block from which path is evaluated from.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	parm= RNA_def_string(func, "text", "", 0, "", "Custom label to display in UI.");
	
	func= RNA_def_function(srna, "template_modifier", "uiTemplateModifier");
	parm= RNA_def_pointer(func, "data", "Modifier", "", "Modifier data.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_constraint", "uiTemplateConstraint");
	parm= RNA_def_pointer(func, "data", "Constraint", "", "Constraint data.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_preview", "uiTemplatePreview");
	parm= RNA_def_pointer(func, "id", "ID", "", "ID datablock.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_pointer(func, "parent", "ID", "", "ID datablock.");
	RNA_def_pointer(func, "slot", "TextureSlot", "", "Texture slot.");

	func= RNA_def_function(srna, "template_curve_mapping", "uiTemplateCurveMapping");
	api_ui_item_rna_common(func);
	RNA_def_enum(func, "type", curve_type_items, 0, "Type", "Type of curves to display.");
	RNA_def_boolean(func, "levels", 0, "", "Show black/white levels.");

	func= RNA_def_function(srna, "template_color_ramp", "uiTemplateColorRamp");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");
	
	func= RNA_def_function(srna, "template_layers", "uiTemplateLayers");
	api_ui_item_rna_common(func);
	parm= RNA_def_pointer(func, "used_layers_data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	parm= RNA_def_string(func, "used_layers_property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "active_layer", 0, 0, INT_MAX, "Active Layer", "", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func= RNA_def_function(srna, "template_triColorSet", "uiTemplateTriColorSet");
	api_ui_item_rna_common(func);

	func= RNA_def_function(srna, "template_image_layers", "uiTemplateImageLayers");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "image", "Image", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "image_user", "ImageUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "template_image", "uiTemplateImage");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	parm= RNA_def_pointer(func, "image_user", "ImageUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	RNA_def_boolean(func, "compact", 0, "", "Use more compact layout.");

	func= RNA_def_function(srna, "template_list", "uiTemplateList");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "active_data", "AnyType", "", "Data from which to take property for the active element.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "active_property", "", 0, "", "Identifier of property in data, for the active element.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "rows", 5, 0, INT_MAX, "", "Number of rows to display.", 0, INT_MAX);
	parm= RNA_def_int(func, "maxrows", 5, 0, INT_MAX, "", "Maximum number of rows to display.", 0, INT_MAX);
	parm= RNA_def_enum(func, "type", list_type_items, 0, "Type", "Type of list to use.");

	func= RNA_def_function(srna, "template_running_jobs", "uiTemplateRunningJobs");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func= RNA_def_function(srna, "template_operator_search", "uiTemplateOperatorSearch");

	func= RNA_def_function(srna, "template_header_3D", "uiTemplateHeader3D");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);



	func= RNA_def_function(srna, "introspect", "uiLayoutIntrospect");
	parm= RNA_def_string(func, "string", "", 1024*1024, "Descr", "DESCR");
	RNA_def_function_return(func, parm);
}
#endif

