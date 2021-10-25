/*
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RNA_ENUM_TYPES_H__
#define __RNA_ENUM_TYPES_H__

/** \file RNA_enum_types.h
 *  \ingroup RNA
 */

#include "RNA_types.h"

struct bNodeTreeType;
struct bNodeType;
struct bNodeSocketType;

/* Types */

/* use in cases where only dynamic types are used */
extern EnumPropertyItem DummyRNA_NULL_items[];
extern EnumPropertyItem DummyRNA_DEFAULT_items[];

/* all others should follow 'rna_enum_*_items' naming */
extern EnumPropertyItem rna_enum_id_type_items[];

extern EnumPropertyItem rna_enum_object_mode_items[];
extern EnumPropertyItem rna_enum_object_empty_drawtype_items[];
extern EnumPropertyItem rna_enum_metaelem_type_items[];

extern EnumPropertyItem rna_enum_proportional_falloff_items[];
extern EnumPropertyItem rna_enum_proportional_falloff_curve_only_items[];
extern EnumPropertyItem rna_enum_proportional_editing_items[];
extern EnumPropertyItem rna_enum_snap_target_items[];
extern EnumPropertyItem rna_enum_snap_element_items[];
extern EnumPropertyItem rna_enum_snap_node_element_items[];
extern EnumPropertyItem rna_enum_curve_fit_method_items[];
extern EnumPropertyItem rna_enum_mesh_select_mode_items[];
extern EnumPropertyItem rna_enum_mesh_delimit_mode_items[];
extern EnumPropertyItem rna_enum_space_type_items[];
extern EnumPropertyItem rna_enum_region_type_items[];
extern EnumPropertyItem rna_enum_object_modifier_type_items[];
extern EnumPropertyItem rna_enum_constraint_type_items[];
extern EnumPropertyItem rna_enum_boidrule_type_items[];
extern EnumPropertyItem rna_enum_sequence_modifier_type_items[];

extern EnumPropertyItem rna_enum_modifier_triangulate_quad_method_items[];
extern EnumPropertyItem rna_enum_modifier_triangulate_ngon_method_items[];

extern EnumPropertyItem rna_enum_image_type_items[];
extern EnumPropertyItem rna_enum_image_color_mode_items[];
extern EnumPropertyItem rna_enum_image_color_depth_items[];
extern EnumPropertyItem rna_enum_image_generated_type_items[];

extern EnumPropertyItem rna_enum_normal_space_items[];
extern EnumPropertyItem rna_enum_normal_swizzle_items[];
extern EnumPropertyItem rna_enum_bake_save_mode_items[];

extern EnumPropertyItem rna_enum_views_format_items[];
extern EnumPropertyItem rna_enum_views_format_multilayer_items[];
extern EnumPropertyItem rna_enum_views_format_multiview_items[];
extern EnumPropertyItem rna_enum_stereo3d_display_items[];
extern EnumPropertyItem rna_enum_stereo3d_anaglyph_type_items[];
extern EnumPropertyItem rna_enum_stereo3d_interlace_type_items[];

extern EnumPropertyItem rna_enum_exr_codec_items[];
extern EnumPropertyItem rna_enum_color_sets_items[];

extern EnumPropertyItem rna_enum_beztriple_keyframe_type_items[];
extern EnumPropertyItem rna_enum_beztriple_interpolation_mode_items[];
extern EnumPropertyItem rna_enum_beztriple_interpolation_easing_items[];
extern EnumPropertyItem rna_enum_keyframe_handle_type_items[];

extern EnumPropertyItem rna_enum_keyblock_type_items[];

extern EnumPropertyItem rna_enum_keyingset_path_grouping_items[];
extern EnumPropertyItem rna_enum_keying_flag_items[];

extern EnumPropertyItem rna_enum_keyframe_paste_offset_items[];
extern EnumPropertyItem rna_enum_keyframe_paste_merge_items[];

extern EnumPropertyItem rna_enum_fmodifier_type_items[];

extern EnumPropertyItem rna_enum_nla_mode_extend_items[];
extern EnumPropertyItem rna_enum_nla_mode_blend_items[];

extern EnumPropertyItem rna_enum_motionpath_bake_location_items[];

extern EnumPropertyItem rna_enum_event_value_items[];
extern EnumPropertyItem rna_enum_event_type_items[];
extern EnumPropertyItem rna_enum_operator_return_items[];

extern EnumPropertyItem rna_enum_brush_sculpt_tool_items[];
extern EnumPropertyItem rna_enum_brush_vertex_tool_items[];
extern EnumPropertyItem rna_enum_brush_image_tool_items[];

extern EnumPropertyItem rna_enum_gpencil_sculpt_brush_items[];

extern EnumPropertyItem rna_enum_uv_sculpt_tool_items[];

extern EnumPropertyItem rna_enum_axis_xy_items[];
extern EnumPropertyItem rna_enum_axis_xyz_items[];

extern EnumPropertyItem rna_enum_axis_flag_xyz_items[];

extern EnumPropertyItem rna_enum_symmetrize_direction_items[];

extern EnumPropertyItem rna_enum_texture_type_items[];

