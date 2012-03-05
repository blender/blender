/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_ui_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"

#include "UI_resources.h"

#ifdef RNA_RUNTIME

static void rna_uiItemR(uiLayout *layout, PointerRNA *ptr, const char *propname, const char *name, int icon, int expand, int slider, int toggle, int icon_only, int event, int full_event, int emboss, int index)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	int flag = 0;

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	flag |= (slider)? UI_ITEM_R_SLIDER: 0;
	flag |= (expand)? UI_ITEM_R_EXPAND: 0;
	flag |= (toggle)? UI_ITEM_R_TOGGLE: 0;
	flag |= (icon_only)? UI_ITEM_R_ICON_ONLY: 0;
	flag |= (event)? UI_ITEM_R_EVENT: 0;
	flag |= (full_event)? UI_ITEM_R_FULL_EVENT: 0;
	flag |= (emboss)? 0: UI_ITEM_R_NO_BG;

	uiItemFullR(layout, ptr, prop, index, 0, flag, name, icon);
}

static PointerRNA rna_uiItemO(uiLayout *layout, const char *opname, const char *name, int icon, int emboss)
{
	int flag = UI_ITEM_O_RETURN_PROPS;
	flag |= (emboss)? 0: UI_ITEM_R_NO_BG;
	return uiItemFullO(layout, opname, name, icon, NULL, uiLayoutGetOperatorContext(layout), flag);
}

#else

#define DEF_ICON_BLANK_SKIP
#define DEF_ICON(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_VICO(name) {VICO_##name, (#name), 0, (#name), ""},
static EnumPropertyItem icon_items[] = {
#include "UI_icons.h"
		{0, NULL, 0, NULL, NULL}};
#undef DEF_ICON_BLANK_SKIP
#undef DEF_ICON
#undef DEF_VICO

static void api_ui_item_common(FunctionRNA *func)
{
	PropertyRNA *prop;

	RNA_def_string_translate(func, "text", "", 0, "", "Override automatic text of the item");

	prop = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, icon_items);
	RNA_def_property_ui_text(prop, "Icon", "Override automatic icon of the item");

}

