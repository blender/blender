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

static void api_ui_item_common(FunctionRNA *func)
{
	RNA_def_string(func, "text", "", 0, "", "Override automatic text of the item.");
	RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "Override automatic icon of the item.", 0, INT_MAX);
}

void RNA_api_ui_layout(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static EnumPropertyItem slot_items[]= {
		{0, "DEFAULT", "Default", ""},
		{UI_TSLOT_COLUMN_1, "COLUMN_1", "Column 1", ""},
		{UI_TSLOT_COLUMN_2, "COLUMN_2", "Column 2", ""},
		{UI_TSLOT_COLUMN_3, "COLUMN_3", "Column 3", ""},
		{UI_TSLOT_COLUMN_4, "COLUMN_4", "Column 4", ""},
		{UI_TSLOT_COLUMN_5, "COLUMN_5", "Column 5", ""},
		{UI_TSLOT_LR_LEFT, "LEFT", "Left", ""},
		{UI_TSLOT_LR_RIGHT, "RIGHT", "Right", ""},
		{0, NULL, NULL, NULL}
	};

	/* templates */
	func= RNA_def_function(srna, "template_row", "uiTemplateRow");
	func= RNA_def_function(srna, "template_column", "uiTemplateColumn");
	func= RNA_def_function(srna, "template_left_right", "uiTemplateLeftRight");

	func= RNA_def_function(srna, "template_column_flow", "uiTemplateColumnFlow");
	parm= RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns.", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "template_stack", "uiTemplateStack");
	parm= RNA_def_pointer(func, "sub_layout", "UILayout", "", "Sub-layout to put stack items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_header_menus", "uiTemplateHeaderMenus");
	func= RNA_def_function(srna, "template_header_buttons", "uiTemplateHeaderButtons");
	//func= RNA_def_function(srna, "template_header_ID", "uiTemplateHeaderID");

	func= RNA_def_function(srna, "template_slot", "uiTemplateSlot");
	parm= RNA_def_enum(func, "slot", slot_items, 0, "", "Where in the template to put the following items.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* items */
	func= RNA_def_function(srna, "itemR", "uiItemR");
	api_ui_item_common(func);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemO", "uiItemO");
	api_ui_item_common(func);
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemL", "uiItemL");
	api_ui_item_common(func);
}

