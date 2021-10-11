/*
 * This program is free software you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup RNA
 */

/* NOTE: this is included multiple times with different #defines for DEF_ENUM. */

/* use in cases where only dynamic types are used */
DEF_ENUM(DummyRNA_NULL_items)
DEF_ENUM(DummyRNA_DEFAULT_items)

/* all others should follow 'rna_enum_*_items' naming */
DEF_ENUM(rna_enum_id_type_items)

DEF_ENUM(rna_enum_object_mode_items)
DEF_ENUM(rna_enum_workspace_object_mode_items)
DEF_ENUM(rna_enum_object_empty_drawtype_items)
DEF_ENUM(rna_enum_object_gpencil_type_items)
DEF_ENUM(rna_enum_metaelem_type_items)

DEF_ENUM(rna_enum_proportional_falloff_items)
DEF_ENUM(rna_enum_proportional_falloff_curve_only_items)
DEF_ENUM(rna_enum_snap_target_items)
DEF_ENUM(rna_enum_snap_element_items)
DEF_ENUM(rna_enum_snap_node_element_items)
DEF_ENUM(rna_enum_curve_fit_method_items)
DEF_ENUM(rna_enum_mesh_select_mode_items)
DEF_ENUM(rna_enum_mesh_select_mode_uv_items)
DEF_ENUM(rna_enum_mesh_delimit_mode_items)
DEF_ENUM(rna_enum_space_graph_mode_items)
DEF_ENUM(rna_enum_space_file_browse_mode_items)
DEF_ENUM(rna_enum_space_sequencer_view_type_items)
DEF_ENUM(rna_enum_space_type_items)
DEF_ENUM(rna_enum_space_image_mode_items)
DEF_ENUM(rna_enum_space_image_mode_all_items)
DEF_ENUM(rna_enum_space_action_mode_items)
DEF_ENUM(rna_enum_fileselect_params_sort_items)
DEF_ENUM(rna_enum_region_type_items)
DEF_ENUM(rna_enum_object_modifier_type_items)
DEF_ENUM(rna_enum_constraint_type_items)
DEF_ENUM(rna_enum_boidrule_type_items)
DEF_ENUM(rna_enum_sequence_modifier_type_items)
DEF_ENUM(rna_enum_object_greasepencil_modifier_type_items)
DEF_ENUM(rna_enum_object_shaderfx_type_items)

DEF_ENUM(rna_enum_modifier_triangulate_quad_method_items)
DEF_ENUM(rna_enum_modifier_triangulate_ngon_method_items)
DEF_ENUM(rna_enum_modifier_shrinkwrap_mode_items)

DEF_ENUM(rna_enum_image_type_items)
DEF_ENUM(rna_enum_image_color_mode_items)
DEF_ENUM(rna_enum_image_color_depth_items)
DEF_ENUM(rna_enum_image_generated_type_items)

DEF_ENUM(rna_enum_normal_space_items)
DEF_ENUM(rna_enum_normal_swizzle_items)
DEF_ENUM(rna_enum_bake_save_mode_items)
DEF_ENUM(rna_enum_bake_target_items)

DEF_ENUM(rna_enum_views_format_items)
DEF_ENUM(rna_enum_views_format_multilayer_items)
DEF_ENUM(rna_enum_views_format_multiview_items)
DEF_ENUM(rna_enum_stereo3d_display_items)
DEF_ENUM(rna_enum_stereo3d_anaglyph_type_items)
DEF_ENUM(rna_enum_stereo3d_interlace_type_items)

#ifdef WITH_OPENEXR
DEF_ENUM(rna_enum_exr_codec_items)
#endif
DEF_ENUM(rna_enum_color_sets_items)

DEF_ENUM(rna_enum_beztriple_keyframe_type_items)
DEF_ENUM(rna_enum_beztriple_interpolation_mode_items)
DEF_ENUM(rna_enum_beztriple_interpolation_easing_items)
DEF_ENUM(rna_enum_fcurve_auto_smoothing_items)
DEF_ENUM(rna_enum_keyframe_handle_type_items)
DEF_ENUM(rna_enum_driver_target_rotation_mode_items)

DEF_ENUM(rna_enum_keyingset_path_grouping_items)
DEF_ENUM(rna_enum_keying_flag_items)
DEF_ENUM(rna_enum_keying_flag_items_api)

DEF_ENUM(rna_enum_fmodifier_type_items)

DEF_ENUM(rna_enum_motionpath_bake_location_items)

DEF_ENUM(rna_enum_event_value_all_items)
DEF_ENUM(rna_enum_event_value_keymouse_items)
DEF_ENUM(rna_enum_event_value_tweak_items)

DEF_ENUM(rna_enum_event_type_items)
DEF_ENUM(rna_enum_event_type_mask_items)

DEF_ENUM(rna_enum_operator_type_flag_items)
DEF_ENUM(rna_enum_operator_return_items)
DEF_ENUM(rna_enum_operator_property_tags)

DEF_ENUM(rna_enum_brush_sculpt_tool_items)
DEF_ENUM(rna_enum_brush_uv_sculpt_tool_items)
DEF_ENUM(rna_enum_brush_vertex_tool_items)
DEF_ENUM(rna_enum_brush_weight_tool_items)
DEF_ENUM(rna_enum_brush_gpencil_types_items)
DEF_ENUM(rna_enum_brush_gpencil_vertex_types_items)
DEF_ENUM(rna_enum_brush_gpencil_sculpt_types_items)
DEF_ENUM(rna_enum_brush_gpencil_weight_types_items)
DEF_ENUM(rna_enum_brush_image_tool_items)

