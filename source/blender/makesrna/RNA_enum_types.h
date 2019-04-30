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
 */

#ifndef __RNA_ENUM_TYPES_H__
#define __RNA_ENUM_TYPES_H__

/** \file
 * \ingroup RNA
 */

#include "RNA_types.h"

struct bNodeSocketType;
struct bNodeTreeType;
struct bNodeType;

/* Types */

/* use in cases where only dynamic types are used */
extern const EnumPropertyItem DummyRNA_NULL_items[];
extern const EnumPropertyItem DummyRNA_DEFAULT_items[];

/* all others should follow 'rna_enum_*_items' naming */
extern const EnumPropertyItem rna_enum_id_type_items[];

extern const EnumPropertyItem rna_enum_object_mode_items[];
extern const EnumPropertyItem rna_enum_workspace_object_mode_items[];
extern const EnumPropertyItem rna_enum_object_empty_drawtype_items[];
extern const EnumPropertyItem rna_enum_object_gpencil_type_items[];
extern const EnumPropertyItem rna_enum_metaelem_type_items[];

extern const EnumPropertyItem rna_enum_proportional_falloff_items[];
extern const EnumPropertyItem rna_enum_proportional_falloff_curve_only_items[];
extern const EnumPropertyItem rna_enum_snap_target_items[];
extern const EnumPropertyItem rna_enum_snap_element_items[];
extern const EnumPropertyItem rna_enum_snap_node_element_items[];
extern const EnumPropertyItem rna_enum_curve_fit_method_items[];
extern const EnumPropertyItem rna_enum_mesh_select_mode_items[];
extern const EnumPropertyItem rna_enum_mesh_select_mode_uv_items[];
extern const EnumPropertyItem rna_enum_mesh_delimit_mode_items[];
extern const EnumPropertyItem rna_enum_space_graph_mode_items[];
extern const EnumPropertyItem rna_enum_space_type_items[];
extern const EnumPropertyItem rna_enum_space_image_mode_items[];
extern const EnumPropertyItem rna_enum_space_image_mode_all_items[];
extern const EnumPropertyItem rna_enum_space_action_mode_items[];
extern const EnumPropertyItem rna_enum_region_type_items[];
extern const EnumPropertyItem rna_enum_object_modifier_type_items[];
extern const EnumPropertyItem rna_enum_constraint_type_items[];
extern const EnumPropertyItem rna_enum_boidrule_type_items[];
extern const EnumPropertyItem rna_enum_sequence_modifier_type_items[];
extern const EnumPropertyItem rna_enum_object_greasepencil_modifier_type_items[];
extern const EnumPropertyItem rna_enum_object_shaderfx_type_items[];

extern const EnumPropertyItem rna_enum_modifier_triangulate_quad_method_items[];
extern const EnumPropertyItem rna_enum_modifier_triangulate_ngon_method_items[];
extern const EnumPropertyItem rna_enum_modifier_shrinkwrap_mode_items[];

extern const EnumPropertyItem rna_enum_image_type_items[];
extern const EnumPropertyItem rna_enum_image_color_mode_items[];
extern const EnumPropertyItem rna_enum_image_color_depth_items[];
extern const EnumPropertyItem rna_enum_image_generated_type_items[];

extern const EnumPropertyItem rna_enum_normal_space_items[];
extern const EnumPropertyItem rna_enum_normal_swizzle_items[];
extern const EnumPropertyItem rna_enum_bake_save_mode_items[];

extern const EnumPropertyItem rna_enum_views_format_items[];
extern const EnumPropertyItem rna_enum_views_format_multilayer_items[];
extern const EnumPropertyItem rna_enum_views_format_multiview_items[];
extern const EnumPropertyItem rna_enum_stereo3d_display_items[];
extern const EnumPropertyItem rna_enum_stereo3d_anaglyph_type_items[];
extern const EnumPropertyItem rna_enum_stereo3d_interlace_type_items[];

extern const EnumPropertyItem rna_enum_exr_codec_items[];
extern const EnumPropertyItem rna_enum_color_sets_items[];

extern const EnumPropertyItem rna_enum_beztriple_keyframe_type_items[];
extern const EnumPropertyItem rna_enum_beztriple_interpolation_mode_items[];
extern const EnumPropertyItem rna_enum_beztriple_interpolation_easing_items[];
extern const EnumPropertyItem rna_enum_keyframe_handle_type_items[];

extern const EnumPropertyItem rna_enum_keyblock_type_items[];

extern const EnumPropertyItem rna_enum_keyingset_path_grouping_items[];
extern const EnumPropertyItem rna_enum_keying_flag_items[];

