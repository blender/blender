/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup edobj
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct StructRNA;
struct bContext;
struct wmOperator;
struct wmOperatorType;

struct ModifierData;

/* add hook menu */
enum eObject_Hook_Add_Mode {
  OBJECT_ADDHOOK_NEWOB = 1,
  OBJECT_ADDHOOK_SELOB,
  OBJECT_ADDHOOK_SELOB_BONE,
};

/* internal exports only */

/* object_transform.cc */

void OBJECT_OT_location_clear(struct wmOperatorType *ot);
void OBJECT_OT_rotation_clear(struct wmOperatorType *ot);
void OBJECT_OT_scale_clear(struct wmOperatorType *ot);
void OBJECT_OT_origin_clear(struct wmOperatorType *ot);
void OBJECT_OT_visual_transform_apply(struct wmOperatorType *ot);
void OBJECT_OT_transform_apply(struct wmOperatorType *ot);
void OBJECT_OT_parent_inverse_apply(wmOperatorType *ot);
void OBJECT_OT_transform_axis_target(struct wmOperatorType *ot);
void OBJECT_OT_origin_set(struct wmOperatorType *ot);

/* object_relations.c */

void OBJECT_OT_parent_set(struct wmOperatorType *ot);
void OBJECT_OT_parent_no_inverse_set(struct wmOperatorType *ot);
void OBJECT_OT_parent_clear(struct wmOperatorType *ot);
void OBJECT_OT_vertex_parent_set(struct wmOperatorType *ot);
void OBJECT_OT_track_set(struct wmOperatorType *ot);
void OBJECT_OT_track_clear(struct wmOperatorType *ot);
void OBJECT_OT_make_local(struct wmOperatorType *ot);
void OBJECT_OT_make_single_user(struct wmOperatorType *ot);
void OBJECT_OT_make_links_scene(struct wmOperatorType *ot);
void OBJECT_OT_make_links_data(struct wmOperatorType *ot);

void OBJECT_OT_make_override_library(struct wmOperatorType *ot);
void OBJECT_OT_reset_override_library(struct wmOperatorType *ot);
void OBJECT_OT_clear_override_library(struct wmOperatorType *ot);

/**
 * Used for drop-box.
 * Assigns to object under cursor, only first material slot.
 */
void OBJECT_OT_drop_named_material(struct wmOperatorType *ot);
/**
 * Used for drop-box.
 * Assigns to object under cursor, creates a new geometry nodes modifier.
 */
void OBJECT_OT_drop_geometry_nodes(struct wmOperatorType *ot);
/**
 * \note Only for empty-image objects, this operator is needed
 */
void OBJECT_OT_unlink_data(struct wmOperatorType *ot);

/* object_edit.cc */

void OBJECT_OT_hide_view_set(struct wmOperatorType *ot);
void OBJECT_OT_hide_view_clear(struct wmOperatorType *ot);
void OBJECT_OT_hide_collection(struct wmOperatorType *ot);
void OBJECT_OT_mode_set(struct wmOperatorType *ot);
void OBJECT_OT_mode_set_with_submode(struct wmOperatorType *ot);
void OBJECT_OT_editmode_toggle(struct wmOperatorType *ot);
void OBJECT_OT_posemode_toggle(struct wmOperatorType *ot);
void OBJECT_OT_shade_smooth(struct wmOperatorType *ot);
void OBJECT_OT_shade_flat(struct wmOperatorType *ot);
void OBJECT_OT_paths_calculate(struct wmOperatorType *ot);
void OBJECT_OT_paths_update(struct wmOperatorType *ot);
void OBJECT_OT_paths_clear(struct wmOperatorType *ot);
void OBJECT_OT_paths_update_visible(struct wmOperatorType *ot);
void OBJECT_OT_forcefield_toggle(struct wmOperatorType *ot);

void OBJECT_OT_move_to_collection(struct wmOperatorType *ot);
void OBJECT_OT_link_to_collection(struct wmOperatorType *ot);

void OBJECT_OT_transfer_mode(struct wmOperatorType *ot);

/* object_select.c */

void OBJECT_OT_select_all(struct wmOperatorType *ot);
void OBJECT_OT_select_random(struct wmOperatorType *ot);
void OBJECT_OT_select_by_type(struct wmOperatorType *ot);
void OBJECT_OT_select_linked(struct wmOperatorType *ot);
void OBJECT_OT_select_grouped(struct wmOperatorType *ot);
void OBJECT_OT_select_mirror(struct wmOperatorType *ot);
void OBJECT_OT_select_more(struct wmOperatorType *ot);
void OBJECT_OT_select_less(struct wmOperatorType *ot);
void OBJECT_OT_select_same_collection(struct wmOperatorType *ot);

