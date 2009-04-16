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

	/* simple layout specifiers */
	func= RNA_def_function(srna, "row", "uiLayoutRow");
	func= RNA_def_function(srna, "column", "uiLayoutColumn");
	func= RNA_def_function(srna, "column_flow", "uiLayoutColumnFlow");
	parm= RNA_def_int(func, "columns", 0, 0, INT_MAX, "", "Number of columns, 0 is automatic.", 0, INT_MAX);

	/* box layout */
	func= RNA_def_function(srna, "box", "uiLayoutBox");
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	/* split layout */
	func= RNA_def_function(srna, "split", "uiLayoutSplit");
	parm= RNA_def_int(func, "number", 2, 0, INT_MAX, "", "Number of splits.", 0, INT_MAX);
	parm= RNA_def_boolean(func, "lr", 0, "", "LR.");

	/* sub layout */
	func= RNA_def_function(srna, "sub", "uiLayoutSub");
	parm= RNA_def_int(func, "n", 0, 0, INT_MAX, "", "Index of sub-layout.", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "layout", "UILayout", "", "Sub-layout to put items in.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "template_header_menus", "uiTemplateHeaderMenus");
	//func= RNA_def_function(srna, "template_header_ID", "uiTemplateHeaderID");

	/* items */
	func= RNA_def_function(srna, "itemR", "uiItemR");
	api_ui_item_common(func);
	parm= RNA_def_pointer(func, "data", "AnyType", "", "Data from which to take property.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_string(func, "property", "", 0, "", "Identifier of property in data.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "expand", 0, "", "Expand button to show more detail.");

	func= RNA_def_function(srna, "itemO", "uiItemO");
	api_ui_item_common(func);
	parm= RNA_def_string(func, "operator", "", 0, "", "Identifier of the operator.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "itemL", "uiItemL");
	api_ui_item_common(func);
}

