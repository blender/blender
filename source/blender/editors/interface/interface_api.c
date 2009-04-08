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

void RNA_api_ui_layout(StructRNA *srna)
{
	FunctionRNA *func;

	/* templates */
	func= RNA_def_function(srna, "template_column", "uiTemplateColumn");
	func= RNA_def_function(srna, "template_left_right", "uiTemplateLeftRight");
	func= RNA_def_function(srna, "template_stack", "uiTemplateStack");

	func= RNA_def_function(srna, "template_header_menus", "uiTemplateHeaderMenus");
	func= RNA_def_function(srna, "template_header_buttons", "uiTemplateHeaderButtons");
	//func= RNA_def_function(srna, "template_header_ID", "uiTemplateHeaderID");

	/* items */
	func= RNA_def_function(srna, "itemR", "uiItemR");
	RNA_def_int(func, "slot", 0, 0, 5, "", "", 0, 5);
	RNA_def_string(func, "name", "", 0, "", "");
	RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_pointer(func, "data", "AnyType", "", "");
	RNA_def_string(func, "property", "", 0, "", "");

	func= RNA_def_function(srna, "itemO", "uiItemO");
	RNA_def_int(func, "slot", 0, 0, 5, "", "", 0, 5);
	RNA_def_string(func, "name", "", 0, "", "");
	RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "", 0, INT_MAX);
	RNA_def_string(func, "operator", "", 0, "", "");

	func= RNA_def_function(srna, "itemL", "uiItemLabel");
	RNA_def_int(func, "slot", 0, 0, 5, "", "", 0, 5);
	RNA_def_string(func, "name", "", 0, "", "");
	RNA_def_int(func, "icon", 0, 0, INT_MAX, "", "", 0, INT_MAX);
}