DEF_ENUM(rna_enum_axis_xy_items)
DEF_ENUM(rna_enum_axis_xyz_items)

DEF_ENUM(rna_enum_axis_flag_xyz_items)

DEF_ENUM(rna_enum_symmetrize_direction_items)

DEF_ENUM(rna_enum_texture_type_items)

DEF_ENUM(rna_enum_light_type_items)

DEF_ENUM(rna_enum_lightprobes_type_items)

DEF_ENUM(rna_enum_unpack_method_items)

DEF_ENUM(rna_enum_object_type_items)
DEF_ENUM(rna_enum_object_rotation_mode_items)

DEF_ENUM(rna_enum_object_type_curve_items)

DEF_ENUM(rna_enum_rigidbody_object_type_items)
DEF_ENUM(rna_enum_rigidbody_object_shape_items)
DEF_ENUM(rna_enum_rigidbody_constraint_type_items)

DEF_ENUM(rna_enum_object_axis_items)

DEF_ENUM(rna_enum_render_pass_type_items)

DEF_ENUM(rna_enum_bake_pass_type_items)
DEF_ENUM(rna_enum_bake_pass_filter_type_items)

DEF_ENUM(rna_enum_keymap_propvalue_items)

DEF_ENUM(rna_enum_operator_context_items)

DEF_ENUM(rna_enum_wm_report_items)

DEF_ENUM(rna_enum_property_type_items)
DEF_ENUM(rna_enum_property_subtype_items)
DEF_ENUM(rna_enum_property_unit_items)

DEF_ENUM(rna_enum_shading_type_items)

DEF_ENUM(rna_enum_navigation_mode_items)

DEF_ENUM(rna_enum_node_socket_in_out_items)

DEF_ENUM(rna_enum_node_math_items)
DEF_ENUM(rna_enum_mapping_type_items)
DEF_ENUM(rna_enum_node_vec_math_items)
DEF_ENUM(rna_enum_node_boolean_math_items)
DEF_ENUM(rna_enum_node_float_compare_items)
DEF_ENUM(rna_enum_node_filter_items)
DEF_ENUM(rna_enum_node_float_to_int_items)
DEF_ENUM(rna_enum_node_map_range_items)
DEF_ENUM(rna_enum_node_clamp_items)

DEF_ENUM(rna_enum_ramp_blend_items)

DEF_ENUM(rna_enum_prop_dynamicpaint_type_items)

DEF_ENUM(rna_enum_clip_editor_mode_items)

DEF_ENUM(rna_enum_icon_items)
DEF_ENUM(rna_enum_uilist_layout_type_items)

DEF_ENUM(rna_enum_linestyle_color_modifier_type_items)
DEF_ENUM(rna_enum_linestyle_alpha_modifier_type_items)
DEF_ENUM(rna_enum_linestyle_thickness_modifier_type_items)
DEF_ENUM(rna_enum_linestyle_geometry_modifier_type_items)

DEF_ENUM(rna_enum_window_cursor_items)

DEF_ENUM(rna_enum_dt_method_vertex_items)
DEF_ENUM(rna_enum_dt_method_edge_items)
DEF_ENUM(rna_enum_dt_method_loop_items)
DEF_ENUM(rna_enum_dt_method_poly_items)
DEF_ENUM(rna_enum_dt_mix_mode_items)
DEF_ENUM(rna_enum_dt_layers_select_src_items)
DEF_ENUM(rna_enum_dt_layers_select_dst_items)

DEF_ENUM(rna_enum_context_mode_items)

DEF_ENUM(rna_enum_preference_section_items)

DEF_ENUM(rna_enum_attribute_type_items)
DEF_ENUM(rna_enum_attribute_type_with_auto_items)
DEF_ENUM(rna_enum_attribute_domain_items)
DEF_ENUM(rna_enum_attribute_domain_without_corner_items)
DEF_ENUM(rna_enum_attribute_domain_with_auto_items)

DEF_ENUM(rna_enum_collection_color_items)
DEF_ENUM(rna_enum_strip_color_items)

DEF_ENUM(rna_enum_subdivision_uv_smooth_items)
DEF_ENUM(rna_enum_subdivision_boundary_smooth_items)

DEF_ENUM(rna_enum_transform_orientation_items)

/* Not available to RNA pre-processing (`makrsrna`).
 * Defined in editors for example. */
#ifndef RNA_MAKESRNA

DEF_ENUM(rna_enum_particle_edit_hair_brush_items)
DEF_ENUM(rna_enum_particle_edit_disconnected_hair_brush_items)

DEF_ENUM(rna_enum_keyframe_paste_offset_items)
DEF_ENUM(rna_enum_keyframe_paste_merge_items)

DEF_ENUM(rna_enum_transform_pivot_items_full)
DEF_ENUM(rna_enum_transform_mode_types)

/* In the runtime part of RNA, could be removed from this section. */
DEF_ENUM(rna_enum_nla_mode_extend_items)
DEF_ENUM(rna_enum_nla_mode_blend_items)
DEF_ENUM(rna_enum_keyblock_type_items)

#endif

#undef DEF_ENUM
