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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "mball_intern.h"

void ED_operatortypes_metaball(void)
{
	WM_operatortype_append(MBALL_OT_delete_metaelems);
	WM_operatortype_append(MBALL_OT_duplicate_metaelems);
	
	WM_operatortype_append(MBALL_OT_hide_metaelems);
	WM_operatortype_append(MBALL_OT_reveal_metaelems);
	
	WM_operatortype_append(MBALL_OT_select_deselect_all_metaelems);
	WM_operatortype_append(MBALL_OT_select_inverse_metaelems);
	WM_operatortype_append(MBALL_OT_select_random_metaelems);
}

void ED_keymap_metaball(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Metaball", 0, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_metaball_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MBALL_OT_reveal_metaelems", HKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "MBALL_OT_hide_metaelems", HKEY, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "MBALL_OT_hide_metaelems", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);
	
	WM_keymap_add_item(keymap, "MBALL_OT_delete_metaelems", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MBALL_OT_duplicate_metaelems", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "MBALL_OT_select_deselect_all_metaelems", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "MBALL_OT_select_inverse_metaelems", IKEY, KM_PRESS, KM_CTRL, 0);
}

