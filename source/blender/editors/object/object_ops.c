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

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "object_intern.h"


/* ************************** registration **********************************/


void ED_operatortypes_object(void)
{
	WM_operatortype_append(OBJECT_OT_location_clear);
	WM_operatortype_append(OBJECT_OT_rotation_clear);
	WM_operatortype_append(OBJECT_OT_scale_clear);
	WM_operatortype_append(OBJECT_OT_origin_clear);
	WM_operatortype_append(OBJECT_OT_visual_transform_apply);
	WM_operatortype_append(OBJECT_OT_location_apply);
	WM_operatortype_append(OBJECT_OT_scale_apply);
	WM_operatortype_append(OBJECT_OT_rotation_apply);
	WM_operatortype_append(OBJECT_OT_origin_set);
	
	WM_operatortype_append(OBJECT_OT_mode_set);
	WM_operatortype_append(OBJECT_OT_editmode_toggle);
	WM_operatortype_append(OBJECT_OT_posemode_toggle);
	WM_operatortype_append(OBJECT_OT_proxy_make);
	WM_operatortype_append(OBJECT_OT_restrictview_clear);
	WM_operatortype_append(OBJECT_OT_restrictview_set);
	WM_operatortype_append(OBJECT_OT_restrictrender_clear);
	WM_operatortype_append(OBJECT_OT_restrictrender_set);
	WM_operatortype_append(OBJECT_OT_shade_smooth);
	WM_operatortype_append(OBJECT_OT_shade_flat);
	WM_operatortype_append(OBJECT_OT_paths_calculate);
	WM_operatortype_append(OBJECT_OT_paths_clear);

	WM_operatortype_append(OBJECT_OT_parent_set);
	WM_operatortype_append(OBJECT_OT_parent_no_inverse_set);
	WM_operatortype_append(OBJECT_OT_parent_clear);
	WM_operatortype_append(OBJECT_OT_vertex_parent_set);
	WM_operatortype_append(OBJECT_OT_track_set);
	WM_operatortype_append(OBJECT_OT_track_clear);
	WM_operatortype_append(OBJECT_OT_slow_parent_set);
	WM_operatortype_append(OBJECT_OT_slow_parent_clear);
	WM_operatortype_append(OBJECT_OT_make_local);
	WM_operatortype_append(OBJECT_OT_make_single_user);
	WM_operatortype_append(OBJECT_OT_make_links_scene);
	WM_operatortype_append(OBJECT_OT_make_links_data);
	WM_operatortype_append(OBJECT_OT_move_to_layer);

	WM_operatortype_append(OBJECT_OT_select_inverse);
	WM_operatortype_append(OBJECT_OT_select_random);
	WM_operatortype_append(OBJECT_OT_select_all);
	WM_operatortype_append(OBJECT_OT_select_by_type);
	WM_operatortype_append(OBJECT_OT_select_by_layer);
	WM_operatortype_append(OBJECT_OT_select_linked);
	WM_operatortype_append(OBJECT_OT_select_grouped);
	WM_operatortype_append(OBJECT_OT_select_mirror);
	WM_operatortype_append(OBJECT_OT_select_name); /* XXX - weak, not compat with linked objects */

	WM_operatortype_append(GROUP_OT_create);
	WM_operatortype_append(GROUP_OT_objects_remove);
	WM_operatortype_append(GROUP_OT_objects_add_active);
	WM_operatortype_append(GROUP_OT_objects_remove_active);

	WM_operatortype_append(OBJECT_OT_delete);
	WM_operatortype_append(OBJECT_OT_curve_add);
	WM_operatortype_append(OBJECT_OT_text_add);
	WM_operatortype_append(OBJECT_OT_surface_add);
	WM_operatortype_append(OBJECT_OT_armature_add);
	WM_operatortype_append(OBJECT_OT_lamp_add);
	WM_operatortype_append(OBJECT_OT_camera_add);
	WM_operatortype_append(OBJECT_OT_add);
	WM_operatortype_append(OBJECT_OT_add_named);
	WM_operatortype_append(OBJECT_OT_effector_add);
	WM_operatortype_append(OBJECT_OT_group_instance_add);
	WM_operatortype_append(OBJECT_OT_metaball_add);
	WM_operatortype_append(OBJECT_OT_duplicates_make_real);
	WM_operatortype_append(OBJECT_OT_duplicate);
	WM_operatortype_append(OBJECT_OT_join);
	WM_operatortype_append(OBJECT_OT_join_shapes);
	WM_operatortype_append(OBJECT_OT_convert);

	WM_operatortype_append(OBJECT_OT_modifier_add);
	WM_operatortype_append(OBJECT_OT_modifier_remove);
	WM_operatortype_append(OBJECT_OT_modifier_move_up);
	WM_operatortype_append(OBJECT_OT_modifier_move_down);
	WM_operatortype_append(OBJECT_OT_modifier_apply);
	WM_operatortype_append(OBJECT_OT_modifier_convert);
	WM_operatortype_append(OBJECT_OT_modifier_copy);
	WM_operatortype_append(OBJECT_OT_multires_subdivide);
	WM_operatortype_append(OBJECT_OT_multires_reshape);
	WM_operatortype_append(OBJECT_OT_multires_higher_levels_delete);
	WM_operatortype_append(OBJECT_OT_multires_save_external);
	WM_operatortype_append(OBJECT_OT_multires_pack_external);
	WM_operatortype_append(OBJECT_OT_meshdeform_bind);
	WM_operatortype_append(OBJECT_OT_explode_refresh);
	
	WM_operatortype_append(OBJECT_OT_constraint_add);
	WM_operatortype_append(OBJECT_OT_constraint_add_with_targets);
	WM_operatortype_append(OBJECT_OT_constraint_copy);
	WM_operatortype_append(POSE_OT_constraint_add);
	WM_operatortype_append(POSE_OT_constraint_add_with_targets);
	WM_operatortype_append(OBJECT_OT_constraints_clear);
	WM_operatortype_append(POSE_OT_constraints_clear);
	WM_operatortype_append(POSE_OT_ik_add);
	WM_operatortype_append(POSE_OT_ik_clear);
	WM_operatortype_append(CONSTRAINT_OT_delete);
	WM_operatortype_append(CONSTRAINT_OT_move_up);
	WM_operatortype_append(CONSTRAINT_OT_move_down);
	WM_operatortype_append(CONSTRAINT_OT_stretchto_reset);
	WM_operatortype_append(CONSTRAINT_OT_limitdistance_reset);
	WM_operatortype_append(CONSTRAINT_OT_childof_set_inverse);
	WM_operatortype_append(CONSTRAINT_OT_childof_clear_inverse);

	WM_operatortype_append(OBJECT_OT_vertex_group_add);
	WM_operatortype_append(OBJECT_OT_vertex_group_remove);
	WM_operatortype_append(OBJECT_OT_vertex_group_assign);
	WM_operatortype_append(OBJECT_OT_vertex_group_remove_from);
	WM_operatortype_append(OBJECT_OT_vertex_group_select);
	WM_operatortype_append(OBJECT_OT_vertex_group_deselect);
	WM_operatortype_append(OBJECT_OT_vertex_group_copy_to_linked);
	WM_operatortype_append(OBJECT_OT_vertex_group_copy_to_selected);
	WM_operatortype_append(OBJECT_OT_vertex_group_copy);
	WM_operatortype_append(OBJECT_OT_vertex_group_normalize);
	WM_operatortype_append(OBJECT_OT_vertex_group_normalize_all);
	WM_operatortype_append(OBJECT_OT_vertex_group_invert);
	WM_operatortype_append(OBJECT_OT_vertex_group_levels);
	WM_operatortype_append(OBJECT_OT_vertex_group_blend);
	WM_operatortype_append(OBJECT_OT_vertex_group_clean);
	WM_operatortype_append(OBJECT_OT_vertex_group_mirror);
	WM_operatortype_append(OBJECT_OT_vertex_group_set_active);
	WM_operatortype_append(OBJECT_OT_vertex_group_sort);
	WM_operatortype_append(OBJECT_OT_vertex_group_move);

	WM_operatortype_append(OBJECT_OT_game_property_new);
	WM_operatortype_append(OBJECT_OT_game_property_remove);

	WM_operatortype_append(OBJECT_OT_shape_key_add);
	WM_operatortype_append(OBJECT_OT_shape_key_remove);
	WM_operatortype_append(OBJECT_OT_shape_key_clear);
	WM_operatortype_append(OBJECT_OT_shape_key_mirror);
	WM_operatortype_append(OBJECT_OT_shape_key_move);

	WM_operatortype_append(LATTICE_OT_select_all);
	WM_operatortype_append(LATTICE_OT_make_regular);

	WM_operatortype_append(OBJECT_OT_group_add);
	WM_operatortype_append(OBJECT_OT_group_link);
	WM_operatortype_append(OBJECT_OT_group_remove);

	WM_operatortype_append(OBJECT_OT_hook_add_selobj);
	WM_operatortype_append(OBJECT_OT_hook_add_newobj);
	WM_operatortype_append(OBJECT_OT_hook_remove);
	WM_operatortype_append(OBJECT_OT_hook_select);
	WM_operatortype_append(OBJECT_OT_hook_assign);
	WM_operatortype_append(OBJECT_OT_hook_reset);
	WM_operatortype_append(OBJECT_OT_hook_recenter);

	WM_operatortype_append(OBJECT_OT_bake_image);
	WM_operatortype_append(OBJECT_OT_drop_named_material);
}

