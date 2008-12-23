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
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "action_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"


/* ************************** registration - operator types **********************************/

void action_operatortypes(void)
{
	/* channels */
	
	/* keyframes */
	WM_operatortype_append(ED_ACT_OT_keyframes_clickselect);
}

/* ************************** registration - keymaps **********************************/

static void action_keymap_keyframes (ListBase *keymap)
{
		/* click-select */
	WM_keymap_add_item(keymap, "ED_ACT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ED_ACT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend_select", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ED_ACT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "left_right", 1);
}

/* --------------- */

void action_keymap(wmWindowManager *wm)
{
	ListBase *keymap;
	
	/* channels */
	keymap= WM_keymap_listbase(wm, "Action_Channels", SPACE_ACTION, 0);
	
	/* keyframes */
	keymap= WM_keymap_listbase(wm, "Action_Keys", SPACE_ACTION, 0);
	action_keymap_keyframes(keymap);
}

