/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#pragma once

#include "BLI_vector.hh"

#include "RNA_types.hh"

struct bContext;
struct ModifierData;
struct Object;
struct StructRNA;
struct wmOperator;
struct wmOperatorType;

namespace blender::ed::object {

/* add hook menu */
enum eObject_Hook_Add_Mode {
  OBJECT_ADDHOOK_NEWOB = 1,
  OBJECT_ADDHOOK_SELOB,
  OBJECT_ADDHOOK_SELOB_BONE,
};

/* internal exports only */

/* object_transform.cc */

void OBJECT_OT_location_clear(wmOperatorType *ot);
void OBJECT_OT_rotation_clear(wmOperatorType *ot);
void OBJECT_OT_scale_clear(wmOperatorType *ot);
void OBJECT_OT_origin_clear(wmOperatorType *ot);
void OBJECT_OT_visual_transform_apply(wmOperatorType *ot);
void OBJECT_OT_transform_apply(wmOperatorType *ot);
void OBJECT_OT_parent_inverse_apply(wmOperatorType *ot);
void OBJECT_OT_transform_axis_target(wmOperatorType *ot);
void OBJECT_OT_origin_set(wmOperatorType *ot);

/* `object_relations.cc` */

void OBJECT_OT_parent_set(wmOperatorType *ot);
void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot);
void OBJECT_OT_parent_clear(wmOperatorType *ot);
void OBJECT_OT_vertex_parent_set(wmOperatorType *ot);
void OBJECT_OT_track_set(wmOperatorType *ot);
void OBJECT_OT_track_clear(wmOperatorType *ot);
void OBJECT_OT_make_local(wmOperatorType *ot);
void OBJECT_OT_make_single_user(wmOperatorType *ot);
void OBJECT_OT_make_links_scene(wmOperatorType *ot);
void OBJECT_OT_make_links_data(wmOperatorType *ot);

void OBJECT_OT_make_override_library(wmOperatorType *ot);
void OBJECT_OT_reset_override_library(wmOperatorType *ot);
void OBJECT_OT_clear_override_library(wmOperatorType *ot);

/**
 * Used for drop-box.
 * Assigns to object under cursor, only first material slot.
 */
void OBJECT_OT_drop_named_material(wmOperatorType *ot);
/**
 * Used for drop-box.
 * Assigns to object under cursor, creates a new geometry nodes modifier.
 */
void OBJECT_OT_drop_geometry_nodes(wmOperatorType *ot);
/**
 * \note Only for empty-image objects, this operator is needed
 */
void OBJECT_OT_unlink_data(wmOperatorType *ot);

/* object_edit.cc */

void OBJECT_OT_hide_view_set(wmOperatorType *ot);
void OBJECT_OT_hide_view_clear(wmOperatorType *ot);
void OBJECT_OT_hide_collection(wmOperatorType *ot);
void OBJECT_OT_mode_set(wmOperatorType *ot);
void OBJECT_OT_mode_set_with_submode(wmOperatorType *ot);
void OBJECT_OT_editmode_toggle(wmOperatorType *ot);
void OBJECT_OT_posemode_toggle(wmOperatorType *ot);
void OBJECT_OT_shade_smooth(wmOperatorType *ot);
void OBJECT_OT_shade_smooth_by_angle(wmOperatorType *ot);
void OBJECT_OT_shade_auto_smooth(wmOperatorType *ot);
void OBJECT_OT_shade_flat(wmOperatorType *ot);
void OBJECT_OT_paths_calculate(wmOperatorType *ot);
void OBJECT_OT_paths_update(wmOperatorType *ot);
void OBJECT_OT_paths_clear(wmOperatorType *ot);
void OBJECT_OT_paths_update_visible(wmOperatorType *ot);
void OBJECT_OT_forcefield_toggle(wmOperatorType *ot);

