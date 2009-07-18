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


/* ------------- */

#include "BLI_dlrbTree.h"
#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"
#include "DNA_object_types.h"

static int act_drawtree_test_exec (bContext *C, wmOperator *op)
{
	bAnimContext ac;
	bDopeSheet *ads;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter, items;
	
	ANIM_animdata_get_context(C, &ac);
	ads= ac.data;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	if (items) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			AnimData *adt= BKE_animdata_from_id(ale->id);
			
			
			if (ale->type == ANIMTYPE_GROUP) {
				DLRBT_Tree keys;
				ActKeyColumn *ak;
				
				BLI_dlrbTree_init(&keys);
				
					agroup_to_keylist(adt, ale->data, &keys, NULL);
				
				BLI_dlrbTree_linkedlist_sync(&keys);
				
					printf("printing sorted list of object keyframes --------------- \n");
					for (ak= keys.first; ak; ak= ak->next) {
						printf("\t%p (%f) | L:%p R:%p P:%p \n", ak, ak->cfra, ak->left, ak->right, ak->parent);
					}	

					printf("printing tree ---------------- \n");
					for (ak= keys.root; ak; ak= ak->next) {
						printf("\t%p (%f) | L:%p R:%p P:%p \n", ak, ak->cfra, ak->left, ak->right, ak->parent);
					}
				
				BLI_dlrbTree_free(&keys);
				
				break;
			}
		}
		
		BLI_freelistN(&anim_data);
	}
}

void ACT_OT_test (wmOperatorType *ot)
{
	ot->idname= "ACT_OT_test";
	
	ot->exec= act_drawtree_test_exec;
}

/* ************************** registration - operator types **********************************/

void action_operatortypes(void)
{
	/* keyframes */
		/* selection */
	WM_operatortype_append(ACT_OT_clickselect);
	WM_operatortype_append(ACT_OT_select_all_toggle);
	WM_operatortype_append(ACT_OT_select_border);
	WM_operatortype_append(ACT_OT_select_column);
	
		/* editing */
	WM_operatortype_append(ACT_OT_snap);
	WM_operatortype_append(ACT_OT_mirror);
	WM_operatortype_append(ACT_OT_frame_jump);
	WM_operatortype_append(ACT_OT_handle_type);
	WM_operatortype_append(ACT_OT_interpolation_type);
	WM_operatortype_append(ACT_OT_extrapolation_type);
	WM_operatortype_append(ACT_OT_sample);
	WM_operatortype_append(ACT_OT_clean);
	WM_operatortype_append(ACT_OT_delete);
	WM_operatortype_append(ACT_OT_duplicate);
	WM_operatortype_append(ACT_OT_insert_keyframe);
	WM_operatortype_append(ACT_OT_copy);
	WM_operatortype_append(ACT_OT_paste);
	
	WM_operatortype_append(ACT_OT_previewrange_set);
	WM_operatortype_append(ACT_OT_view_all);
	
	// test
	WM_operatortype_append(ACT_OT_test);
}

/* ************************** registration - keymaps **********************************/

static void action_keymap_keyframes (wmWindowManager *wm, ListBase *keymap)
{
	wmKeymapItem *kmi;
	
	/* action_select.c - selection tools */
		/* click-select */
	WM_keymap_add_item(keymap, "ACT_OT_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	kmi= WM_keymap_add_item(keymap, "ACT_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
		RNA_boolean_set(kmi->ptr, "column", 1);
	kmi= WM_keymap_add_item(keymap, "ACT_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	kmi= WM_keymap_add_item(keymap, "ACT_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT|KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
		RNA_boolean_set(kmi->ptr, "column", 1);
	kmi= WM_keymap_add_item(keymap, "ACT_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "left_right", ACTKEYS_LRSEL_TEST);
	
		/* deselect all */
	WM_keymap_add_item(keymap, "ACT_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ACT_OT_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "ACT_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "ACT_OT_select_border", BKEY, KM_PRESS, KM_ALT, 0)->ptr, "axis_range", 1);
	
		/* column select */
	RNA_enum_set(WM_keymap_add_item(keymap, "ACT_OT_select_column", KKEY, KM_PRESS, 0, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_KEYS);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACT_OT_select_column", KKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_CFRA);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACT_OT_select_column", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_COLUMN);
	RNA_enum_set(WM_keymap_add_item(keymap, "ACT_OT_select_column", KKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", ACTKEYS_COLUMNSEL_MARKERS_BETWEEN);
	
	/* action_edit.c */
		/* snap - current frame to selected keys */
		// TODO: maybe since this is called jump, we're better to have it on <something>-J?
	WM_keymap_add_item(keymap, "ACT_OT_frame_jump", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		
		/* menu + single-step transform */
	WM_keymap_add_item(keymap, "ACT_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACT_OT_mirror", MKEY, KM_PRESS, KM_SHIFT, 0);
	
		/* menu + set setting */
	WM_keymap_add_item(keymap, "ACT_OT_handle_type", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACT_OT_interpolation_type", TKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACT_OT_extrapolation_type", EKEY, KM_PRESS, KM_SHIFT, 0); 
	
		/* destructive */
	WM_keymap_add_item(keymap, "ACT_OT_clean", OKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACT_OT_sample", OKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "ACT_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ACT_OT_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "ACT_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ACT_OT_insert_keyframe", IKEY, KM_PRESS, 0, 0);
	
		/* copy/paste */
	WM_keymap_add_item(keymap, "ACT_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ACT_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	
		/* auto-set range */
	WM_keymap_add_item(keymap, "ACT_OT_previewrange_set", PKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "ACT_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	
	/* transform system */
	transform_keymap_for_space(wm, keymap, SPACE_ACTION);
	
		/* test */
	WM_keymap_add_item(keymap, "ACT_OT_test", QKEY, KM_PRESS, 0, 0);
}

/* --------------- */

void action_keymap(wmWindowManager *wm)
{
	ListBase *keymap;
	
	/* channels */
	/* Channels are not directly handled by the Action Editor module, but are inherited from the Animation module. 
	 * All the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as these
	 * are all used for the IPO-Editor too.
	 */
	
	/* keyframes */
	keymap= WM_keymap_listbase(wm, "Action_Keys", SPACE_ACTION, 0);
	action_keymap_keyframes(wm, keymap);
}

