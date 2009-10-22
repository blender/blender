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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
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

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_object.h"
#include "ED_transform.h"

#include "armature_intern.h"

/* ************************** quick tests **********************************/

/*  XXX This is a quick test operator to print names of all EditBones in context
 *  		that should be removed once tool coding starts...
 */

static int armature_test_exec (bContext *C, wmOperator *op)
{
	printf("EditMode Armature Test: \n");
	
	printf("\tSelected Bones \n");
	CTX_DATA_BEGIN(C, EditBone*, ebone, selected_bones)
	{
		printf("\t\tEditBone '%s' \n", ebone->name);
	}
	CTX_DATA_END;
	
	printf("\tEditable Bones \n");
	CTX_DATA_BEGIN(C, EditBone*, ebone, selected_editable_bones) 
	{
		printf("\t\tEditBone '%s' \n", ebone->name);
	}
	CTX_DATA_END;
	
	printf("\tActive Bone \n");
	{
		EditBone *ebone= CTX_data_active_bone(C);
		if (ebone) printf("\t\tEditBone '%s' \n", ebone->name);
		else printf("\t\t<None> \n");
	}
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_test(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Test Context";
	ot->idname= "ARMATURE_OT_test";
	
	/* api callbacks */
	ot->exec= armature_test_exec;
}

/* ************************** registration **********************************/

/* Both operators ARMATURE_OT_xxx and POSE_OT_xxx here */
void ED_operatortypes_armature(void)
{
	/* EDIT ARMATURE */
	WM_operatortype_append(ARMATURE_OT_bone_primitive_add);
	
	WM_operatortype_append(ARMATURE_OT_align);
	WM_operatortype_append(ARMATURE_OT_calculate_roll);
	WM_operatortype_append(ARMATURE_OT_switch_direction);
	WM_operatortype_append(ARMATURE_OT_subdivs);
	WM_operatortype_append(ARMATURE_OT_subdivide_simple);
	WM_operatortype_append(ARMATURE_OT_subdivide_multi);
	
	WM_operatortype_append(ARMATURE_OT_parent_set);
	WM_operatortype_append(ARMATURE_OT_parent_clear);
	
	WM_operatortype_append(ARMATURE_OT_select_all_toggle);
	WM_operatortype_append(ARMATURE_OT_select_inverse);
	WM_operatortype_append(ARMATURE_OT_select_hierarchy);
	WM_operatortype_append(ARMATURE_OT_select_linked);

	WM_operatortype_append(ARMATURE_OT_delete);
	WM_operatortype_append(ARMATURE_OT_duplicate);
	WM_operatortype_append(ARMATURE_OT_extrude);
	WM_operatortype_append(ARMATURE_OT_click_extrude);
	WM_operatortype_append(ARMATURE_OT_fill);
	WM_operatortype_append(ARMATURE_OT_merge);
	WM_operatortype_append(ARMATURE_OT_separate);
	
	WM_operatortype_append(ARMATURE_OT_autoside_names);
	WM_operatortype_append(ARMATURE_OT_flip_names);
	
	WM_operatortype_append(ARMATURE_OT_flags_set);
	
	WM_operatortype_append(ARMATURE_OT_armature_layers);
	WM_operatortype_append(ARMATURE_OT_bone_layers);

	/* SKETCH */	
	WM_operatortype_append(SKETCH_OT_gesture);
	WM_operatortype_append(SKETCH_OT_delete);
	WM_operatortype_append(SKETCH_OT_draw_stroke);
	WM_operatortype_append(SKETCH_OT_draw_preview);
	WM_operatortype_append(SKETCH_OT_finish_stroke);
	WM_operatortype_append(SKETCH_OT_cancel_stroke);
	WM_operatortype_append(SKETCH_OT_select);

	/* POSE */
	WM_operatortype_append(POSE_OT_hide);
	WM_operatortype_append(POSE_OT_reveal);
	
	WM_operatortype_append(POSE_OT_apply);
	
	WM_operatortype_append(POSE_OT_rot_clear);
	WM_operatortype_append(POSE_OT_loc_clear);
	WM_operatortype_append(POSE_OT_scale_clear);
	
	WM_operatortype_append(POSE_OT_copy);
	WM_operatortype_append(POSE_OT_paste);
	
	WM_operatortype_append(POSE_OT_select_all_toggle);
	WM_operatortype_append(POSE_OT_select_inverse);

	WM_operatortype_append(POSE_OT_select_parent);
	WM_operatortype_append(POSE_OT_select_hierarchy);
	WM_operatortype_append(POSE_OT_select_linked);
	WM_operatortype_append(POSE_OT_select_constraint_target);
	
	WM_operatortype_append(POSE_OT_groups_menu);
	WM_operatortype_append(POSE_OT_group_add);
	WM_operatortype_append(POSE_OT_group_remove);
	WM_operatortype_append(POSE_OT_group_assign);
	WM_operatortype_append(POSE_OT_group_unassign);
	
	WM_operatortype_append(POSE_OT_paths_calculate);
	WM_operatortype_append(POSE_OT_paths_clear);
	
	WM_operatortype_append(POSE_OT_autoside_names);
	WM_operatortype_append(POSE_OT_flip_names);
	
	WM_operatortype_append(POSE_OT_flags_set);
	
	WM_operatortype_append(POSE_OT_armature_layers);
	WM_operatortype_append(POSE_OT_bone_layers);
	
	/* POSELIB */
	WM_operatortype_append(POSELIB_OT_browse_interactive);
	
	WM_operatortype_append(POSELIB_OT_pose_add);
	WM_operatortype_append(POSELIB_OT_pose_remove);
	WM_operatortype_append(POSELIB_OT_pose_rename);
	
	/* POSE SLIDING */
	WM_operatortype_append(POSE_OT_push);
	WM_operatortype_append(POSE_OT_relax);
	WM_operatortype_append(POSE_OT_breakdown);
	
	/* TESTS */
	WM_operatortype_append(ARMATURE_OT_test); // XXX temp test for context iterators... to be removed
}

void ED_keymap_armature(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Armature ------------------------ */
	keymap= WM_keymap_find(keyconf, "Armature", 0, 0);
	keymap->poll= ED_operator_editarmature;
	
	/* only set in editmode armature, by space_view3d listener */
//	WM_keymap_add_item(keymap, "ARMATURE_OT_hide", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_align", AKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_calculate_roll", NKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_switch_direction", FKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_bone_primitive_add", AKEY, KM_PRESS, KM_SHIFT, 0);
		/* only the menu-version of subdivide is registered in keymaps for now */
	WM_keymap_add_item(keymap, "ARMATURE_OT_subdivs", SKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_parent_clear", PKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_test", TKEY, KM_PRESS, 0, 0);  // XXX temp test for context iterators... to be removed

	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_PARENT);
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_PARENT);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_CHILD);
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_CHILD);
		RNA_boolean_set(kmi->ptr, "extend", 1);

	WM_keymap_add_item(keymap, "ARMATURE_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_extrude", EKEY, KM_PRESS, 0, 0);
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_extrude", EKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "forked", 1);
	WM_keymap_add_item(keymap, "ARMATURE_OT_click_extrude", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_fill", FKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_merge", MKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "ARMATURE_OT_separate", PKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	
		/* set flags */
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_flags_set", WKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "mode", 2); // toggle
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_flags_set", WKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "mode", 1); // enable
	kmi= WM_keymap_add_item(keymap, "ARMATURE_OT_flags_set", WKEY, KM_PRESS, KM_ALT, 0);
		RNA_enum_set(kmi->ptr, "mode", 0); // clear
		
		/* armature/bone layers */
	WM_keymap_add_item(keymap, "ARMATURE_OT_armature_layers", MKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "ARMATURE_OT_bone_layers", MKEY, KM_PRESS, 0, 0);
	
		/* special transforms: */
		/* 	1) envelope/b-bone size */
	kmi= WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0);
		RNA_enum_set(kmi->ptr, "mode", TFM_BONESIZE);
		/* 	2) set roll */
	kmi= WM_keymap_add_item(keymap, "TFM_OT_transform", RKEY, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "mode", TFM_BONE_ROLL);
	
	/* Armature -> Etch-A-Ton ------------------------ */
	WM_keymap_add_item(keymap, "SKETCH_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SKETCH_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SKETCH_OT_finish_stroke", SELECTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SKETCH_OT_cancel_stroke", ESCKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "SKETCH_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);

	/* sketch poll checks mode */	
	WM_keymap_add_item(keymap, "SKETCH_OT_gesture", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "SKETCH_OT_draw_stroke", ACTIONMOUSE, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "SKETCH_OT_draw_stroke", ACTIONMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "snap", 1);
	WM_keymap_add_item(keymap, "SKETCH_OT_draw_preview", MOUSEMOVE, KM_ANY, 0, 0);
	kmi = WM_keymap_add_item(keymap, "SKETCH_OT_draw_preview", MOUSEMOVE, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "snap", 1);

	/* Pose ------------------------ */
	/* only set in posemode, by space_view3d listener */
	keymap= WM_keymap_find(keyconf, "Pose", 0, 0);
	keymap->poll= ED_operator_posemode;
	
	// XXX: set parent is object-based operator, but it should also be available here...
	WM_keymap_add_item(keymap, "OBJECT_OT_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "POSE_OT_hide", HKEY, KM_PRESS, 0, 0);
	kmi= WM_keymap_add_item(keymap, "POSE_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "unselected", 1);
	WM_keymap_add_item(keymap, "POSE_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "POSE_OT_apply", AKEY, KM_PRESS, KM_CTRL, 0);
	
	// TODO: clear pose
	WM_keymap_add_item(keymap, "POSE_OT_rot_clear", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_loc_clear", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_scale_clear", SKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "POSE_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "POSE_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	kmi= WM_keymap_add_item(keymap, "POSE_OT_paste", VKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "flipped", 1);
	
	WM_keymap_add_item(keymap, "POSE_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "POSE_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "POSE_OT_select_parent", PKEY, KM_PRESS, KM_SHIFT, 0);

	kmi= WM_keymap_add_item(keymap, "POSE_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_PARENT);
	kmi= WM_keymap_add_item(keymap, "POSE_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_PARENT);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	
	kmi= WM_keymap_add_item(keymap, "POSE_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_CHILD);
	kmi= WM_keymap_add_item(keymap, "POSE_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "direction", BONE_SELECT_CHILD);
		RNA_boolean_set(kmi->ptr, "extend", 1);

	WM_keymap_add_item(keymap, "POSE_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "POSE_OT_constraint_add_with_targets", CKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_constraints_clear", CKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_ik_add", IKEY, KM_PRESS, /*KM_CTRL|*/KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_ik_clear", IKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "POSE_OT_groups_menu", GKEY, KM_PRESS, KM_CTRL, 0);
	
		/* set flags */
	kmi= WM_keymap_add_item(keymap, "POSE_OT_flags_set", WKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "mode", 2); // toggle
	kmi= WM_keymap_add_item(keymap, "POSE_OT_flags_set", WKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		RNA_enum_set(kmi->ptr, "mode", 1); // enable
	kmi= WM_keymap_add_item(keymap, "POSE_OT_flags_set", WKEY, KM_PRESS, KM_ALT, 0);
		RNA_enum_set(kmi->ptr, "mode", 0); // clear
		
		/* armature/bone layers */
	WM_keymap_add_item(keymap, "POSE_OT_armature_layers", MKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_bone_layers", MKEY, KM_PRESS, 0, 0);
	
		/* special transforms: */
		/* 	1) envelope/b-bone size */
	kmi= WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_ALT, 0);
		RNA_enum_set(kmi->ptr, "mode", TFM_BONESIZE);
	
	// XXX this should probably be in screen instead... here for testing purposes in the meantime... - Aligorith
	WM_keymap_verify_item(keymap, "ANIM_OT_insert_keyframe_menu", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_delete_keyframe_v3d", IKEY, KM_PRESS, KM_ALT, 0);
	
	/* Pose -> PoseLib ------------- */
	/* only set in posemode, by space_view3d listener */
	WM_keymap_add_item(keymap, "POSELIB_OT_browse_interactive", LKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "POSELIB_OT_pose_add", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "POSELIB_OT_pose_remove", LKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "POSELIB_OT_pose_rename", LKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	
	/* Pose -> Pose Sliding ------------- */
	/* only set in posemode, by space_view3d listener */
	WM_keymap_add_item(keymap, "POSE_OT_push", EKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "POSE_OT_relax", EKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "POSE_OT_breakdown", EKEY, KM_PRESS, KM_SHIFT, 0);
}

