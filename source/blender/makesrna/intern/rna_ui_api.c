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

#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "rna_internal.h"

#define DEF_ICON_BLANK_SKIP
#define DEF_ICON(name) {ICON_##name, (#name), 0, (#name), ""},
#define DEF_VICO(name) {VICO_##name, (#name), 0, (#name), ""},
EnumPropertyItem icon_items[] = {
#include "UI_icons.h"
	{0, NULL, 0, NULL, NULL}
};
#undef DEF_ICON_BLANK_SKIP
#undef DEF_ICON
#undef DEF_VICO

#ifdef RNA_RUNTIME

static const char *rna_translate_ui_text(const char *text, const char *text_ctxt, StructRNA *type, PropertyRNA *prop,
                                         int translate)
{
	/* Also return text if UI labels translation is disabled. */
	if (!text || !text[0] || !translate || !BLF_translate_iface()) {
		return text;
	}

	/* If a text_ctxt is specified, use it! */
	if (text_ctxt && text_ctxt[0]) {
		return BLF_pgettext(text_ctxt, text);
	}

	/* Else, if an RNA type or property is specified, use its context. */
#if 0
	/* XXX Disabled for now. Unfortunately, their is absolutely no way from py code to get the RNA struct corresponding
	 *     to the 'data' (in functions like prop() & co), as this is pure runtime data. Hence, messages extraction
	 *     script can't determine the correct context it should use for such 'text' messages...
	 *     So for now, one have to explicitly specify the 'text_ctxt' when using prop() etc. functions,
	 *     if default context is not suitable.
	 */
	if (prop) {
		return BLF_pgettext(RNA_property_translation_context(prop), text);
	}
#else
	(void)prop;
#endif
	if (type) {
		return BLF_pgettext(RNA_struct_translation_context(type), text);
	}

	/* Else, default context! */
	return BLF_pgettext(BLF_I18NCONTEXT_DEFAULT, text);
}

static void rna_uiItemR(uiLayout *layout, PointerRNA *ptr, const char *propname, const char *name, const char *text_ctxt,
                        int translate, int icon, int expand, int slider, int toggle, int icon_only, int event,
                        int full_event, int emboss, int index, int icon_value)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
	int flag = 0;

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	if (icon_value && !icon) {
		icon = icon_value;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	flag |= (slider) ? UI_ITEM_R_SLIDER : 0;
	flag |= (expand) ? UI_ITEM_R_EXPAND : 0;
	flag |= (toggle) ? UI_ITEM_R_TOGGLE : 0;
	flag |= (icon_only) ? UI_ITEM_R_ICON_ONLY : 0;
	flag |= (event) ? UI_ITEM_R_EVENT : 0;
	flag |= (full_event) ? UI_ITEM_R_FULL_EVENT : 0;
	flag |= (emboss) ? 0 : UI_ITEM_R_NO_BG;

	uiItemFullR(layout, ptr, prop, index, 0, flag, name, icon);
}

static void rna_uiItemMenuEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name,
                                const char *text_ctxt, int translate, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	/* XXX This will search property again :( */
	uiItemMenuEnumR(layout, ptr, propname, name, icon);
}

static void rna_uiItemEnumR_string(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *value,
                                   const char *name, const char *text_ctxt, int translate, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	/* XXX This will search property again :( */
	uiItemEnumR_string(layout, ptr, propname, value, name, icon);
}

static void rna_uiItemPointerR(uiLayout *layout, struct PointerRNA *ptr, const char *propname,
                               struct PointerRNA *searchptr, const char *searchpropname,
                               const char *name, const char *text_ctxt, int translate, int icon)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	/* XXX This will search property again :( */
	uiItemPointerR(layout, ptr, propname, searchptr, searchpropname, name, icon);
}

static PointerRNA rna_uiItemO(uiLayout *layout, const char *opname, const char *name, const char *text_ctxt,
                              int translate, int icon, int emboss, int icon_value)
{
	wmOperatorType *ot;
	int flag;

	ot = WM_operatortype_find(opname, 0); /* print error next */
	if (!ot || !ot->srna) {
		RNA_warning("%s '%s'", ot ? "unknown operator" : "operator missing srna", opname);
		return PointerRNA_NULL;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, ot->srna, NULL, translate);

	if (icon_value && !icon) {
		icon = icon_value;
	}

	flag = UI_ITEM_O_RETURN_PROPS;
	flag |= (emboss) ? 0 : UI_ITEM_R_NO_BG;

	return uiItemFullO_ptr(layout, ot, name, icon, NULL, uiLayoutGetOperatorContext(layout), flag);
}

