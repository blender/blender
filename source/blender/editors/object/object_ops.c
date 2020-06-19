/*
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
 */

/** \file
 * \ingroup edobj
 */

#include <math.h>
#include <stdlib.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "DEG_depsgraph.h"

#include "object_intern.h"

/* ************************** registration **********************************/

void ED_operatortypes_object(void)
{
  WM_operatortype_append(OBJECT_OT_location_clear);
  WM_operatortype_append(OBJECT_OT_rotation_clear);
  WM_operatortype_append(OBJECT_OT_scale_clear);
  WM_operatortype_append(OBJECT_OT_origin_clear);
  WM_operatortype_append(OBJECT_OT_visual_transform_apply);
  WM_operatortype_append(OBJECT_OT_transform_apply);
  WM_operatortype_append(OBJECT_OT_transform_axis_target);
  WM_operatortype_append(OBJECT_OT_origin_set);

  WM_operatortype_append(OBJECT_OT_mode_set);
  WM_operatortype_append(OBJECT_OT_mode_set_with_submode);
  WM_operatortype_append(OBJECT_OT_editmode_toggle);
  WM_operatortype_append(OBJECT_OT_posemode_toggle);
  WM_operatortype_append(OBJECT_OT_proxy_make);
  WM_operatortype_append(OBJECT_OT_shade_smooth);
  WM_operatortype_append(OBJECT_OT_shade_flat);
  WM_operatortype_append(OBJECT_OT_paths_calculate);
  WM_operatortype_append(OBJECT_OT_paths_update);
  WM_operatortype_append(OBJECT_OT_paths_clear);
  WM_operatortype_append(OBJECT_OT_paths_range_update);
  WM_operatortype_append(OBJECT_OT_forcefield_toggle);

  WM_operatortype_append(OBJECT_OT_parent_set);
  WM_operatortype_append(OBJECT_OT_parent_no_inverse_set);
  WM_operatortype_append(OBJECT_OT_parent_clear);
  WM_operatortype_append(OBJECT_OT_vertex_parent_set);
  WM_operatortype_append(OBJECT_OT_track_set);
  WM_operatortype_append(OBJECT_OT_track_clear);
  WM_operatortype_append(OBJECT_OT_make_local);
  WM_operatortype_append(OBJECT_OT_make_override_library);
  WM_operatortype_append(OBJECT_OT_make_single_user);
  WM_operatortype_append(OBJECT_OT_make_links_scene);
  WM_operatortype_append(OBJECT_OT_make_links_data);

  WM_operatortype_append(OBJECT_OT_select_random);
  WM_operatortype_append(OBJECT_OT_select_all);
  WM_operatortype_append(OBJECT_OT_select_same_collection);
  WM_operatortype_append(OBJECT_OT_select_by_type);
  WM_operatortype_append(OBJECT_OT_select_linked);
  WM_operatortype_append(OBJECT_OT_select_grouped);
  WM_operatortype_append(OBJECT_OT_select_mirror);
  WM_operatortype_append(OBJECT_OT_select_more);
  WM_operatortype_append(OBJECT_OT_select_less);

  WM_operatortype_append(COLLECTION_OT_create);
  WM_operatortype_append(COLLECTION_OT_objects_remove_all);
  WM_operatortype_append(COLLECTION_OT_objects_remove);
  WM_operatortype_append(COLLECTION_OT_objects_add_active);
  WM_operatortype_append(COLLECTION_OT_objects_remove_active);

  WM_operatortype_append(OBJECT_OT_delete);
  WM_operatortype_append(OBJECT_OT_text_add);
  WM_operatortype_append(OBJECT_OT_armature_add);
  WM_operatortype_append(OBJECT_OT_empty_add);
  WM_operatortype_append(OBJECT_OT_lightprobe_add);
  WM_operatortype_append(OBJECT_OT_drop_named_image);
  WM_operatortype_append(OBJECT_OT_gpencil_add);
  WM_operatortype_append(OBJECT_OT_light_add);
  WM_operatortype_append(OBJECT_OT_camera_add);
  WM_operatortype_append(OBJECT_OT_speaker_add);
#ifdef WITH_NEW_OBJECT_TYPES
  WM_operatortype_append(OBJECT_OT_hair_add);
  WM_operatortype_append(OBJECT_OT_pointcloud_add);
#endif
  WM_operatortype_append(OBJECT_OT_volume_add);
  WM_operatortype_append(OBJECT_OT_volume_import);
  WM_operatortype_append(OBJECT_OT_add);
  WM_operatortype_append(OBJECT_OT_add_named);
  WM_operatortype_append(OBJECT_OT_effector_add);
  WM_operatortype_append(OBJECT_OT_collection_instance_add);
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
  WM_operatortype_append(OBJECT_OT_modifier_move_to_index);
  WM_operatortype_append(OBJECT_OT_modifier_apply);
  WM_operatortype_append(OBJECT_OT_modifier_convert);
  WM_operatortype_append(OBJECT_OT_modifier_copy);
  WM_operatortype_append(OBJECT_OT_multires_subdivide);
  WM_operatortype_append(OBJECT_OT_multires_reshape);
  WM_operatortype_append(OBJECT_OT_multires_higher_levels_delete);
  WM_operatortype_append(OBJECT_OT_multires_base_apply);
  WM_operatortype_append(OBJECT_OT_multires_unsubdivide);
  WM_operatortype_append(OBJECT_OT_multires_rebuild_subdiv);
  WM_operatortype_append(OBJECT_OT_multires_external_save);
  WM_operatortype_append(OBJECT_OT_multires_external_pack);
  WM_operatortype_append(OBJECT_OT_skin_root_mark);
  WM_operatortype_append(OBJECT_OT_skin_loose_mark_clear);
  WM_operatortype_append(OBJECT_OT_skin_radii_equalize);
  WM_operatortype_append(OBJECT_OT_skin_armature_create);

  /* grease pencil modifiers */
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_add);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_remove);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_move_up);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_move_down);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_move_to_index);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_apply);
  WM_operatortype_append(OBJECT_OT_gpencil_modifier_copy);

  /* shader fx */
  WM_operatortype_append(OBJECT_OT_shaderfx_add);
  WM_operatortype_append(OBJECT_OT_shaderfx_remove);
  WM_operatortype_append(OBJECT_OT_shaderfx_move_up);
  WM_operatortype_append(OBJECT_OT_shaderfx_move_down);
  WM_operatortype_append(OBJECT_OT_shaderfx_move_to_index);

  WM_operatortype_append(OBJECT_OT_correctivesmooth_bind);
  WM_operatortype_append(OBJECT_OT_meshdeform_bind);
  WM_operatortype_append(OBJECT_OT_explode_refresh);
  WM_operatortype_append(OBJECT_OT_ocean_bake);

  WM_operatortype_append(OBJECT_OT_constraint_add);
  WM_operatortype_append(OBJECT_OT_constraint_add_with_targets);
  WM_operatortype_append(POSE_OT_constraint_add);
  WM_operatortype_append(POSE_OT_constraint_add_with_targets);
  WM_operatortype_append(OBJECT_OT_constraints_copy);
  WM_operatortype_append(POSE_OT_constraints_copy);
  WM_operatortype_append(OBJECT_OT_constraints_clear);
  WM_operatortype_append(POSE_OT_constraints_clear);
  WM_operatortype_append(POSE_OT_ik_add);
  WM_operatortype_append(POSE_OT_ik_clear);
  WM_operatortype_append(CONSTRAINT_OT_delete);
  WM_operatortype_append(CONSTRAINT_OT_move_up);
  WM_operatortype_append(CONSTRAINT_OT_move_down);
  WM_operatortype_append(CONSTRAINT_OT_move_to_index);
  WM_operatortype_append(CONSTRAINT_OT_stretchto_reset);
  WM_operatortype_append(CONSTRAINT_OT_limitdistance_reset);
  WM_operatortype_append(CONSTRAINT_OT_childof_set_inverse);
  WM_operatortype_append(CONSTRAINT_OT_childof_clear_inverse);
  WM_operatortype_append(CONSTRAINT_OT_objectsolver_set_inverse);
  WM_operatortype_append(CONSTRAINT_OT_objectsolver_clear_inverse);
  WM_operatortype_append(CONSTRAINT_OT_followpath_path_animate);

  WM_operatortype_append(OBJECT_OT_vertex_group_add);
  WM_operatortype_append(OBJECT_OT_vertex_group_remove);
  WM_operatortype_append(OBJECT_OT_vertex_group_assign);
  WM_operatortype_append(OBJECT_OT_vertex_group_assign_new);
  WM_operatortype_append(OBJECT_OT_vertex_group_remove_from);
  WM_operatortype_append(OBJECT_OT_vertex_group_select);
  WM_operatortype_append(OBJECT_OT_vertex_group_deselect);
  WM_operatortype_append(OBJECT_OT_vertex_group_copy_to_linked);
  WM_operatortype_append(OBJECT_OT_vertex_group_copy_to_selected);
  WM_operatortype_append(OBJECT_OT_vertex_group_copy);
  WM_operatortype_append(OBJECT_OT_vertex_group_normalize);
  WM_operatortype_append(OBJECT_OT_vertex_group_normalize_all);
  WM_operatortype_append(OBJECT_OT_vertex_group_lock);
  WM_operatortype_append(OBJECT_OT_vertex_group_fix);
  WM_operatortype_append(OBJECT_OT_vertex_group_invert);
  WM_operatortype_append(OBJECT_OT_vertex_group_levels);
  WM_operatortype_append(OBJECT_OT_vertex_group_smooth);
  WM_operatortype_append(OBJECT_OT_vertex_group_clean);
  WM_operatortype_append(OBJECT_OT_vertex_group_quantize);
  WM_operatortype_append(OBJECT_OT_vertex_group_limit_total);
  WM_operatortype_append(OBJECT_OT_vertex_group_mirror);
  WM_operatortype_append(OBJECT_OT_vertex_group_set_active);
  WM_operatortype_append(OBJECT_OT_vertex_group_sort);
  WM_operatortype_append(OBJECT_OT_vertex_group_move);
  WM_operatortype_append(OBJECT_OT_vertex_weight_paste);
  WM_operatortype_append(OBJECT_OT_vertex_weight_delete);
  WM_operatortype_append(OBJECT_OT_vertex_weight_set_active);
  WM_operatortype_append(OBJECT_OT_vertex_weight_normalize_active_vertex);
  WM_operatortype_append(OBJECT_OT_vertex_weight_copy);

  WM_operatortype_append(OBJECT_OT_face_map_add);
  WM_operatortype_append(OBJECT_OT_face_map_remove);
  WM_operatortype_append(OBJECT_OT_face_map_assign);
  WM_operatortype_append(OBJECT_OT_face_map_remove_from);
  WM_operatortype_append(OBJECT_OT_face_map_select);
  WM_operatortype_append(OBJECT_OT_face_map_deselect);
  WM_operatortype_append(OBJECT_OT_face_map_move);

  WM_operatortype_append(TRANSFORM_OT_vertex_warp);

  WM_operatortype_append(OBJECT_OT_move_to_collection);
  WM_operatortype_append(OBJECT_OT_link_to_collection);

  WM_operatortype_append(OBJECT_OT_shape_key_add);
  WM_operatortype_append(OBJECT_OT_shape_key_remove);
  WM_operatortype_append(OBJECT_OT_shape_key_clear);
  WM_operatortype_append(OBJECT_OT_shape_key_retime);
  WM_operatortype_append(OBJECT_OT_shape_key_mirror);
  WM_operatortype_append(OBJECT_OT_shape_key_move);

  WM_operatortype_append(OBJECT_OT_collection_add);
  WM_operatortype_append(OBJECT_OT_collection_link);
  WM_operatortype_append(OBJECT_OT_collection_remove);
  WM_operatortype_append(OBJECT_OT_collection_unlink);
  WM_operatortype_append(OBJECT_OT_collection_objects_select);

  WM_operatortype_append(OBJECT_OT_hook_add_selob);
  WM_operatortype_append(OBJECT_OT_hook_add_newob);
  WM_operatortype_append(OBJECT_OT_hook_remove);
  WM_operatortype_append(OBJECT_OT_hook_select);
  WM_operatortype_append(OBJECT_OT_hook_assign);
  WM_operatortype_append(OBJECT_OT_hook_reset);
  WM_operatortype_append(OBJECT_OT_hook_recenter);

  WM_operatortype_append(OBJECT_OT_bake_image);
  WM_operatortype_append(OBJECT_OT_bake);
  WM_operatortype_append(OBJECT_OT_drop_named_material);
  WM_operatortype_append(OBJECT_OT_unlink_data);
  WM_operatortype_append(OBJECT_OT_laplaciandeform_bind);

  WM_operatortype_append(TRANSFORM_OT_vertex_random);

  WM_operatortype_append(OBJECT_OT_data_transfer);
  WM_operatortype_append(OBJECT_OT_datalayout_transfer);
  WM_operatortype_append(OBJECT_OT_surfacedeform_bind);

  WM_operatortype_append(OBJECT_OT_hide_view_clear);
  WM_operatortype_append(OBJECT_OT_hide_view_set);
  WM_operatortype_append(OBJECT_OT_hide_collection);

  WM_operatortype_append(OBJECT_OT_voxel_remesh);
  WM_operatortype_append(OBJECT_OT_voxel_size_edit);

  WM_operatortype_append(OBJECT_OT_quadriflow_remesh);
}

void ED_operatormacros_object(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("OBJECT_OT_duplicate_move",
                                    "Duplicate Objects",
                                    "Duplicate selected objects and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  if (ot) {
    WM_operatortype_macro_define(ot, "OBJECT_OT_duplicate");
    otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
    RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  }

  /* grr, should be able to pass options on... */
  ot = WM_operatortype_append_macro("OBJECT_OT_duplicate_move_linked",
                                    "Duplicate Linked",
                                    "Duplicate selected objects and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  if (ot) {
    otmacro = WM_operatortype_macro_define(ot, "OBJECT_OT_duplicate");
    RNA_boolean_set(otmacro->ptr, "linked", true);
    otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
    RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  }
}

static bool object_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return (!ob || ob->mode == OB_MODE_OBJECT);
}

void ED_keymap_object(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;

  /* Objects, Regardless of Mode -------------------------------------------------- */
  keymap = WM_keymap_ensure(keyconf, "Object Non-modal", 0, 0);

  /* Object Mode ---------------------------------------------------------------- */
  /* Note: this keymap gets disabled in non-objectmode,  */
  keymap = WM_keymap_ensure(keyconf, "Object Mode", 0, 0);
  keymap->poll = object_mode_poll;
}