extern const EnumPropertyItem rna_enum_keyframe_paste_offset_items[];
extern const EnumPropertyItem rna_enum_keyframe_paste_merge_items[];

extern const EnumPropertyItem rna_enum_fmodifier_type_items[];

extern const EnumPropertyItem rna_enum_nla_mode_extend_items[];
extern const EnumPropertyItem rna_enum_nla_mode_blend_items[];

extern const EnumPropertyItem rna_enum_motionpath_bake_location_items[];

extern const EnumPropertyItem rna_enum_event_value_items[];
extern const EnumPropertyItem rna_enum_event_type_items[];
extern const EnumPropertyItem rna_enum_event_type_mask_items[];
extern const EnumPropertyItem rna_enum_operator_return_items[];
extern const EnumPropertyItem rna_enum_operator_property_tags[];

extern const EnumPropertyItem rna_enum_brush_sculpt_tool_items[];
extern const EnumPropertyItem rna_enum_brush_vertex_tool_items[];
extern const EnumPropertyItem rna_enum_brush_weight_tool_items[];
extern const EnumPropertyItem rna_enum_brush_gpencil_types_items[];
extern const EnumPropertyItem rna_enum_brush_image_tool_items[];

extern const EnumPropertyItem rna_enum_particle_edit_hair_brush_items[];
extern const EnumPropertyItem rna_enum_particle_edit_disconnected_hair_brush_items[];
extern const EnumPropertyItem rna_enum_gpencil_sculpt_brush_items[];
extern const EnumPropertyItem rna_enum_gpencil_weight_brush_items[];

extern const EnumPropertyItem rna_enum_uv_sculpt_tool_items[];

extern const EnumPropertyItem rna_enum_axis_xy_items[];
extern const EnumPropertyItem rna_enum_axis_xyz_items[];

extern const EnumPropertyItem rna_enum_axis_flag_xyz_items[];

extern const EnumPropertyItem rna_enum_symmetrize_direction_items[];

extern const EnumPropertyItem rna_enum_texture_type_items[];

extern const EnumPropertyItem rna_enum_light_type_items[];

extern const EnumPropertyItem rna_enum_unpack_method_items[];

extern const EnumPropertyItem rna_enum_object_type_items[];
extern const EnumPropertyItem rna_enum_object_rotation_mode_items[];

extern const EnumPropertyItem rna_enum_object_type_curve_items[];

extern const EnumPropertyItem rna_enum_rigidbody_object_type_items[];
extern const EnumPropertyItem rna_enum_rigidbody_object_shape_items[];
extern const EnumPropertyItem rna_enum_rigidbody_constraint_type_items[];

extern const EnumPropertyItem rna_enum_object_axis_items[];

extern const EnumPropertyItem rna_enum_controller_type_items[];

extern const EnumPropertyItem rna_enum_render_pass_type_items[];
extern const EnumPropertyItem rna_enum_render_pass_debug_type_items[];

extern const EnumPropertyItem rna_enum_bake_pass_type_items[];
extern const EnumPropertyItem rna_enum_bake_pass_filter_type_items[];

extern const EnumPropertyItem rna_enum_keymap_propvalue_items[];

extern const EnumPropertyItem rna_enum_operator_context_items[];

extern const EnumPropertyItem rna_enum_wm_report_items[];

extern const EnumPropertyItem rna_enum_transform_pivot_items_full[];
extern const EnumPropertyItem rna_enum_transform_orientation_items[];
extern const EnumPropertyItem rna_enum_transform_mode_types[];

extern const EnumPropertyItem rna_enum_property_type_items[];
extern const EnumPropertyItem rna_enum_property_subtype_items[];
extern const EnumPropertyItem rna_enum_property_unit_items[];

extern const EnumPropertyItem rna_enum_shading_type_items[];

extern const EnumPropertyItem rna_enum_navigation_mode_items[];

extern const EnumPropertyItem rna_enum_file_sort_items[];

extern const EnumPropertyItem rna_enum_node_socket_in_out_items[];

extern const EnumPropertyItem rna_enum_node_math_items[];
extern const EnumPropertyItem rna_enum_node_vec_math_items[];
extern const EnumPropertyItem rna_enum_node_filter_items[];

extern const EnumPropertyItem rna_enum_ramp_blend_items[];

extern const EnumPropertyItem rna_enum_prop_dynamicpaint_type_items[];

extern const EnumPropertyItem rna_enum_clip_editor_mode_items[];

extern const EnumPropertyItem rna_enum_icon_items[];
extern const EnumPropertyItem rna_enum_uilist_layout_type_items[];

