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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"

#include "outliner_intern.h"

/* ************************** registration **********************************/



void outliner_operatortypes(void)
{
	WM_operatortype_append(OUTLINER_OT_activate_click);
	
	WM_operatortype_append(OUTLINER_OT_keyingset_add_selected);
	WM_operatortype_append(OUTLINER_OT_keyingset_remove_selected);
}

void outliner_keymap(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Outliner", SPACE_OUTLINER, 0);
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_activate_click", LEFTMOUSE, KM_PRESS, 0, 0);
	
	/* keying sets - only for databrowse */
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_add_selected", KKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_remove_selected", KKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "ANIM_OT_insert_keyframe", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_delete_keyframe", IKEY, KM_PRESS, KM_ALT, 0);
}