static void rna_uiItemMenuEnumO(uiLayout *layout, bContext *C, const char *opname, const char *propname, const char *name,
                                const char *text_ctxt, int translate, int icon)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0); /* print error next */

	if (!ot || !ot->srna) {
		RNA_warning("%s '%s'", ot ? "unknown operator" : "operator missing srna", opname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, ot->srna, NULL, translate);

	/* XXX This will search operator again :( */
	uiItemMenuEnumO(layout, C, opname, propname, name, icon);
}

static void rna_uiItemL(uiLayout *layout, const char *name, const char *text_ctxt, int translate,
                        int icon, int icon_value)
{
	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, NULL, translate);

	if (icon_value && !icon) {
		icon = icon_value;
	}

	uiItemL(layout, name, icon);
}

static void rna_uiItemM(uiLayout *layout, bContext *C, const char *menuname, const char *name, const char *text_ctxt,
                        int translate, int icon)
{
	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, NULL, translate);

	uiItemM(layout, C, menuname, name, icon);
}

static void rna_uiTemplateAnyID(uiLayout *layout, PointerRNA *ptr, const char *propname, const char *proptypename,
                                const char *name, const char *text_ctxt, int translate)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	/* XXX This will search property again :( */
	uiTemplateAnyID(layout, ptr, propname, proptypename, name);
}

