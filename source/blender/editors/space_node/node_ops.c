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
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h"

void node_operatortypes(void)
{
	WM_operatortype_append(NODE_OT_select);
	WM_operatortype_append(NODE_OT_extend_select);
}

void node_keymap(struct wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Node", SPACE_NODE, 0);
	
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, 0, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_extend_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
}
