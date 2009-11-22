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

static void rna_uiItemR(uiLayout *layout, char *name, int icon, PointerRNA *ptr, char *propname, int expand, int slider, int toggle, int icon_only, int event, int full_event)
{
	int flag= 0;

	flag |= (slider)? UI_ITEM_R_SLIDER: 0;
	flag |= (expand)? UI_ITEM_R_EXPAND: 0;
	flag |= (toggle)? UI_ITEM_R_TOGGLE: 0;
	flag |= (icon_only)? UI_ITEM_R_ICON_ONLY: 0;
	flag |= (event)? UI_ITEM_R_EVENT: 0;
	flag |= (full_event)? UI_ITEM_R_FULL_EVENT: 0;
	
	uiItemR(layout, name, icon, ptr, propname, flag);
}

static PointerRNA rna_uiItemO(uiLayout *layout, char *name, int icon, char *opname, int properties)
{
	int flag= (properties)? UI_ITEM_O_RETURN_PROPS: 0;
	return uiItemFullO(layout, name, icon, opname, NULL, uiLayoutGetOperatorContext(layout), flag);
}

#else

#define DEF_ICON(name) {name, #name, 0, #name, ""},
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

	/* items */
	func= RNA_def_function(srna, "itemR", "rna_uiItemR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");
	RNA_def_boolean(func, "slider", 0, "", "Use slider widget for numeric values.");
	RNA_def_boolean(func, "toggle", 0, "", "Use toggle widget for boolean values.");
	RNA_def_boolean(func, "icon_only", 0, "", "Draw only icons in buttons, no text.");
	RNA_def_boolean(func, "event", 0, "", "Use button to input key events.");
	RNA_def_boolean(func, "full_event", 0, "", "Use button to input full events including modifiers.");

	func= RNA_def_function(srna, "items_enumR", "uiItemsEnumR");
	api_ui_item_rna_common(func);

	func= RNA_def_function(srna, "item_menu_enumR", "uiItemMenuEnumR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);

	func= RNA_def_function(srna, "item_enumR", "uiItemEnumR_string");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "item_pointerR", "uiItemPointerR");
	api_ui_item_common(func);
	api_ui_item_rna_common(func);
	parm= RNA_def_pointer(func, "search_data", "AnyType", "", "Data from which to take collection to search in.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm= RNA_def_string(func, "search_property", "", 0, "", "Identifier of search collection property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemO", "rna_uiItemO");
	api_ui_item_op_common(func);
	parm= RNA_def_boolean(func, "properties", 0, "Properties", "Return operator properties to fill in manually.");
	parm= RNA_def_pointer(func, "return_properties", "OperatorProperties", "", "Operator properties to fill in, return when 'properties' is set to true.");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	RNA_def_function_return(func, parm);

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
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_common(func);
	parm= RNA_def_string(func, "menu", "", 0, "", "Identifier of the menu.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemS", "uiItemS");

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
	parm= RNA_def_enum(func, "type", list_type_items, 0, "Type", "Type of list to use.");

	func= RNA_def_function(srna, "template_running_jobs", "uiTemplateRunningJobs");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func= RNA_def_function(srna, "template_operator_search", "uiTemplateOperatorSearch");

	func= RNA_def_function(srna, "template_header_3D", "uiTemplateHeader3D");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
}
#endif