void OBJECT_OT_move_to_collection(wmOperatorType *ot);
void OBJECT_OT_link_to_collection(wmOperatorType *ot);
void move_to_collection_menu_register();
void link_to_collection_menu_register();

void OBJECT_OT_transfer_mode(wmOperatorType *ot);

/* `object_select.cc` */

void OBJECT_OT_select_all(wmOperatorType *ot);
void OBJECT_OT_select_random(wmOperatorType *ot);
void OBJECT_OT_select_by_type(wmOperatorType *ot);
void OBJECT_OT_select_linked(wmOperatorType *ot);
void OBJECT_OT_select_grouped(wmOperatorType *ot);
void OBJECT_OT_select_mirror(wmOperatorType *ot);
void OBJECT_OT_select_more(wmOperatorType *ot);
void OBJECT_OT_select_less(wmOperatorType *ot);
void OBJECT_OT_select_same_collection(wmOperatorType *ot);

/* object_add.cc */

void OBJECT_OT_add(wmOperatorType *ot);
void OBJECT_OT_lattice_add_to_selected(wmOperatorType *ot);
void OBJECT_OT_add_named(wmOperatorType *ot);
void OBJECT_OT_transform_to_mouse(wmOperatorType *ot);
void OBJECT_OT_metaball_add(wmOperatorType *ot);
void OBJECT_OT_text_add(wmOperatorType *ot);
void OBJECT_OT_armature_add(wmOperatorType *ot);
void OBJECT_OT_empty_add(wmOperatorType *ot);
void OBJECT_OT_lightprobe_add(wmOperatorType *ot);
void OBJECT_OT_empty_image_add(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_add(wmOperatorType *ot);
void OBJECT_OT_light_add(wmOperatorType *ot);
void OBJECT_OT_effector_add(wmOperatorType *ot);
void OBJECT_OT_camera_add(wmOperatorType *ot);
void OBJECT_OT_speaker_add(wmOperatorType *ot);
void OBJECT_OT_curves_random_add(wmOperatorType *ot);
void OBJECT_OT_curves_empty_hair_add(wmOperatorType *ot);
void OBJECT_OT_pointcloud_random_add(wmOperatorType *ot);
/**
 * Only used as menu.
 */
void OBJECT_OT_collection_instance_add(wmOperatorType *ot);
void OBJECT_OT_collection_external_asset_drop(wmOperatorType *ot);
void OBJECT_OT_data_instance_add(wmOperatorType *ot);

void OBJECT_OT_duplicates_make_real(wmOperatorType *ot);
void OBJECT_OT_duplicate(wmOperatorType *ot);
void OBJECT_OT_delete(wmOperatorType *ot);
void OBJECT_OT_join(wmOperatorType *ot);
void OBJECT_OT_join_shapes(wmOperatorType *ot);
void OBJECT_OT_update_shapes(wmOperatorType *ot);
void OBJECT_OT_convert(wmOperatorType *ot);

/* `object_volume.cc` */

void OBJECT_OT_volume_add(wmOperatorType *ot);
/**
 * Called by other space types too.
 */
void OBJECT_OT_volume_import(wmOperatorType *ot);

/* `object_hook.cc` */

void OBJECT_OT_hook_add_selob(wmOperatorType *ot);
void OBJECT_OT_hook_add_newob(wmOperatorType *ot);
void OBJECT_OT_hook_remove(wmOperatorType *ot);
void OBJECT_OT_hook_select(wmOperatorType *ot);
void OBJECT_OT_hook_assign(wmOperatorType *ot);
void OBJECT_OT_hook_reset(wmOperatorType *ot);
void OBJECT_OT_hook_recenter(wmOperatorType *ot);

/* `object_collection.cc` */

void COLLECTION_OT_create(wmOperatorType *ot);
void COLLECTION_OT_objects_remove_all(wmOperatorType *ot);
void COLLECTION_OT_objects_remove(wmOperatorType *ot);
void COLLECTION_OT_objects_add_active(wmOperatorType *ot);
void COLLECTION_OT_objects_remove_active(wmOperatorType *ot);

/* object_light_linking_ops.cc */

void OBJECT_OT_light_linking_receiver_collection_new(wmOperatorType *ot);
void OBJECT_OT_light_linking_receivers_select(wmOperatorType *ot);
void OBJECT_OT_light_linking_receivers_link(wmOperatorType *ot);

void OBJECT_OT_light_linking_blocker_collection_new(wmOperatorType *ot);
void OBJECT_OT_light_linking_blockers_select(wmOperatorType *ot);
void OBJECT_OT_light_linking_blockers_link(wmOperatorType *ot);

void OBJECT_OT_light_linking_unlink_from_collection(wmOperatorType *ot);

/* object_camera.cc */

void OBJECT_OT_camera_custom_update(wmOperatorType *ot);

/* `object_modifier.cc` */

bool edit_modifier_poll_generic(bContext *C,
                                StructRNA *rna_type,
                                int obtype_flag,
                                bool is_editmode_allowed,
                                bool is_liboverride_allowed);
void edit_modifier_properties(wmOperatorType *ot);
bool edit_modifier_invoke_properties(bContext *C, wmOperator *op);

ModifierData *edit_modifier_property_get(wmOperator *op, Object *ob, int type);

void OBJECT_OT_modifier_add(wmOperatorType *ot);
void OBJECT_OT_modifier_remove(wmOperatorType *ot);
void OBJECT_OT_modifiers_clear(wmOperatorType *ot);
void OBJECT_OT_modifier_move_up(wmOperatorType *ot);
void OBJECT_OT_modifier_move_down(wmOperatorType *ot);
void OBJECT_OT_modifier_move_to_index(wmOperatorType *ot);
void OBJECT_OT_modifier_apply(wmOperatorType *ot);
void OBJECT_OT_modifier_apply_as_shapekey(wmOperatorType *ot);
void OBJECT_OT_modifier_convert(wmOperatorType *ot);
void OBJECT_OT_modifier_copy(wmOperatorType *ot);
void OBJECT_OT_modifier_copy_to_selected(wmOperatorType *ot);
void OBJECT_OT_modifiers_copy_to_selected(wmOperatorType *ot);
void OBJECT_OT_modifier_set_active(wmOperatorType *ot);
void OBJECT_OT_multires_subdivide(wmOperatorType *ot);
void OBJECT_OT_multires_reshape(wmOperatorType *ot);
void OBJECT_OT_multires_higher_levels_delete(wmOperatorType *ot);
void OBJECT_OT_multires_base_apply(wmOperatorType *ot);
void OBJECT_OT_multires_unsubdivide(wmOperatorType *ot);
void OBJECT_OT_multires_rebuild_subdiv(wmOperatorType *ot);
void OBJECT_OT_multires_external_save(wmOperatorType *ot);
void OBJECT_OT_multires_external_pack(wmOperatorType *ot);
void OBJECT_OT_correctivesmooth_bind(wmOperatorType *ot);
void OBJECT_OT_meshdeform_bind(wmOperatorType *ot);
void OBJECT_OT_explode_refresh(wmOperatorType *ot);
void OBJECT_OT_ocean_bake(wmOperatorType *ot);
void OBJECT_OT_skin_root_mark(wmOperatorType *ot);
void OBJECT_OT_skin_loose_mark_clear(wmOperatorType *ot);
void OBJECT_OT_skin_radii_equalize(wmOperatorType *ot);
void OBJECT_OT_skin_armature_create(wmOperatorType *ot);
void OBJECT_OT_laplaciandeform_bind(wmOperatorType *ot);
void OBJECT_OT_surfacedeform_bind(wmOperatorType *ot);
void OBJECT_OT_geometry_nodes_input_attribute_toggle(wmOperatorType *ot);
void OBJECT_OT_geometry_node_tree_copy_assign(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_dash_modifier_segment_add(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_dash_modifier_segment_remove(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_dash_modifier_segment_move(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_time_modifier_segment_add(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_time_modifier_segment_remove(wmOperatorType *ot);
void OBJECT_OT_grease_pencil_time_modifier_segment_move(wmOperatorType *ot);

/* `object_shader_fx.cc` */

void OBJECT_OT_shaderfx_add(wmOperatorType *ot);
void OBJECT_OT_shaderfx_copy(wmOperatorType *ot);
void OBJECT_OT_shaderfx_remove(wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_up(wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_down(wmOperatorType *ot);
void OBJECT_OT_shaderfx_move_to_index(wmOperatorType *ot);

/* `object_constraint.cc` */

void OBJECT_OT_constraint_add(wmOperatorType *ot);
void OBJECT_OT_constraint_add_with_targets(wmOperatorType *ot);
void POSE_OT_constraint_add(wmOperatorType *ot);
void POSE_OT_constraint_add_with_targets(wmOperatorType *ot);

void OBJECT_OT_constraints_copy(wmOperatorType *ot);
void POSE_OT_constraints_copy(wmOperatorType *ot);

void OBJECT_OT_constraints_clear(wmOperatorType *ot);
void POSE_OT_constraints_clear(wmOperatorType *ot);

void POSE_OT_ik_add(wmOperatorType *ot);
void POSE_OT_ik_clear(wmOperatorType *ot);

void CONSTRAINT_OT_delete(wmOperatorType *ot);
void CONSTRAINT_OT_apply(wmOperatorType *ot);
void CONSTRAINT_OT_copy(wmOperatorType *ot);
void CONSTRAINT_OT_copy_to_selected(wmOperatorType *ot);

void CONSTRAINT_OT_move_up(wmOperatorType *ot);
void CONSTRAINT_OT_move_to_index(wmOperatorType *ot);
void CONSTRAINT_OT_move_down(wmOperatorType *ot);

void CONSTRAINT_OT_stretchto_reset(wmOperatorType *ot);
void CONSTRAINT_OT_limitdistance_reset(wmOperatorType *ot);
void CONSTRAINT_OT_childof_set_inverse(wmOperatorType *ot);
void CONSTRAINT_OT_childof_clear_inverse(wmOperatorType *ot);
void CONSTRAINT_OT_objectsolver_set_inverse(wmOperatorType *ot);
void CONSTRAINT_OT_objectsolver_clear_inverse(wmOperatorType *ot);
void CONSTRAINT_OT_followpath_path_animate(wmOperatorType *ot);

/* object_vgroup.cc */

void OBJECT_OT_vertex_group_add(wmOperatorType *ot);
void OBJECT_OT_vertex_group_remove(wmOperatorType *ot);
void OBJECT_OT_vertex_group_assign(wmOperatorType *ot);
void OBJECT_OT_vertex_group_assign_new(wmOperatorType *ot);
void OBJECT_OT_vertex_group_remove_from(wmOperatorType *ot);
void OBJECT_OT_vertex_group_select(wmOperatorType *ot);
void OBJECT_OT_vertex_group_deselect(wmOperatorType *ot);
void OBJECT_OT_vertex_group_copy_to_selected(wmOperatorType *ot);
void OBJECT_OT_vertex_group_copy(wmOperatorType *ot);
void OBJECT_OT_vertex_group_normalize(wmOperatorType *ot);
void OBJECT_OT_vertex_group_normalize_all(wmOperatorType *ot);
void OBJECT_OT_vertex_group_levels(wmOperatorType *ot);
void OBJECT_OT_vertex_group_lock(wmOperatorType *ot);
void OBJECT_OT_vertex_group_invert(wmOperatorType *ot);
void OBJECT_OT_vertex_group_smooth(wmOperatorType *ot);
void OBJECT_OT_vertex_group_clean(wmOperatorType *ot);
void OBJECT_OT_vertex_group_quantize(wmOperatorType *ot);
void OBJECT_OT_vertex_group_limit_total(wmOperatorType *ot);
void OBJECT_OT_vertex_group_mirror(wmOperatorType *ot);
void OBJECT_OT_vertex_group_set_active(wmOperatorType *ot);
void OBJECT_OT_vertex_group_sort(wmOperatorType *ot);
void OBJECT_OT_vertex_group_move(wmOperatorType *ot);
void OBJECT_OT_vertex_weight_paste(wmOperatorType *ot);
void OBJECT_OT_vertex_weight_delete(wmOperatorType *ot);
void OBJECT_OT_vertex_weight_set_active(wmOperatorType *ot);
void OBJECT_OT_vertex_weight_normalize_active_vertex(wmOperatorType *ot);
void OBJECT_OT_vertex_weight_copy(wmOperatorType *ot);

/* `object_warp.cc` */

void TRANSFORM_OT_vertex_warp(wmOperatorType *ot);

/* `object_shapekey.cc` */

void OBJECT_OT_shape_key_add(wmOperatorType *ot);
void OBJECT_OT_shape_key_copy(wmOperatorType *ot);
void OBJECT_OT_shape_key_remove(wmOperatorType *ot);
void OBJECT_OT_shape_key_clear(wmOperatorType *ot);
void OBJECT_OT_shape_key_retime(wmOperatorType *ot);
void OBJECT_OT_shape_key_mirror(wmOperatorType *ot);
void OBJECT_OT_shape_key_move(wmOperatorType *ot);
void OBJECT_OT_shape_key_lock(wmOperatorType *ot);
void OBJECT_OT_shape_key_make_basis(wmOperatorType *ot);

/* `object_collection.cc` */

void OBJECT_OT_collection_add(wmOperatorType *ot);
void OBJECT_OT_collection_link(wmOperatorType *ot);
void OBJECT_OT_collection_remove(wmOperatorType *ot);
void OBJECT_OT_collection_unlink(wmOperatorType *ot);
void OBJECT_OT_collection_objects_select(wmOperatorType *ot);

/* `object_bake.cc` */

void OBJECT_OT_bake_image(wmOperatorType *ot);
void OBJECT_OT_bake(wmOperatorType *ot);

/* object_bake_simulation.cc */

namespace bake_simulation {

void OBJECT_OT_simulation_nodes_cache_calculate_to_frame(wmOperatorType *ot);
void OBJECT_OT_simulation_nodes_cache_bake(wmOperatorType *ot);
void OBJECT_OT_simulation_nodes_cache_delete(wmOperatorType *ot);
void OBJECT_OT_geometry_node_bake_single(wmOperatorType *ot);
void OBJECT_OT_geometry_node_bake_delete_single(wmOperatorType *ot);
void OBJECT_OT_geometry_node_bake_pack_single(wmOperatorType *ot);
void OBJECT_OT_geometry_node_bake_unpack_single(wmOperatorType *ot);

}  // namespace bake_simulation

/* `object_random.cc` */

void TRANSFORM_OT_vertex_random(wmOperatorType *ot);

/* object_remesh.cc */

void OBJECT_OT_voxel_remesh(wmOperatorType *ot);
void OBJECT_OT_voxel_size_edit(wmOperatorType *ot);
void OBJECT_OT_quadriflow_remesh(wmOperatorType *ot);

/* object_transfer_data.c */

/**
 * Transfer mesh data from active to selected objects.
 */
void OBJECT_OT_data_transfer(wmOperatorType *ot);
void OBJECT_OT_datalayout_transfer(wmOperatorType *ot);

void object_modifier_add_asset_register();

void collection_exporter_register();

Vector<PointerRNA> modifier_get_edit_objects(const bContext &C, const wmOperator &op);
void modifier_register_use_selected_objects_prop(wmOperatorType *ot);

/* object_visual_geometry_to_objects.cc */
void OBJECT_OT_visual_geometry_to_objects(wmOperatorType *ot);

}  // namespace blender::ed::object
