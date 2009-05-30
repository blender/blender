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

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

#include "nla_intern.h"	// own include


/* ************************** registration - operator types **********************************/

void nla_operatortypes(void)
{
	//WM_operatortype_append();
	
}

/* ************************** registration - keymaps **********************************/

static void nla_keymap_channels (wmWindowManager *wm, ListBase *keymap)
{
	//wmKeymapItem *kmi;
	
	
	
	/* transform system */
	//transform_keymap_for_space(wm, keymap, SPACE_NLA);
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
	keymap= WM_keymap_listbase(wm, "NLA_Channels", SPACE_NLA, 0);
	nla_keymap_channels(wm, keymap);
	
	/* data */
	keymap= WM_keymap_listbase(wm, "NLA_Data", SPACE_NLA, 0);
	nla_keymap_main(wm, keymap);
}