static void rna_uiTemplatePathBuilder(uiLayout *layout, PointerRNA *ptr, const char *propname, PointerRNA *root_ptr,
                                      const char *name, const char *text_ctxt, int translate)
{
	PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

	if (!prop) {
		RNA_warning("property not found: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return;
	}

	/* Get translated name (label). */
	name = rna_translate_ui_text(name, text_ctxt, NULL, prop, translate);

	/* XXX This will search property again :( */
	uiTemplatePathBuilder(layout, ptr, propname, root_ptr, name);
}

static int rna_ui_get_rnaptr_icon(bContext *C, PointerRNA *ptr_icon)
{
	return UI_rnaptr_icon_get(C, ptr_icon, RNA_struct_ui_icon(ptr_icon->type), false);
}

static const char *rna_ui_get_enum_name(bContext *C, PointerRNA *ptr, const char *propname, const char *identifier)
{
	PropertyRNA *prop = NULL;
	EnumPropertyItem *items = NULL, *item;
	bool free;
	const char *name = "";

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
		RNA_warning("Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return name;
	}

	RNA_property_enum_items_gettexted(C, ptr, prop, &items, NULL, &free);

	if (items) {
		for (item = items; item->identifier; item++) {
			if (item->identifier[0] && strcmp(item->identifier, identifier) == 0) {
				name = item->name;
				break;
			}
		}
		if (free) {
			MEM_freeN(items);
		}
	}

	return name;
}

static const char *rna_ui_get_enum_description(bContext *C, PointerRNA *ptr, const char *propname,
                                               const char *identifier)
{
	PropertyRNA *prop = NULL;
	EnumPropertyItem *items = NULL, *item;
	bool free;
	const char *desc = "";

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
		RNA_warning("Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return desc;
	}

	RNA_property_enum_items_gettexted(C, ptr, prop, &items, NULL, &free);

	if (items) {
		for (item = items; item->identifier; item++) {
			if (item->identifier[0] && strcmp(item->identifier, identifier) == 0) {
				desc = item->description;
				break;
			}
		}
		if (free) {
			MEM_freeN(items);
		}
	}

	return desc;
}

static int rna_ui_get_enum_icon(bContext *C, PointerRNA *ptr, const char *propname, const char *identifier)
{
	PropertyRNA *prop = NULL;
	EnumPropertyItem *items = NULL, *item;
	bool free;
	int icon = ICON_NONE;

	prop = RNA_struct_find_property(ptr, propname);
	if (!prop || (RNA_property_type(prop) != PROP_ENUM)) {
		RNA_warning("Property not found or not an enum: %s.%s", RNA_struct_identifier(ptr->type), propname);
		return icon;
	}

	RNA_property_enum_items(C, ptr, prop, &items, NULL, &free);

	if (items) {
		for (item = items; item->identifier; item++) {
			if (item->identifier[0] && strcmp(item->identifier, identifier) == 0) {
				icon = item->icon;
				break;
			}
		}
		if (free) {
			MEM_freeN(items);
		}
	}

	return icon;
}

#else

static void api_ui_item_common_text(FunctionRNA *func)
{
	RNA_def_string(func, "text", NULL, 0, "", "Override automatic text of the item");
	RNA_def_string(func, "text_ctxt", NULL, 0, "", "Override automatic translation context of the given text");
	RNA_def_boolean(func, "translate", true, "", "Translate the given text, when UI translation is enabled");
}

static void api_ui_item_common(FunctionRNA *func)
{
	PropertyRNA *prop;

	api_ui_item_common_text(func);

	prop = RNA_def_property(func, "icon", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, icon_items);
	RNA_def_property_ui_text(prop, "Icon", "Override automatic icon of the item");
}

static void api_ui_item_op(FunctionRNA *func)
{
	PropertyRNA *parm;
	parm = RNA_def_string(func, "operator", NULL, 0, "", "Identifier of the operator");
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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in data");
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
		{'h', "HUE", 0, "Hue", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static float node_socket_color_default[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	/* simple layout specifiers */
	func = RNA_def_function(srna, "row", "uiLayoutRow");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func,
	                                "Sub-layout. Items placed in this sublayout are placed next to each other "
	                                "in a row");
	RNA_def_boolean(func, "align", false, "", "Align buttons to each other");
	
	func = RNA_def_function(srna, "column", "uiLayoutColumn");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func,
	                                "Sub-layout. Items placed in this sublayout are placed under each other "
	                                "in a column");
	RNA_def_boolean(func, "align", false, "", "Align buttons to each other");

	func = RNA_def_function(srna, "column_flow", "uiLayoutColumnFlow");
	RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns, 0 is automatic", 0, INT_MAX);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_boolean(func, "align", false, "", "Align buttons to each other");

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
	RNA_def_boolean(func, "align", false, "", "Align buttons to each other");

	/* radial/pie layout */
	func = RNA_def_function(srna, "menu_pie", "uiLayoutRadial");
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Sublayout. Items placed in this sublayout are placed "
	                                "in a radial fashion around the menu center)");

	/* Icon of a rna pointer */
	func = RNA_def_function(srna, "icon", "rna_ui_get_rnaptr_icon");
	parm = RNA_def_int(func, "icon_value", ICON_NONE, 0, INT_MAX, "", "Icon identifier", 0, INT_MAX);
	RNA_def_function_return(func, parm);
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take the icon");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_function_ui_description(func, "Return the custom icon for this data, "
	                                      "use it e.g. to get materials or texture icons");

	/* UI name, description and icon of an enum item */
	func = RNA_def_function(srna, "enum_item_name", "rna_ui_get_enum_name");
	parm = RNA_def_string(func, "name", NULL, 0, "", "UI name of the enum item");
	RNA_def_function_return(func, parm);
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	parm = RNA_def_string(func, "identifier", NULL, 0, "", "Identifier of the enum item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Return the UI name for this enum item");

	func = RNA_def_function(srna, "enum_item_description", "rna_ui_get_enum_description");
	parm = RNA_def_string(func, "description", NULL, 0, "", "UI description of the enum item");
	RNA_def_function_return(func, parm);
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	parm = RNA_def_string(func, "identifier", NULL, 0, "", "Identifier of the enum item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Return the UI description for this enum item");

	func = RNA_def_function(srna, "enum_item_icon", "rna_ui_get_enum_icon");
	parm = RNA_def_int(func, "icon_value", ICON_NONE, 0, INT_MAX, "", "Icon identifier", 0, INT_MAX);
	RNA_def_function_return(func, parm);
	RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	parm = RNA_def_string(func, "identifier", NULL, 0, "", "Identifier of the enum item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_ui_description(func, "Return the icon for this enum item");

	/* items */
	func = RNA_def_function(srna, "prop", "rna_uiItemR");
	RNA_def_function_ui_description(func, "Item. Exposes an RNA item and places it into the layout");
	api_ui_item_rna_common(func);
	api_ui_item_common(func);
	RNA_def_boolean(func, "expand", false, "", "Expand button to show more detail");
	RNA_def_boolean(func, "slider", false, "", "Use slider widget for numeric values");
	RNA_def_boolean(func, "toggle", false, "", "Use toggle widget for boolean values");
	RNA_def_boolean(func, "icon_only", false, "", "Draw only icons in buttons, no text");
	RNA_def_boolean(func, "event", false, "", "Use button to input key events");
	RNA_def_boolean(func, "full_event", false, "", "Use button to input full events including modifiers");
	RNA_def_boolean(func, "emboss", true, "", "Draw the button itself, just the icon/text");
	RNA_def_int(func, "index", -1, -2, INT_MAX, "",
	            "The index of this button, when set a single member of an array can be accessed, "
	            "when set to -1 all array members are used", -2, INT_MAX); /* RNA_NO_INDEX == -1 */
	parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(parm, "Icon Value",
	                         "Override automatic icon of the item "
	                         "(use it e.g. with custom material icons returned by icon()...)");

	func = RNA_def_function(srna, "props_enum", "uiItemsEnumR");
	api_ui_item_rna_common(func);

	func = RNA_def_function(srna, "prop_menu_enum", "rna_uiItemMenuEnumR");
	api_ui_item_rna_common(func);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "prop_enum", "rna_uiItemEnumR_string");
	api_ui_item_rna_common(func);
	parm = RNA_def_string(func, "value", NULL, 0, "", "Enum property value");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "prop_search", "rna_uiItemPointerR");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "search_data", "AnyType", "", "Data from which to take collection to search in");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "search_property", NULL, 0, "", "Identifier of search collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

	func = RNA_def_function(srna, "operator", "rna_uiItemO");
	api_ui_item_op_common(func);
	RNA_def_boolean(func, "emboss", true, "", "Draw the button itself, just the icon/text");
	parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(parm, "Icon Value",
	                         "Override automatic icon of the item "
	                         "(use it e.g. with custom material icons returned by icon()...)");
	parm = RNA_def_pointer(func, "properties", "OperatorProperties", "",
	                       "Operator properties to fill in, return when 'properties' is set to true");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	RNA_def_function_return(func, parm);
	RNA_def_function_ui_description(func, "Item. Places a button into the layout to call an Operator");

	func = RNA_def_function(srna, "operator_enum", "uiItemsEnumO");
	parm = RNA_def_string(func, "operator", NULL, 0, "", "Identifier of the operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "operator_menu_enum", "rna_uiItemMenuEnumO");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_op(func); /* cant use api_ui_item_op_common because property must come right after */
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common(func);

	/* useful in C but not in python */
#if 0

	func = RNA_def_function(srna, "operator_enum_single", "uiItemEnumO_string");
	api_ui_item_op_common(func);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "value", NULL, 0, "", "Enum property value");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "operator_boolean", "uiItemBooleanO");
	api_ui_item_op_common(func);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_boolean(func, "value", false, "", "Value of the property to call the operator with");
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func = RNA_def_function(srna, "operator_int", "uiItemIntO");
	api_ui_item_op_common(func);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "value", 0, INT_MIN, INT_MAX, "",
	                  "Value of the property to call the operator with", INT_MIN, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func = RNA_def_function(srna, "operator_float", "uiItemFloatO");
	api_ui_item_op_common(func);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_float(func, "value", 0, -FLT_MAX, FLT_MAX, "",
	                    "Value of the property to call the operator with", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED); */

	func = RNA_def_function(srna, "operator_string", "uiItemStringO");
	api_ui_item_op_common(func);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in operator");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "value", NULL, 0, "", "Value of the property to call the operator with");
	RNA_def_property_flag(parm, PROP_REQUIRED);
