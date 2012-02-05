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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/metaball/mball_ops.c
 *  \ingroup edmeta
 */


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_mball.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "BLI_utildefines.h"

#include "mball_intern.h"

void ED_operatortypes_metaball(void)
{
	WM_operatortype_append(MBALL_OT_delete_metaelems);
	WM_operatortype_append(MBALL_OT_duplicate_metaelems);
	
	WM_operatortype_append(MBALL_OT_hide_metaelems);
	WM_operatortype_append(MBALL_OT_reveal_metaelems);
	
	WM_operatortype_append(MBALL_OT_select_all);
	WM_operatortype_append(MBALL_OT_select_random_metaelems);
}

void ED_keymap_metaball(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	keymap= WM_keymap_find(keyconf, "Metaball", 0, 0);
	keymap->poll= ED_operator_editmball;

	WM_keymap_add_item(keymap, "OBJECT_OT_metaball_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MBALL_OT_reveal_metaelems", HKEY, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "MBALL_OT_hide_metaelems", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);
	kmi = WM_keymap_add_item(keymap, "MBALL_OT_hide_metaelems", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);
	
	WM_keymap_add_item(keymap, "MBALL_OT_delete_metaelems", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MBALL_OT_delete_metaelems", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MBALL_OT_duplicate_metaelems", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	kmi = WM_keymap_add_item(keymap, "MBALL_OT_select_all", AKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "MBALL_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	ED_object_generic_keymap(keyconf, keymap, 3);
}