extern const EnumPropertyItem rna_enum_linestyle_color_modifier_type_items[];
extern const EnumPropertyItem rna_enum_linestyle_alpha_modifier_type_items[];
extern const EnumPropertyItem rna_enum_linestyle_thickness_modifier_type_items[];
extern const EnumPropertyItem rna_enum_linestyle_geometry_modifier_type_items[];

extern const EnumPropertyItem rna_enum_window_cursor_items[];

extern const EnumPropertyItem rna_enum_dt_method_vertex_items[];
extern const EnumPropertyItem rna_enum_dt_method_edge_items[];
extern const EnumPropertyItem rna_enum_dt_method_loop_items[];
extern const EnumPropertyItem rna_enum_dt_method_poly_items[];
extern const EnumPropertyItem rna_enum_dt_mix_mode_items[];
extern const EnumPropertyItem rna_enum_dt_layers_select_src_items[];
extern const EnumPropertyItem rna_enum_dt_layers_select_dst_items[];

extern const EnumPropertyItem rna_enum_abc_compression_items[];
extern const EnumPropertyItem rna_enum_context_mode_items[];

/* API calls */
int rna_node_tree_type_to_enum(struct bNodeTreeType *typeinfo);
int rna_node_tree_idname_to_enum(const char *idname);
struct bNodeTreeType *rna_node_tree_type_from_enum(int value);
const EnumPropertyItem *rna_node_tree_type_itemf(void *data,
                                                 bool (*poll)(void *data, struct bNodeTreeType *),
                                                 bool *r_free);

int rna_node_type_to_enum(struct bNodeType *typeinfo);
int rna_node_idname_to_enum(const char *idname);
struct bNodeType *rna_node_type_from_enum(int value);
const EnumPropertyItem *rna_node_type_itemf(void *data,
                                            bool (*poll)(void *data, struct bNodeType *),
                                            bool *r_free);

int rna_node_socket_type_to_enum(struct bNodeSocketType *typeinfo);
int rna_node_socket_idname_to_enum(const char *idname);
struct bNodeSocketType *rna_node_socket_type_from_enum(int value);
const EnumPropertyItem *rna_node_socket_type_itemf(
    void *data, bool (*poll)(void *data, struct bNodeSocketType *), bool *r_free);

struct PointerRNA;
struct PropertyRNA;
struct bContext;

const EnumPropertyItem *rna_TransformOrientation_itemf(struct bContext *C,
                                                       struct PointerRNA *ptr,
                                                       struct PropertyRNA *prop,
                                                       bool *r_free);

/* Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed */
const EnumPropertyItem *RNA_action_itemf(struct bContext *C,
                                         struct PointerRNA *ptr,
                                         struct PropertyRNA *prop,
                                         bool *r_free);
#if 0
EnumPropertyItem *RNA_action_local_itemf(struct bContext *C,
                                         struct PointerRNA *ptr,
                                         struct PropertyRNA *prop,
                                         bool *r_free);
#endif
const EnumPropertyItem *RNA_collection_itemf(struct bContext *C,
                                             struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             bool *r_free);
const EnumPropertyItem *RNA_collection_local_itemf(struct bContext *C,
                                                   struct PointerRNA *ptr,
                                                   struct PropertyRNA *prop,
                                                   bool *r_free);
const EnumPropertyItem *RNA_image_itemf(struct bContext *C,
                                        struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_image_local_itemf(struct bContext *C,
                                              struct PointerRNA *ptr,
                                              struct PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_scene_itemf(struct bContext *C,
                                        struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_scene_without_active_itemf(struct bContext *C,
                                                       struct PointerRNA *ptr,
                                                       struct PropertyRNA *prop,
                                                       bool *r_free);
const EnumPropertyItem *RNA_scene_local_itemf(struct bContext *C,
                                              struct PointerRNA *ptr,
                                              struct PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_movieclip_itemf(struct bContext *C,
                                            struct PointerRNA *ptr,
                                            struct PropertyRNA *prop,
                                            bool *r_free);
const EnumPropertyItem *RNA_movieclip_local_itemf(struct bContext *C,
                                                  struct PointerRNA *ptr,
                                                  struct PropertyRNA *prop,
                                                  bool *r_free);
const EnumPropertyItem *RNA_mask_itemf(struct bContext *C,
                                       struct PointerRNA *ptr,
                                       struct PropertyRNA *prop,
                                       bool *r_free);
const EnumPropertyItem *RNA_mask_local_itemf(struct bContext *C,
                                             struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             bool *r_free);

/* Non confirming, utility function. */
const EnumPropertyItem *RNA_enum_node_tree_types_itemf_impl(struct bContext *C, bool *r_free);

#endif /* __RNA_ENUM_TYPES_H__ */
