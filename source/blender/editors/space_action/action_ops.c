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
#include "DNA_anim_types.h"
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

#include "ED_transform.h"

#include "action_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** registration - operator types **********************************/

void action_operatortypes(void)
{
	/* keyframes */
		/* selection */
	WM_operatortype_append(ACTION_OT_clickselect);
	WM_operatortype_append(ACTION_OT_select_all_toggle);
	WM_operatortype_append(ACTION_OT_select_border);
	WM_operatortype_append(ACTION_OT_select_column);
	WM_operatortype_append(ACTION_OT_select_more);
	WM_operatortype_append(ACTION_OT_select_less);
	
		/* editing */
	WM_operatortype_append(ACTION_OT_snap);
	WM_operatortype_append(ACTION_OT_mirror);
	WM_operatortype_append(ACTION_OT_frame_jump);
	WM_operatortype_append(ACTION_OT_handle_type);
	WM_operatortype_append(ACTION_OT_interpolation_type);
	WM_operatortype_append(ACTION_OT_extrapolation_type);
	WM_operatortype_append(ACTION_OT_keyframe_type);
	WM_operatortype_append(ACTION_OT_sample);
	WM_operatortype_append(ACTION_OT_clean);
	WM_operatortype_append(ACTION_OT_delete);
	WM_operatortype_append(ACTION_OT_duplicate);
	WM_operatortype_append(ACTION_OT_keyframe_insert);
	WM_operatortype_append(ACTION_OT_copy);
	WM_operatortype_append(ACTION_OT_paste);
	WM_operatortype_append(ACTION_OT_new);
	
	WM_operatortype_append(ACTION_OT_previewrange_set);
	WM_operatortype_append(ACTION_OT_view_all);
}

/* ************************** registration - keymaps **********************************/

static void action_keymap_keyframes (wmKeyConfig *keyconf, wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	
	/* action_select.c - selection tools */
		/* click-select */
	WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	kmi= WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
		RNA_boolean_set(kmi->ptr, "column", 1);
	kmi= WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	kmi= WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT|KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
		RNA_boolean_set(kmi->ptr, "column", 1);
	kmi= WM_keymap_add_item(keymap, "ACTION_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "left_right", ACTKEYS_LRSEL_TEST);
	
		/* deselect all */
	WM_keymap_add_item(keymap, "ACTION_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ACTION_OT_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "ACTION_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ACTION_OT_select_border", BKEY, KM_PRESS, KM_ALT, 0)->ptr, "axis_range", 1);
	
		/* column select */
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, 0, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_KEYS);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_CFRA);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_COLUMN);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACTION_OT_select_column", KKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_BETWEEN);
	
		/* select more/less */
	WM_keymap_add_item(keymap, "ACTION_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	
	
	/* action_edit.c */
		/* snap - current frame to selected keys */
		// TODO: maybe since this is called jump, we're better to have it on <something>-J?
	WM_keymap_add_item(keymap, "ACTION_OT_frame_jump", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		
		/* menu + single-step transform */
	WM_keymap_add_item(keymap, "ACTION_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_mirror", MKEY, KM_PRESS, KM_SHIFT, 0);
	
		/* menu + set setting */
	WM_keymap_add_item(keymap, "ACTION_OT_handle_type", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_interpolation_type", TKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_extrapolation_type", EKEY, KM_PRESS, KM_SHIFT, 0); 
	WM_keymap_add_item(keymap, "ACTION_OT_keyframe_type", RKEY, KM_PRESS, 0, 0); 
	
		/* destructive */
	WM_keymap_add_item(keymap, "ACTION_OT_clean", OKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_sample", OKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "ACTION_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "ACTION_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	
		/* copy/paste */
	WM_keymap_add_item(keymap, "ACTION_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	
		/* auto-set range */
	WM_keymap_add_item(keymap, "ACTION_OT_previewrange_set", PKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "ACTION_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	
	/* animation module */
		/* channels list 
		 * NOTE: these operators were originally for the channels list, but are added here too for convenience...
		 */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* transform system */
	transform_keymap_for_space(keyconf, keymap, SPACE_ACTION);
}

/* --------------- */

void action_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	/* channels */
	/* Channels are not directly handled by the Action Editor module, but are inherited from the Animation module. 
	 * All the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as these
	 * are all used for the Graph-Editor too.
	 */
	
	/* keyframes */
	keymap= WM_keymap_find(keyconf, "Dopesheet", SPACE_ACTION, 0);
	action_keymap_keyframes(keyconf, keymap);
}

