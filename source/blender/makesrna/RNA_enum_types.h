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

extern EnumPropertyItem id_type_items[];

/* use in cases where only dynamic types are used */
extern EnumPropertyItem DummyRNA_NULL_items[];
extern EnumPropertyItem DummyRNA_DEFAULT_items[];

extern EnumPropertyItem object_mode_items[];
extern EnumPropertyItem object_empty_drawtype_items[];
extern EnumPropertyItem metaelem_type_items[];

extern EnumPropertyItem proportional_falloff_items[];
extern EnumPropertyItem proportional_falloff_curve_only_items[];
extern EnumPropertyItem proportional_editing_items[];
extern EnumPropertyItem snap_target_items[];
extern EnumPropertyItem snap_element_items[];
extern EnumPropertyItem snap_node_element_items[];
extern EnumPropertyItem mesh_select_mode_items[];
extern EnumPropertyItem mesh_delimit_mode_items[];
extern EnumPropertyItem space_type_items[];
extern EnumPropertyItem region_type_items[];
extern EnumPropertyItem modifier_type_items[];
extern EnumPropertyItem constraint_type_items[];
extern EnumPropertyItem boidrule_type_items[];
extern EnumPropertyItem sequence_modifier_type_items[];

extern EnumPropertyItem modifier_triangulate_quad_method_items[];
extern EnumPropertyItem modifier_triangulate_ngon_method_items[];

extern EnumPropertyItem image_type_items[];
extern EnumPropertyItem image_color_mode_items[];
extern EnumPropertyItem image_color_depth_items[];
extern EnumPropertyItem image_generated_type_items[];

extern EnumPropertyItem normal_space_items[];
extern EnumPropertyItem normal_swizzle_items[];
extern EnumPropertyItem bake_save_mode_items[];

extern EnumPropertyItem exr_codec_items[];
extern EnumPropertyItem color_sets_items[];

extern EnumPropertyItem beztriple_keyframe_type_items[];
extern EnumPropertyItem beztriple_interpolation_mode_items[];
extern EnumPropertyItem beztriple_interpolation_easing_items[];
extern EnumPropertyItem keyframe_handle_type_items[];

extern EnumPropertyItem keyblock_type_items[];

extern EnumPropertyItem keyingset_path_grouping_items[];
extern EnumPropertyItem keying_flag_items[];

extern EnumPropertyItem keyframe_paste_offset_items[];
extern EnumPropertyItem keyframe_paste_merge_items[];

extern EnumPropertyItem fmodifier_type_items[];

extern EnumPropertyItem nla_mode_extend_items[];
extern EnumPropertyItem nla_mode_blend_items[];

extern EnumPropertyItem motionpath_bake_location_items[];

extern EnumPropertyItem event_value_items[];
extern EnumPropertyItem event_type_items[];
extern EnumPropertyItem operator_return_items[];

extern EnumPropertyItem brush_sculpt_tool_items[];
extern EnumPropertyItem brush_vertex_tool_items[];
extern EnumPropertyItem brush_image_tool_items[];

extern EnumPropertyItem symmetrize_direction_items[];

extern EnumPropertyItem texture_type_items[];

extern EnumPropertyItem lamp_type_items[];

extern EnumPropertyItem unpack_method_items[];

extern EnumPropertyItem object_type_items[];

extern EnumPropertyItem object_type_curve_items[];

extern EnumPropertyItem rigidbody_object_type_items[];
extern EnumPropertyItem rigidbody_object_shape_items[];
extern EnumPropertyItem rigidbody_constraint_type_items[];

extern EnumPropertyItem object_axis_items[];
extern EnumPropertyItem object_axis_unsigned_items[];

extern EnumPropertyItem controller_type_items[];

extern EnumPropertyItem render_pass_type_items[];

extern EnumPropertyItem keymap_propvalue_items[];

extern EnumPropertyItem operator_context_items[];

extern EnumPropertyItem wm_report_items[];

extern EnumPropertyItem transform_mode_types[];

extern EnumPropertyItem posebone_rotmode_items[];

extern EnumPropertyItem property_type_items[];
extern EnumPropertyItem property_subtype_items[];
extern EnumPropertyItem property_unit_items[];

extern EnumPropertyItem gameproperty_type_items[];

extern EnumPropertyItem viewport_shade_items[];

extern EnumPropertyItem navigation_mode_items[];

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

extern EnumPropertyItem node_socket_in_out_items[];
extern EnumPropertyItem node_icon_items[];

extern EnumPropertyItem node_math_items[];
extern EnumPropertyItem node_vec_math_items[];
extern EnumPropertyItem node_filter_items[];

extern EnumPropertyItem ramp_blend_items[];

extern EnumPropertyItem prop_dynamicpaint_type_items[];

extern EnumPropertyItem clip_editor_mode_items[];

extern EnumPropertyItem icon_items[];
extern EnumPropertyItem uilist_layout_type_items[];

extern EnumPropertyItem linestyle_color_modifier_type_items[];
extern EnumPropertyItem linestyle_alpha_modifier_type_items[];
extern EnumPropertyItem linestyle_thickness_modifier_type_items[];
extern EnumPropertyItem linestyle_geometry_modifier_type_items[];

extern EnumPropertyItem window_cursor_items[];

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
