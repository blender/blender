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
#include "RNA_enum_types.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

static wmKeyMapItem *rna_KeyMap_add_item(wmKeyMap *km, char *idname, int type, int value, int shift, int ctrl, int alt, int oskey, int keymodifier)
{
	int modifier= 0;

	if(shift) modifier |= KM_SHIFT;
	if(ctrl) modifier |= KM_CTRL;
	if(alt) modifier |= KM_ALT;
	if(oskey) modifier |= KM_OSKEY;

	return WM_keymap_add_item(km, idname, type, value, modifier, keymodifier);
}

#else

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_fileselect", "WM_event_add_fileselect");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Show up the file selector.");
	parm= RNA_def_pointer(func, "operator", "Operator", "", "Operator to call.");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_keyconfig", "WM_keyconfig_add");
	parm= RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "keyconfig", "KeyConfig", "Key Configuration", "Added key configuration.");
	RNA_def_function_return(func, parm);
}

void RNA_api_keyconfig(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_keymap", "WM_keymap_find");
	parm= RNA_def_string(func, "name", "", 0, "Name", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_enum(func, "space_type", space_type_items, SPACE_EMPTY, "Space Type", "");
	RNA_def_enum(func, "region_type", region_type_items, RGN_TYPE_WINDOW, "Region Type", "");
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "Added key map.");
	RNA_def_function_return(func, parm);
}

void RNA_api_keymap(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "add_item", "rna_KeyMap_add_item");
	parm= RNA_def_string(func, "idname", "", 0, "Operator Identifier", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "type", event_type_items, 0, "Type", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_enum(func, "value", event_value_items, 0, "Value", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_boolean(func, "shift", 0, "Shift", "");
	RNA_def_boolean(func, "ctrl", 0, "Ctrl", "");
	RNA_def_boolean(func, "alt", 0, "Alt", "");
	RNA_def_boolean(func, "oskey", 0, "OS Key", "");
	RNA_def_enum(func, "key_modifier", event_type_items, 0, "Key Modifier", "");
	parm= RNA_def_pointer(func, "item", "KeyMapItem", "Item", "Added key map item.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "remove_item", "WM_keymap_remove_item");
	parm= RNA_def_pointer(func, "item", "KeyMapItem", "Item", "");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "copy_to_user", "WM_keymap_copy_to_user");
	parm= RNA_def_pointer(func, "keymap", "KeyMap", "Key Map", "User editable key map.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "restore_to_default", "WM_keymap_restore_to_default");
}

#endif