void ED_operatormacros_object(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;
	
	ot= WM_operatortype_append_macro("OBJECT_OT_duplicate_move", "Duplicate", OPTYPE_UNDO|OPTYPE_REGISTER);
	if(ot) {
		WM_operatortype_macro_define(ot, "OBJECT_OT_duplicate");
		otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
		RNA_enum_set(otmacro->ptr, "proportional", PROP_EDIT_OFF);
	}

	/* grr, should be able to pass options on... */
	ot= WM_operatortype_append_macro("OBJECT_OT_duplicate_move_linked", "Duplicate Linked", OPTYPE_UNDO|OPTYPE_REGISTER);
	if(ot) {
		otmacro= WM_operatortype_macro_define(ot, "OBJECT_OT_duplicate");
		RNA_boolean_set(otmacro->ptr, "linked", 1);
		otmacro= WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
		RNA_enum_set(otmacro->ptr, "proportional", PROP_EDIT_OFF);
	}
	
	/* XXX */
	ot= WM_operatortype_append_macro("OBJECT_OT_add_named_cursor", "Add named object at cursor", OPTYPE_UNDO|OPTYPE_REGISTER);
	if(ot) {
		RNA_def_string(ot->srna, "name", "Cube", 24, "Name", "Object name to add.");

		WM_operatortype_macro_define(ot, "VIEW3D_OT_cursor3d");
		WM_operatortype_macro_define(ot, "OBJECT_OT_add_named");
	}
}

