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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include


/* ************************** registration - operator types **********************************/

void nla_operatortypes(void)
{
	/* channels */
	WM_operatortype_append(NLA_OT_channels_click);
	
	/* select */
	// ...
}

/* ************************** registration - keymaps **********************************/

static void nla_keymap_channels (wmWindowManager *wm, ListBase *keymap)
{
	/* NLA-specific (different to standard channels keymap) -------------------------- */
	/* selection */
		/* click-select */
		// XXX for now, only leftmouse.... 
	WM_keymap_add_item(keymap, "NLA_OT_channels_click", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "NLA_OT_channels_click", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	
	/* General Animation Channels keymap (see anim_channels.c) ----------------------- */
		/* deselect all */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_select_border", BKEY, KM_PRESS, 0, 0);
	
	/* settings */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_toggle", WKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_enable", WKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_setting_disable", WKEY, KM_PRESS, KM_ALT, 0);
	
	/* settings - specialised hotkeys */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* expand/collapse */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, 0, 0);
	
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_expand", PADPLUSKEY, KM_PRESS, KM_CTRL, 0)->ptr, "all", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ANIM_OT_channels_collapse", PADMINUS, KM_PRESS, KM_CTRL, 0)->ptr, "all", 1);
}

static void nla_keymap_main (wmWindowManager *wm, ListBase *keymap)
{
	//wmKeymapItem *kmi;
	
	
	
	/* transform system */
	//transform_keymap_for_space(wm, keymap, SPACE_NLA);
}

/* --------------- */

void nla_keymap(wmWindowManager *wm)
{
	ListBase *keymap;
	
	/* channels */
	/* Channels are not directly handled by the NLA Editor module, but are inherited from the Animation module. 
	 * Most of the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as there
	 * are many similarities with the other Animation Editors.
	 *
	 * However, those operations which involve clicking on channels and/or the placement of them in the view are implemented here instead
	 */
	keymap= WM_keymap_listbase(wm, "NLA Channels", SPACE_NLA, 0);
	nla_keymap_channels(wm, keymap);
	
	/* data */
	keymap= WM_keymap_listbase(wm, "NLA Data", SPACE_NLA, 0);
	nla_keymap_main(wm, keymap);
}