static void api_ui_item_op(FunctionRNA *func)
{
	PropertyRNA *parm;
	parm = RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

static void api_ui_item_op_common(FunctionRNA *func)
{
	api_ui_item_op(func);
	api_ui_item_common(func);
}

static void api_ui_item_rna_common(FunctionRNA *func)
{
	PropertyRNA *parm;

	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in data");
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
	func = RNA_def_function(srna, "row", "uiLayoutRow");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Sub-layout. Items placed in this sublayout are placed next to each other in a row");
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other");
	
	func = RNA_def_function(srna, "column", "uiLayoutColumn");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Sub-layout. Items placed in this sublayout are placed under each other in a column");
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other");

	func = RNA_def_function(srna, "column_flow", "uiLayoutColumnFlow");
	RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns, 0 is automatic", 0, INT_MAX);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other");

	/* box layout */
	func = RNA_def_function(srna, "box", "uiLayoutBox");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Sublayout (items placed in this sublayout are placed "
	                                      "under each other in a column and are surrounded by a box)");
	
	/* split layout */
	func = RNA_def_function(srna, "split", "uiLayoutSplit");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_float(func, "percentage", 0.0f, 0.0f, 1.0f, "Percentage", "Percentage of width to split at", 0.0f, 1.0f);
	RNA_def_boolean(func, "align", 0, "", "Align buttons to each other");

	/* items */
	func = RNA_def_function(srna, "prop", "rna_uiItemR");
	RNA_def_function_ui_description(func, "Item. Exposes an RNA item and places it into the layout");
	api_ui_item_rna_common(func);
	api_ui_item_common(func);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail");
	RNA_def_boolean(func, "slider", 0, "", "Use slider widget for numeric values");
	RNA_def_boolean(func, "toggle", 0, "", "Use toggle widget for boolean values");
	RNA_def_boolean(func, "icon_only", 0, "", "Draw only icons in buttons, no text");
	RNA_def_boolean(func, "event", 0, "", "Use button to input key events");
	RNA_def_boolean(func, "full_event", 0, "", "Use button to input full events including modifiers");
	RNA_def_boolean(func, "emboss", 1, "", "Draw the button itself, just the icon/text");
	RNA_def_int(func, "index", -1, -2, INT_MAX, "",
	            "The index of this button, when set a single member of an array can be accessed, "
	            "when set to -1 all array members are used", -2, INT_MAX); /* RNA_NO_INDEX == -1 */

	func = RNA_def_function(srna, "props_enum", "uiItemsEnumR");
	api_ui_item_rna_common(func);

	func = RNA_def_function(srna, "prop_menu_enum", "uiItemMenuEnumR");
	api_ui_item_rna_common(func);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "prop_enum", "uiItemEnumR_string");
	api_ui_item_rna_common(func);
	parm = RNA_def_string(func, "value", "", 0, "", "Enum property value");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "prop_search", "uiItemPointerR");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "search_data", "AnyType", "", "Data from which to take collection to search in");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_string(func, "search_property", "", 0, "", "Identifier of search collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "operator", "rna_uiItemO");
	api_ui_item_op_common(func);
	RNA_def_boolean(func, "emboss", 1, "", "Draw the button itself, just the icon/text");
	parm = RNA_def_pointer(func, "properties", "OperatorProperties", "",
	                      "Operator properties to fill in, return when 'properties' is set to true");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Item. Places a button into the layout to call an Operator");

/*	func= RNA_def_function(srna, "operator_enum_single", "uiItemEnumO_string");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Enum property value");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func = RNA_def_function(srna, "operator_enum", "uiItemsEnumO");
	parm = RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "operator_menu_enum", "uiItemMenuEnumO");
	api_ui_item_op(func); /* cant use api_ui_item_op_common because property must come right after */
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

/*	func= RNA_def_function(srna, "operator_boolean", "uiItemBooleanO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_boolean(func, "value", 0, "", "Value of the property to call the operator with");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_int", "uiItemIntO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "",
	                  "Value of the property to call the operator with", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_float", "uiItemFloatO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_float(func, "value", 0, -FLT_MAX, FLT_MAX, "",
	                    "Value of the property to call the operator with", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

/*	func= RNA_def_function(srna, "operator_string", "uiItemStringO");
	api_ui_item_op_common(func);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "value", "", 0, "", "Value of the property to call the operator with");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func = RNA_def_function(srna, "label", "uiItemL");
	RNA_def_function_ui_description(func, "Item. Display text in the layout");
	api_ui_item_common(func);

	func = RNA_def_function(srna, "menu", "uiItemM");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "menu", "", 0, "", "Identifier of the menu");
	api_ui_item_common(func);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "separator", "uiItemS");
	RNA_def_function_ui_description(func, "Item. Inserts empty space into the layout between items");

	/* context */
	func = RNA_def_function(srna, "context_pointer_set", "uiLayoutSetContextPointer");
	parm = RNA_def_string(func, "name", "", 0, "Name", "Name of entry in the context");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Pointer to put in context");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	
	/* templates */
	func = RNA_def_function(srna, "template_header", "uiTemplateHeader");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_boolean(func, "menus", 1, "", "The header has menus, and should show menu expander");

	func = RNA_def_function(srna, "template_ID", "uiTemplateID");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_string(func, "new", "", 0, "", "Operator identifier to create a new ID block");
	RNA_def_string(func, "open", "", 0, "", "Operator identifier to open a file for creating a new ID block");
	RNA_def_string(func, "unlink", "", 0, "", "Operator identifier to unlink the ID block");
	
	func = RNA_def_function(srna, "template_ID_preview", "uiTemplateIDPreview");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_string(func, "new", "", 0, "", "Operator identifier to create a new ID block");
	RNA_def_string(func, "open", "", 0, "", "Operator identifier to open a file for creating a new ID block");
	RNA_def_string(func, "unlink", "", 0, "", "Operator identifier to unlink the ID block");
	RNA_def_int(func, "rows", 0, 0, INT_MAX, "Number of thumbnail preview rows to display", "", 0, INT_MAX);
	RNA_def_int(func, "cols", 0, 0, INT_MAX, "Number of thumbnail preview columns to display", "", 0, INT_MAX);
	
	func = RNA_def_function(srna, "template_any_ID", "uiTemplateAnyID");
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "type_property", "", 0, "",
	                     "Identifier of property in data giving the type of the ID-blocks to use");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_string_translate(func, "text", "", 0, "", "Custom label to display in UI");
	
	func = RNA_def_function(srna, "template_path_builder", "uiTemplatePathBuilder");
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "root", "ID", "", "ID-block from which path is evaluated from");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	RNA_def_string_translate(func, "text", "", 0, "", "Custom label to display in UI");
	
	func = RNA_def_function(srna, "template_modifier", "uiTemplateModifier");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Layout . Generates the UI layout for modifiers");
	parm = RNA_def_pointer(func, "data", "Modifier", "", "Modifier data");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "template_constraint", "uiTemplateConstraint");
	RNA_def_function_ui_description(func, "Layout . Generates the UI layout for constraints");
	parm = RNA_def_pointer(func, "data", "Constraint", "", "Constraint data");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "template_preview", "uiTemplatePreview");
	RNA_def_function_ui_description(func, "Item. A preview window for materials, textures, lamps, etc");
	parm = RNA_def_pointer(func, "id", "ID", "", "ID datablock");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "show_buttons", 1, "", "Show preview buttons?");
	RNA_def_pointer(func, "parent", "ID", "", "ID datablock");
	RNA_def_pointer(func, "slot", "TextureSlot", "", "Texture slot");

	func = RNA_def_function(srna, "template_curve_mapping", "uiTemplateCurveMapping");
	RNA_def_function_ui_description(func, "Item. A curve mapping widget used for e.g falloff curves for lamps");
	api_ui_item_rna_common(func);
	RNA_def_enum(func, "type", curve_type_items, 0, "Type", "Type of curves to display");
	RNA_def_boolean(func, "levels", 0, "", "Show black/white levels");
	RNA_def_boolean(func, "brush", 0, "", "Show brush options");

	func = RNA_def_function(srna, "template_color_ramp", "uiTemplateColorRamp");
	RNA_def_function_ui_description(func, "Item. A color ramp widget");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail");
	
	func = RNA_def_function(srna, "template_histogram", "uiTemplateHistogram");
	RNA_def_function_ui_description(func, "Item. A histogramm widget to analyze imaga data");
	api_ui_item_rna_common(func);
	
	func = RNA_def_function(srna, "template_waveform", "uiTemplateWaveform");
	RNA_def_function_ui_description(func, "Item. A waveform widget to analyze imaga data");
	api_ui_item_rna_common(func);
	
	func = RNA_def_function(srna, "template_vectorscope", "uiTemplateVectorscope");
	RNA_def_function_ui_description(func, "Item. A vectorscope widget to analyze imaga data");
	api_ui_item_rna_common(func);
	
	func = RNA_def_function(srna, "template_layers", "uiTemplateLayers");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "used_layers_data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	parm = RNA_def_string(func, "used_layers_property", "", 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "active_layer", 0, 0, INT_MAX, "Active Layer", "", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "template_color_wheel", "uiTemplateColorWheel");
	RNA_def_function_ui_description(func, "Item. A color wheel widget to pick colors");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "value_slider", 0, "", "Display the value slider to the right of the color wheel");
	RNA_def_boolean(func, "lock", 0, "", "Lock the color wheel display to value 1.0 regardless of actual color");
	RNA_def_boolean(func, "lock_luminosity", 0, "", "Keep the color at its original vector length");
	RNA_def_boolean(func, "cubic", 1, "", "Cubic saturation for picking values close to white");

	func = RNA_def_function(srna, "template_image_layers", "uiTemplateImageLayers");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "image", "Image", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "image_user", "ImageUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "template_image", "uiTemplateImage");
	RNA_def_function_ui_description(func, "Item(s). User interface for selecting images and their source paths");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "image_user", "ImageUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	RNA_def_boolean(func, "compact", 0, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_image_settings", "uiTemplateImageSettings");
	RNA_def_function_ui_description(func, "User interface for setting image format options");
	parm = RNA_def_pointer(func, "image_settings", "ImageFormatSettings", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);

	func = RNA_def_function(srna, "template_movieclip", "uiTemplateMovieClip");
	RNA_def_function_ui_description(func, "Item(s). User interface for selecting movie clips and their source paths");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "compact", 0, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_track", "uiTemplateTrack");
	RNA_def_function_ui_description(func, "Item. A movie-track widget to preview tracking image.");
	api_ui_item_rna_common(func);

	func = RNA_def_function(srna, "template_marker", "uiTemplateMarker");
	RNA_def_function_ui_description(func, "Item. A widget to control single marker settings.");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "clip_user", "MovieClipUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "track", "MovieTrackingTrack", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	RNA_def_boolean(func, "compact", 0, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_list", "uiTemplateList");
	RNA_def_function_ui_description(func, "Item. A list widget to display data. e.g. vertexgroups");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR);
	parm = RNA_def_string(func, "property", "", 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "active_data", "AnyType", "", "Data from which to take property for the active element");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);
	parm = RNA_def_string(func, "active_property", "", 0, "", "Identifier of property in data, for the active element");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_string(func, "prop_list", "", 0, "",
	               "Identifier of a string property in each data member, specifying which "
	               "of its properties should have a widget displayed in its row "
	               "(format: \"propname1:propname2:propname3:...\")");
	RNA_def_int(func, "rows", 5, 0, INT_MAX, "", "Number of rows to display", 0, INT_MAX);
	RNA_def_int(func, "maxrows", 5, 0, INT_MAX, "", "Maximum number of rows to display", 0, INT_MAX);
	RNA_def_enum(func, "type", list_type_items, 0, "Type", "Type of list to use");

	func = RNA_def_function(srna, "template_running_jobs", "uiTemplateRunningJobs");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	RNA_def_function(srna, "template_operator_search", "uiTemplateOperatorSearch");

	func = RNA_def_function(srna, "template_header_3D", "uiTemplateHeader3D");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "template_edit_mode_selection", "uiTemplateEditModeSelection");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	
	func = RNA_def_function(srna, "template_reports_banner", "uiTemplateReportsBanner");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "template_node_link", "uiTemplateNodeLink");
	parm = RNA_def_pointer(func, "ntree", "NodeTree", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "node", "Node", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "template_node_view", "uiTemplateNodeView");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "ntree", "NodeTree", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "node", "Node", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "socket", "NodeSocket", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "template_texture_user", "uiTemplateTextureUser");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "template_keymap_item_properties", "uiTemplateKeymapItemProperties");
	parm = RNA_def_pointer(func, "item", "KeyMapItem", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_RNAPTR|PROP_NEVER_NULL);

	func = RNA_def_function(srna, "introspect", "uiLayoutIntrospect");
	parm = RNA_def_string(func, "string", "", 1024*1024, "Descr", "DESCR");
	RNA_def_function_return(func, parm);
}
#endif