extern EnumPropertyItem rna_enum_lamp_type_items[];

extern EnumPropertyItem rna_enum_unpack_method_items[];

extern EnumPropertyItem rna_enum_object_type_items[];

extern EnumPropertyItem rna_enum_object_type_curve_items[];

extern EnumPropertyItem rna_enum_rigidbody_object_type_items[];
extern EnumPropertyItem rna_enum_rigidbody_object_shape_items[];
extern EnumPropertyItem rna_enum_rigidbody_constraint_type_items[];

extern EnumPropertyItem rna_enum_object_axis_items[];

extern EnumPropertyItem rna_enum_controller_type_items[];

extern EnumPropertyItem rna_enum_render_pass_type_items[];
extern EnumPropertyItem rna_enum_render_pass_debug_type_items[];

extern EnumPropertyItem rna_enum_bake_pass_type_items[];
extern EnumPropertyItem rna_enum_bake_pass_filter_type_items[];

extern EnumPropertyItem rna_enum_keymap_propvalue_items[];

extern EnumPropertyItem rna_enum_operator_context_items[];

extern EnumPropertyItem rna_enum_wm_report_items[];

extern EnumPropertyItem rna_enum_transform_mode_types[];

extern EnumPropertyItem rna_enum_posebone_rotmode_items[];

extern EnumPropertyItem rna_enum_property_type_items[];
extern EnumPropertyItem rna_enum_property_subtype_items[];
extern EnumPropertyItem rna_enum_property_unit_items[];

extern EnumPropertyItem rna_enum_gameproperty_type_items[];

extern EnumPropertyItem rna_enum_viewport_shade_items[];

extern EnumPropertyItem rna_enum_navigation_mode_items[];

extern EnumPropertyItem rna_enum_file_sort_items[];

extern EnumPropertyItem rna_enum_node_socket_in_out_items[];
extern EnumPropertyItem rna_enum_node_icon_items[];

extern EnumPropertyItem rna_enum_node_math_items[];
extern EnumPropertyItem rna_enum_node_vec_math_items[];
extern EnumPropertyItem rna_enum_node_filter_items[];

extern EnumPropertyItem rna_enum_ramp_blend_items[];

extern EnumPropertyItem rna_enum_prop_dynamicpaint_type_items[];

extern EnumPropertyItem rna_enum_clip_editor_mode_items[];

extern EnumPropertyItem rna_enum_icon_items[];
extern EnumPropertyItem rna_enum_uilist_layout_type_items[];

extern EnumPropertyItem rna_enum_linestyle_color_modifier_type_items[];
extern EnumPropertyItem rna_enum_linestyle_alpha_modifier_type_items[];
extern EnumPropertyItem rna_enum_linestyle_thickness_modifier_type_items[];
extern EnumPropertyItem rna_enum_linestyle_geometry_modifier_type_items[];

extern EnumPropertyItem rna_enum_window_cursor_items[];

extern EnumPropertyItem rna_enum_dt_method_vertex_items[];
extern EnumPropertyItem rna_enum_dt_method_edge_items[];
extern EnumPropertyItem rna_enum_dt_method_loop_items[];
extern EnumPropertyItem rna_enum_dt_method_poly_items[];
extern EnumPropertyItem rna_enum_dt_mix_mode_items[];
extern EnumPropertyItem rna_enum_dt_layers_select_src_items[];
extern EnumPropertyItem rna_enum_dt_layers_select_dst_items[];

extern EnumPropertyItem rna_enum_abc_compression_items[];


/* API calls */
int rna_node_tree_type_to_enum(struct bNodeTreeType *typeinfo);
int rna_node_tree_idname_to_enum(const char *idname);
struct bNodeTreeType *rna_node_tree_type_from_enum(int value);
EnumPropertyItem *rna_node_tree_type_itemf(void *data, int (*poll)(void *data, struct bNodeTreeType *), bool *r_free);

int rna_node_type_to_enum(struct bNodeType *typeinfo);
int rna_node_idname_to_enum(const char *idname);
struct bNodeType *rna_node_type_from_enum(int value);
EnumPropertyItem *rna_node_type_itemf(void *data, int (*poll)(void *data, struct bNodeType *), bool *r_free);

int rna_node_socket_type_to_enum(struct bNodeSocketType *typeinfo);
int rna_node_socket_idname_to_enum(const char *idname);
struct bNodeSocketType *rna_node_socket_type_from_enum(int value);
EnumPropertyItem *rna_node_socket_type_itemf(void *data, int (*poll)(void *data, struct bNodeSocketType *), bool *r_free);

struct bContext;
struct PointerRNA;
struct PropertyRNA;
EnumPropertyItem *rna_TransformOrientation_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *rna_Sensor_type_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *rna_Actuator_type_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);

/* Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed */
EnumPropertyItem *RNA_action_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
// EnumPropertyItem *RNA_action_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_group_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_group_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_image_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_image_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_scene_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_scene_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_movieclip_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_movieclip_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_mask_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);
EnumPropertyItem *RNA_mask_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, bool *r_free);

#endif /* __RNA_ENUM_TYPES_H__ */