/* object_add.cc */

void OBJECT_OT_add(struct wmOperatorType *ot);
void OBJECT_OT_add_named(struct wmOperatorType *ot);
void OBJECT_OT_transform_to_mouse(struct wmOperatorType *ot);
void OBJECT_OT_metaball_add(struct wmOperatorType *ot);
void OBJECT_OT_text_add(struct wmOperatorType *ot);
void OBJECT_OT_armature_add(struct wmOperatorType *ot);
void OBJECT_OT_empty_add(struct wmOperatorType *ot);
void OBJECT_OT_lightprobe_add(struct wmOperatorType *ot);
void OBJECT_OT_drop_named_image(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_add(struct wmOperatorType *ot);
void OBJECT_OT_light_add(struct wmOperatorType *ot);
void OBJECT_OT_effector_add(struct wmOperatorType *ot);
void OBJECT_OT_camera_add(struct wmOperatorType *ot);
void OBJECT_OT_speaker_add(struct wmOperatorType *ot);
void OBJECT_OT_curves_random_add(struct wmOperatorType *ot);
void OBJECT_OT_curves_empty_hair_add(struct wmOperatorType *ot);
void OBJECT_OT_pointcloud_add(struct wmOperatorType *ot);
/**
 * Only used as menu.
 */
void OBJECT_OT_collection_instance_add(struct wmOperatorType *ot);
void OBJECT_OT_collection_external_asset_drop(struct wmOperatorType *ot);
void OBJECT_OT_data_instance_add(struct wmOperatorType *ot);

void OBJECT_OT_duplicates_make_real(struct wmOperatorType *ot);
void OBJECT_OT_duplicate(struct wmOperatorType *ot);
void OBJECT_OT_delete(struct wmOperatorType *ot);
void OBJECT_OT_join(struct wmOperatorType *ot);
void OBJECT_OT_join_shapes(struct wmOperatorType *ot);
void OBJECT_OT_convert(struct wmOperatorType *ot);

/* object_volume.c */

void OBJECT_OT_volume_add(struct wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void OBJECT_OT_volume_import(struct wmOperatorType *ot);

/* object_hook.c */

void OBJECT_OT_hook_add_selob(struct wmOperatorType *ot);
void OBJECT_OT_hook_add_newob(struct wmOperatorType *ot);
void OBJECT_OT_hook_remove(struct wmOperatorType *ot);
void OBJECT_OT_hook_select(struct wmOperatorType *ot);
void OBJECT_OT_hook_assign(struct wmOperatorType *ot);
void OBJECT_OT_hook_reset(struct wmOperatorType *ot);
void OBJECT_OT_hook_recenter(struct wmOperatorType *ot);

/* object_collection.c */

void COLLECTION_OT_create(struct wmOperatorType *ot);
void COLLECTION_OT_objects_remove_all(struct wmOperatorType *ot);
void COLLECTION_OT_objects_remove(struct wmOperatorType *ot);
void COLLECTION_OT_objects_add_active(struct wmOperatorType *ot);
void COLLECTION_OT_objects_remove_active(struct wmOperatorType *ot);

/* object_modifier.c */

bool edit_modifier_poll_generic(struct bContext *C,
                                struct StructRNA *rna_type,
                                int obtype_flag,
                                bool is_editmode_allowed,
                                bool is_liboverride_allowed);
void edit_modifier_properties(struct wmOperatorType *ot);
bool edit_modifier_invoke_properties(struct bContext *C, struct wmOperator *op);

struct ModifierData *edit_modifier_property_get(struct wmOperator *op,
                                                struct Object *ob,
                                                int type);

void OBJECT_OT_modifier_add(struct wmOperatorType *ot);
void OBJECT_OT_modifier_remove(struct wmOperatorType *ot);
void OBJECT_OT_modifier_move_up(struct wmOperatorType *ot);
void OBJECT_OT_modifier_move_down(struct wmOperatorType *ot);
void OBJECT_OT_modifier_move_to_index(struct wmOperatorType *ot);
void OBJECT_OT_modifier_apply(struct wmOperatorType *ot);
void OBJECT_OT_modifier_apply_as_shapekey(wmOperatorType *ot);
void OBJECT_OT_modifier_convert(struct wmOperatorType *ot);
void OBJECT_OT_modifier_copy(struct wmOperatorType *ot);
void OBJECT_OT_modifier_copy_to_selected(struct wmOperatorType *ot);
void OBJECT_OT_modifier_set_active(struct wmOperatorType *ot);
void OBJECT_OT_multires_subdivide(struct wmOperatorType *ot);
void OBJECT_OT_multires_reshape(struct wmOperatorType *ot);
void OBJECT_OT_multires_higher_levels_delete(struct wmOperatorType *ot);
void OBJECT_OT_multires_base_apply(struct wmOperatorType *ot);
void OBJECT_OT_multires_unsubdivide(struct wmOperatorType *ot);
void OBJECT_OT_multires_rebuild_subdiv(struct wmOperatorType *ot);
void OBJECT_OT_multires_external_save(struct wmOperatorType *ot);
void OBJECT_OT_multires_external_pack(struct wmOperatorType *ot);
void OBJECT_OT_correctivesmooth_bind(struct wmOperatorType *ot);
void OBJECT_OT_meshdeform_bind(struct wmOperatorType *ot);
void OBJECT_OT_explode_refresh(struct wmOperatorType *ot);
void OBJECT_OT_ocean_bake(struct wmOperatorType *ot);
void OBJECT_OT_skin_root_mark(struct wmOperatorType *ot);
void OBJECT_OT_skin_loose_mark_clear(struct wmOperatorType *ot);
void OBJECT_OT_skin_radii_equalize(struct wmOperatorType *ot);
void OBJECT_OT_skin_armature_create(struct wmOperatorType *ot);
void OBJECT_OT_laplaciandeform_bind(struct wmOperatorType *ot);
void OBJECT_OT_surfacedeform_bind(struct wmOperatorType *ot);
void OBJECT_OT_geometry_nodes_input_attribute_toggle(struct wmOperatorType *ot);
void OBJECT_OT_geometry_node_tree_copy_assign(struct wmOperatorType *ot);

/* object_gpencil_modifiers.c */

void OBJECT_OT_gpencil_modifier_add(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_remove(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_move_up(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_move_down(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_move_to_index(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_apply(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_copy(struct wmOperatorType *ot);
void OBJECT_OT_gpencil_modifier_copy_to_selected(struct wmOperatorType *ot);

void GPENCIL_OT_segment_add(struct wmOperatorType *ot);
void GPENCIL_OT_segment_remove(struct wmOperatorType *ot);
void GPENCIL_OT_segment_move(struct wmOperatorType *ot);

void GPENCIL_OT_time_segment_add(struct wmOperatorType *ot);
void GPENCIL_OT_time_segment_remove(struct wmOperatorType *ot);
void GPENCIL_OT_time_segment_move(struct wmOperatorType *ot);

/* object_shader_fx.c */

void OBJECT_OT_shaderfx_add(struct wmOperatorType *ot);
void OBJECT_OT_shaderfx_copy(struct wmOperatorType *ot);
void OBJECT_OT_shaderfx_remove(struct wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_up(struct wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_down(struct wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_to_index(struct wmOperatorType *ot);

/* object_constraint.c */

void OBJECT_OT_constraint_add(struct wmOperatorType *ot);
void OBJECT_OT_constraint_add_with_targets(struct wmOperatorType *ot);
void POSE_OT_constraint_add(struct wmOperatorType *ot);
void POSE_OT_constraint_add_with_targets(struct wmOperatorType *ot);

void OBJECT_OT_constraints_copy(struct wmOperatorType *ot);
void POSE_OT_constraints_copy(struct wmOperatorType *ot);

void OBJECT_OT_constraints_clear(struct wmOperatorType *ot);
void POSE_OT_constraints_clear(struct wmOperatorType *ot);

void POSE_OT_ik_add(struct wmOperatorType *ot);
void POSE_OT_ik_clear(struct wmOperatorType *ot);

void CONSTRAINT_OT_delete(struct wmOperatorType *ot);
void CONSTRAINT_OT_apply(struct wmOperatorType *ot);
void CONSTRAINT_OT_copy(struct wmOperatorType *ot);
void CONSTRAINT_OT_copy_to_selected(struct wmOperatorType *ot);

void CONSTRAINT_OT_move_up(struct wmOperatorType *ot);
void CONSTRAINT_OT_move_to_index(struct wmOperatorType *ot);
void CONSTRAINT_OT_move_down(struct wmOperatorType *ot);

void CONSTRAINT_OT_stretchto_reset(struct wmOperatorType *ot);
void CONSTRAINT_OT_limitdistance_reset(struct wmOperatorType *ot);
void CONSTRAINT_OT_childof_set_inverse(struct wmOperatorType *ot);
void CONSTRAINT_OT_childof_clear_inverse(struct wmOperatorType *ot);
void CONSTRAINT_OT_objectsolver_set_inverse(struct wmOperatorType *ot);
void CONSTRAINT_OT_objectsolver_clear_inverse(struct wmOperatorType *ot);
void CONSTRAINT_OT_followpath_path_animate(struct wmOperatorType *ot);

/* object_vgroup.cc */

void OBJECT_OT_vertex_group_add(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_remove(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_assign(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_assign_new(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_remove_from(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_select(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_deselect(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_copy_to_selected(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_copy(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_normalize(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_normalize_all(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_levels(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_lock(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_invert(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_smooth(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_clean(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_quantize(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_limit_total(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_mirror(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_set_active(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_sort(struct wmOperatorType *ot);
void OBJECT_OT_vertex_group_move(struct wmOperatorType *ot);
void OBJECT_OT_vertex_weight_paste(struct wmOperatorType *ot);
void OBJECT_OT_vertex_weight_delete(struct wmOperatorType *ot);
void OBJECT_OT_vertex_weight_set_active(struct wmOperatorType *ot);
void OBJECT_OT_vertex_weight_normalize_active_vertex(struct wmOperatorType *ot);
void OBJECT_OT_vertex_weight_copy(struct wmOperatorType *ot);

/* object_facemap_ops.c */

void OBJECT_OT_face_map_add(struct wmOperatorType *ot);
void OBJECT_OT_face_map_remove(struct wmOperatorType *ot);
void OBJECT_OT_face_map_assign(struct wmOperatorType *ot);
void OBJECT_OT_face_map_remove_from(struct wmOperatorType *ot);
void OBJECT_OT_face_map_select(struct wmOperatorType *ot);
void OBJECT_OT_face_map_deselect(struct wmOperatorType *ot);
void OBJECT_OT_face_map_move(struct wmOperatorType *ot);

/* object_warp.c */

void TRANSFORM_OT_vertex_warp(struct wmOperatorType *ot);

/* object_shapekey.c */

void OBJECT_OT_shape_key_add(struct wmOperatorType *ot);
void OBJECT_OT_shape_key_remove(struct wmOperatorType *ot);
void OBJECT_OT_shape_key_clear(struct wmOperatorType *ot);
void OBJECT_OT_shape_key_retime(struct wmOperatorType *ot);
void OBJECT_OT_shape_key_mirror(struct wmOperatorType *ot);
void OBJECT_OT_shape_key_move(struct wmOperatorType *ot);

/* object_collection.c */

void OBJECT_OT_collection_add(struct wmOperatorType *ot);
void OBJECT_OT_collection_link(struct wmOperatorType *ot);
void OBJECT_OT_collection_remove(struct wmOperatorType *ot);
void OBJECT_OT_collection_unlink(struct wmOperatorType *ot);
void OBJECT_OT_collection_objects_select(struct wmOperatorType *ot);

/* object_bake.c */

void OBJECT_OT_bake_image(wmOperatorType *ot);
void OBJECT_OT_bake(wmOperatorType *ot);

/* object_bake_simulation.cc */

void OBJECT_OT_simulation_nodes_cache_calculate_to_frame(wmOperatorType *ot);
void OBJECT_OT_simulation_nodes_cache_bake(wmOperatorType *ot);
void OBJECT_OT_simulation_nodes_cache_delete(wmOperatorType *ot);

/* object_random.c */

void TRANSFORM_OT_vertex_random(struct wmOperatorType *ot);

/* object_remesh.cc */

void OBJECT_OT_voxel_remesh(struct wmOperatorType *ot);
void OBJECT_OT_voxel_size_edit(struct wmOperatorType *ot);
void OBJECT_OT_quadriflow_remesh(struct wmOperatorType *ot);

/* object_transfer_data.c */

/**
 * Transfer mesh data from active to selected objects.
 */
void OBJECT_OT_data_transfer(struct wmOperatorType *ot);
void OBJECT_OT_datalayout_transfer(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