#endif

	func = RNA_def_function(srna, "label", "rna_uiItemL");
	RNA_def_function_ui_description(func, "Item. Display text and/or icon in the layout");
	api_ui_item_common(func);
	parm = RNA_def_property(func, "icon_value", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_ui_text(parm, "Icon Value",
	                         "Override automatic icon of the item "
	                         "(use it e.g. with custom material icons returned by icon()...)");

	func = RNA_def_function(srna, "menu", "rna_uiItemM");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "menu", NULL, 0, "", "Identifier of the menu");
	api_ui_item_common(func);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "separator", "uiItemS");
	RNA_def_function_ui_description(func, "Item. Inserts empty space into the layout between items");

	/* context */
	func = RNA_def_function(srna, "context_pointer_set", "uiLayoutSetContextPointer");
	parm = RNA_def_string(func, "name", NULL, 0, "Name", "Name of entry in the context");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Pointer to put in context");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	
	/* templates */
	func = RNA_def_function(srna, "template_header", "uiTemplateHeader");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "template_ID", "uiTemplateID");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_string(func, "new", NULL, 0, "", "Operator identifier to create a new ID block");
	RNA_def_string(func, "open", NULL, 0, "", "Operator identifier to open a file for creating a new ID block");
	RNA_def_string(func, "unlink", NULL, 0, "", "Operator identifier to unlink the ID block");
	
	func = RNA_def_function(srna, "template_ID_preview", "uiTemplateIDPreview");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_string(func, "new", NULL, 0, "", "Operator identifier to create a new ID block");
	RNA_def_string(func, "open", NULL, 0, "", "Operator identifier to open a file for creating a new ID block");
	RNA_def_string(func, "unlink", NULL, 0, "", "Operator identifier to unlink the ID block");
	RNA_def_int(func, "rows", 0, 0, INT_MAX, "Number of thumbnail preview rows to display", "", 0, INT_MAX);
	RNA_def_int(func, "cols", 0, 0, INT_MAX, "Number of thumbnail preview columns to display", "", 0, INT_MAX);
	
	func = RNA_def_function(srna, "template_any_ID", "rna_uiTemplateAnyID");
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "type_property", NULL, 0, "",
	                      "Identifier of property in data giving the type of the ID-blocks to use");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	api_ui_item_common_text(func);
	
	func = RNA_def_function(srna, "template_path_builder", "rna_uiTemplatePathBuilder");
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "root", "ID", "", "ID-block from which path is evaluated from");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	api_ui_item_common_text(func);
	
	func = RNA_def_function(srna, "template_modifier", "uiTemplateModifier");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Layout . Generates the UI layout for modifiers");
	parm = RNA_def_pointer(func, "data", "Modifier", "", "Modifier data");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "template_constraint", "uiTemplateConstraint");
	RNA_def_function_ui_description(func, "Layout . Generates the UI layout for constraints");
	parm = RNA_def_pointer(func, "data", "Constraint", "", "Constraint data");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "template_preview", "uiTemplatePreview");
	RNA_def_function_ui_description(func, "Item. A preview window for materials, textures, lamps or worlds");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_pointer(func, "id", "ID", "", "ID datablock");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "show_buttons", true, "", "Show preview buttons?");
	RNA_def_pointer(func, "parent", "ID", "", "ID datablock");
	RNA_def_pointer(func, "slot", "TextureSlot", "", "Texture slot");
	RNA_def_string(func, "preview_id", NULL, 0, "",
	               "Identifier of this preview widget, if not set the ID type will be used "
	               "(i.e. all previews of materials without explicit ID will have the same size...)");

	func = RNA_def_function(srna, "template_curve_mapping", "uiTemplateCurveMapping");
	RNA_def_function_ui_description(func, "Item. A curve mapping widget used for e.g falloff curves for lamps");
	api_ui_item_rna_common(func);
	RNA_def_enum(func, "type", curve_type_items, 0, "Type", "Type of curves to display");
	RNA_def_boolean(func, "levels", false, "", "Show black/white levels");
	RNA_def_boolean(func, "brush", false, "", "Show brush options");
	RNA_def_boolean(func, "use_negative_slope", false, "", "Use a negative slope by default");

	func = RNA_def_function(srna, "template_color_ramp", "uiTemplateColorRamp");
	RNA_def_function_ui_description(func, "Item. A color ramp widget");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "expand", false, "", "Expand button to show more detail");
	
	func = RNA_def_function(srna, "template_icon_view", "uiTemplateIconView");
	RNA_def_function_ui_description(func, "Enum. Large widget showing Icon previews");
	api_ui_item_rna_common(func);
	
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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_string(func, "used_layers_property", NULL, 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "active_layer", 0, 0, INT_MAX, "Active Layer", "", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	
	func = RNA_def_function(srna, "template_color_picker", "uiTemplateColorPicker");
	RNA_def_function_ui_description(func, "Item. A color wheel widget to pick colors");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "value_slider", false, "", "Display the value slider to the right of the color wheel");
	RNA_def_boolean(func, "lock", false, "", "Lock the color wheel display to value 1.0 regardless of actual color");
	RNA_def_boolean(func, "lock_luminosity", false, "", "Keep the color at its original vector length");
	RNA_def_boolean(func, "cubic", false, "", "Cubic saturation for picking values close to white");

	func = RNA_def_function(srna, "template_palette", "uiTemplatePalette");
	RNA_def_function_ui_description(func, "Item. A palette used to pick colors");
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "color", 0, "", "Display the colors as colors or values");

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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_boolean(func, "compact", false, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_image_settings", "uiTemplateImageSettings");
	RNA_def_function_ui_description(func, "User interface for setting image format options");
	parm = RNA_def_pointer(func, "image_settings", "ImageFormatSettings", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_boolean(func, "color_management", false, "", "Show color management settings");

	func = RNA_def_function(srna, "template_movieclip", "uiTemplateMovieClip");
	RNA_def_function_ui_description(func, "Item(s). User interface for selecting movie clips and their source paths");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	RNA_def_boolean(func, "compact", false, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_track", "uiTemplateTrack");
	RNA_def_function_ui_description(func, "Item. A movie-track widget to preview tracking image.");
	api_ui_item_rna_common(func);

	func = RNA_def_function(srna, "template_marker", "uiTemplateMarker");
	RNA_def_function_ui_description(func, "Item. A widget to control single marker settings.");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "clip_user", "MovieClipUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "track", "MovieTrackingTrack", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	RNA_def_boolean(func, "compact", false, "", "Use more compact layout");

	func = RNA_def_function(srna, "template_movieclip_information", "uiTemplateMovieclipInformation");
	RNA_def_function_ui_description(func, "Item. Movie clip information data.");
	api_ui_item_rna_common(func);
	parm = RNA_def_pointer(func, "clip_user", "MovieClipUser", "", "");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "template_list", "uiTemplateList");
	RNA_def_function_ui_description(func, "Item. A list widget to display data, e.g. vertexgroups.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	parm = RNA_def_string(func, "listtype_name", NULL, 0, "", "Identifier of the list type to use");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_string(func, "list_id", NULL, 0, "",
	                      "Identifier of this list widget (mandatory when using default \"" UI_UL_DEFAULT_CLASS_NAME
	                      "\" class). "
	                      "If this is set, the uilist gets a custom ID, otherwise it takes the "
	                      "name of the class used to define the uilist (for example, if the "
	                      "class name is \"OBJECT_UL_vgroups\", and list_id is not set by the "
	                      "script, then bl_idname = \"OBJECT_UL_vgroups\")");
	parm = RNA_def_pointer(func, "dataptr", "AnyType", "", "Data from which to take the Collection property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_string(func, "propname", NULL, 0, "", "Identifier of the Collection property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "active_dataptr", "AnyType", "",
	                       "Data from which to take the integer property, index of the active item");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);
	parm = RNA_def_string(func, "active_propname", NULL, 0, "",
	                      "Identifier of the integer property in active_data, index of the active item");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_int(func, "rows", 5, 0, INT_MAX, "", "Default and minimum number of rows to display", 0, INT_MAX);
	RNA_def_int(func, "maxrows", 5, 0, INT_MAX, "", "Default maximum number of rows to display", 0, INT_MAX);
	RNA_def_enum(func, "type", uilist_layout_type_items, UILST_LAYOUT_DEFAULT, "Type", "Type of layout to use");
	RNA_def_int(func, "columns", 9, 0, INT_MAX, "", "Number of items to display per row, for GRID layout", 0, INT_MAX);

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
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR | PROP_NEVER_NULL);

	func = RNA_def_function(srna, "template_component_menu", "uiTemplateComponentMenu");
	RNA_def_function_ui_description(func, "Item. Display expanded property in a popup menu");
	parm = RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_RNAPTR);
	parm = RNA_def_string(func, "property", NULL, 0, "", "Identifier of property in data");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_string(func, "name", NULL, 0, "", "");

	func = RNA_def_function(srna, "introspect", "uiLayoutIntrospect");
	parm = RNA_def_string(func, "string", NULL, 1024 * 1024, "Descr", "DESCR");
	RNA_def_function_return(func, parm);

	/* color management templates */
	func = RNA_def_function(srna, "template_colorspace_settings", "uiTemplateColorspaceSettings");
	RNA_def_function_ui_description(func, "Item. A widget to control input color space settings.");
	api_ui_item_rna_common(func);

	func = RNA_def_function(srna, "template_colormanaged_view_settings", "uiTemplateColormanagedViewSettings");
	RNA_def_function_ui_description(func, "Item. A widget to control color managed view settings settings.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	api_ui_item_rna_common(func);
	/* RNA_def_boolean(func, "show_global_settings", false, "", "Show widgets to control global color management settings"); */

	/* node socket icon */
	func = RNA_def_function(srna, "template_node_socket", "uiTemplateNodeSocket");
	RNA_def_function_ui_description(func, "Node Socket Icon");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	RNA_def_float_array(func, "color", 4, node_socket_color_default, 0.0f, 1.0f, "Color", "", 0.0f, 1.0f);
}

#endif
