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

/* Types */

extern EnumPropertyItem id_type_items[];

/* use in cases where only dynamic types are used */
extern EnumPropertyItem DummyRNA_NULL_items[];
extern EnumPropertyItem DummyRNA_DEFAULT_items[];

extern EnumPropertyItem object_mode_items[];
extern EnumPropertyItem metaelem_type_items[];

extern EnumPropertyItem proportional_falloff_items[];
extern EnumPropertyItem proportional_editing_items[];
extern EnumPropertyItem snap_target_items[];
extern EnumPropertyItem snap_element_items[];
extern EnumPropertyItem mesh_select_mode_items[];
extern EnumPropertyItem space_type_items[];
extern EnumPropertyItem region_type_items[];
extern EnumPropertyItem modifier_type_items[];
extern EnumPropertyItem constraint_type_items[];
extern EnumPropertyItem boidrule_type_items[];

extern EnumPropertyItem image_type_items[];
extern EnumPropertyItem image_color_mode_items[];
extern EnumPropertyItem image_depth_mode_items[];

extern EnumPropertyItem color_sets_items[];

extern EnumPropertyItem beztriple_keyframe_type_items[];
extern EnumPropertyItem beztriple_handle_type_items[];
extern EnumPropertyItem beztriple_interpolation_mode_items[];
extern EnumPropertyItem keyframe_handle_type_items[];

extern EnumPropertyItem keyblock_type_items[];

extern EnumPropertyItem keyingset_path_grouping_items[];

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

extern EnumPropertyItem texture_type_items[];

extern EnumPropertyItem lamp_type_items[];

extern EnumPropertyItem unpack_method_items[];

extern EnumPropertyItem object_type_items[];

extern EnumPropertyItem object_type_curve_items[];

extern EnumPropertyItem controller_type_items[];

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

extern EnumPropertyItem nodetree_type_items[];
extern EnumPropertyItem node_socket_type_items[];

extern EnumPropertyItem node_math_items[];
extern EnumPropertyItem node_vec_math_items[];
extern EnumPropertyItem node_filter_items[];

extern EnumPropertyItem ramp_blend_items[];

extern EnumPropertyItem prop_dynamicpaint_type_items[];

struct bContext;
struct PointerRNA;
struct PropertyRNA;
EnumPropertyItem *rna_TransformOrientation_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *rna_Sensor_type_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *rna_Actuator_type_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);

/* Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed */
EnumPropertyItem *RNA_action_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
// EnumPropertyItem *RNA_action_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_group_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_group_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_image_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_image_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_scene_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_scene_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_movieclip_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_movieclip_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_mask_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);
EnumPropertyItem *RNA_mask_local_itemf(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop, int *free);

#endif /* __RNA_ENUM_TYPES_H__ */