static int object_mode_poll(bContext *C)
{
	Object *ob= CTX_data_active_object(C);
	return (!ob || ob->mode == OB_MODE_OBJECT);
}

void ED_keymap_object(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	int i;
	
	/* Objects, Regardless of Mode -------------------------------------------------- */
	keymap= WM_keymap_find(keyconf, "Object Non-modal", 0, 0);
	
	/* Note: this keymap works disregarding mode */
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", TABKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "mode", OB_MODE_EDIT);
		RNA_boolean_set(kmi->ptr, "toggle", 1);

	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", TABKEY, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "mode", OB_MODE_POSE);
		RNA_boolean_set(kmi->ptr, "toggle", 1);

	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", VKEY, KM_PRESS, 0, 0);
		RNA_enum_set(kmi->ptr, "mode", OB_MODE_VERTEX_PAINT);
		RNA_boolean_set(kmi->ptr, "toggle", 1);
	
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_mode_set", TABKEY, KM_PRESS, KM_CTRL, 0);
		RNA_enum_set(kmi->ptr, "mode", OB_MODE_WEIGHT_PAINT);
		RNA_boolean_set(kmi->ptr, "toggle", 1);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_origin_set", CKEY, KM_PRESS, KM_ALT|KM_SHIFT|KM_CTRL, 0);

	/* Object Mode ---------------------------------------------------------------- */
	/* Note: this keymap gets disabled in non-objectmode,  */
	keymap= WM_keymap_find(keyconf, "Object Mode", 0, 0);
	keymap->poll= object_mode_poll;
	
	/* object mode supports PET now */
	ED_object_generic_keymap(keyconf, keymap, 1);

	WM_keymap_add_item(keymap, "VIEW3D_OT_game_start", PKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_linked", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_grouped", GKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_mirror", MKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	
	kmi= WM_keymap_add_item(keymap, "OBJECT_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set_identifier(kmi->ptr, "direction", "PARENT");
	kmi= WM_keymap_add_item(keymap, "OBJECT_OT_select_hierarchy", LEFTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set_identifier(kmi->ptr, "direction", "PARENT");
		RNA_boolean_set(kmi->ptr, "extend", 1);

	kmi= WM_keymap_add_item(keymap, "OBJECT_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
		RNA_enum_set_identifier(kmi->ptr, "direction", "CHILD");
	kmi= WM_keymap_add_item(keymap, "OBJECT_OT_select_hierarchy", RIGHTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_enum_set_identifier(kmi->ptr, "direction", "CHILD");
		RNA_boolean_set(kmi->ptr, "extend", 1);

	WM_keymap_verify_item(keymap, "OBJECT_OT_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_parent_no_inverse_set", PKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_parent_clear", PKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_track_set", TKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_track_clear", TKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_constraint_add_with_targets", CKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_constraints_clear", CKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_location_clear", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_rotation_clear", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_scale_clear", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_origin_clear", OKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_restrictview_clear", HKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_restrictview_set", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "OBJECT_OT_restrictview_set", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);

	/* same as above but for rendering */
	WM_keymap_add_item(keymap, "OBJECT_OT_restrictrender_clear", HKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_restrictrender_set", HKEY, KM_PRESS, KM_CTRL, 0);
//	RNA_boolean_set(WM_keymap_add_item(keymap, "OBJECT_OT_restrictrender_set", HKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "unselected", 1); // conflicts, removing

	WM_keymap_add_item(keymap, "OBJECT_OT_move_to_layer", MKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "INFO_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_duplicates_make_real", AKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);

	WM_keymap_add_menu(keymap, "VIEW3D_MT_object_apply", AKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_make_single_user", UKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "VIEW3D_MT_make_links", LKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_duplicate_move_linked", DKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_join", JKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_convert", CKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_proxy_make", PKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_make_local", LKEY, KM_PRESS, 0, 0);

	// XXX this should probably be in screen instead... here for testing purposes in the meantime... - Aligorith
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_insert_menu", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_delete_v3d", IKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_keying_set_active_set", IKEY, KM_PRESS, KM_CTRL|KM_SHIFT|KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "GROUP_OT_create", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_objects_remove", GKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_objects_add_active", GKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_objects_remove_active", GKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);

	WM_keymap_add_menu(keymap, "VIEW3D_MT_object_specials", WKEY, KM_PRESS, 0, 0);

	for(i=0; i<=5; i++) {
		kmi = WM_keymap_add_item(keymap, "OBJECT_OT_subdivision_set", ZEROKEY+i, KM_PRESS, KM_CTRL, 0);
		RNA_int_set(kmi->ptr, "level", i);
	}

	/* Lattice -------------------------------------------------------------------- */
	keymap= WM_keymap_find(keyconf, "Lattice", 0, 0);
	keymap->poll= ED_operator_editlattice;

	WM_keymap_add_item(keymap, "LATTICE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_vertex_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	
		/* menus */
	WM_keymap_add_menu(keymap, "VIEW3D_MT_hook", HKEY, KM_PRESS, KM_CTRL, 0);

	ED_object_generic_keymap(keyconf, keymap, 1);
}

void ED_object_generic_keymap(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap, int do_pet)
{
	wmKeyMapItem *kmi;

	/* used by mesh, curve & lattice only */
	if(do_pet > 0) {
		/* context ops */
		kmi = WM_keymap_add_item(keymap, "WM_OT_context_cycle_enum", OKEY, KM_PRESS, KM_SHIFT, 0);
		RNA_string_set(kmi->ptr, "path", "tool_settings.proportional_editing_falloff");

		kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle_enum", OKEY, KM_PRESS, 0, 0);
		RNA_string_set(kmi->ptr, "path", "tool_settings.proportional_editing");
		RNA_string_set(kmi->ptr, "value_1", "DISABLED");
		RNA_string_set(kmi->ptr, "value_2", "ENABLED");

		/* for modes/object types that allow 'conencted' mode, add the Alt O key */
		if (do_pet > 1) {
			kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle_enum", OKEY, KM_PRESS, KM_ALT, 0);
			RNA_string_set(kmi->ptr, "path", "tool_settings.proportional_editing");
			RNA_string_set(kmi->ptr, "value_1", "DISABLED");
			RNA_string_set(kmi->ptr, "value_2", "CONNECTED");
		}
	}
}

