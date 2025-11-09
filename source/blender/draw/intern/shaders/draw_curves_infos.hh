/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_attribute_shader_shared.hh"
#  include "draw_object_infos_infos.hh"

#  define DRW_HAIR_INFO
#endif

#include "draw_curves_defines.hh"

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_curves_topology)
LOCAL_GROUP_SIZE(CURVES_PER_THREADGROUP)
/* Offsets giving the start and end of the curve. */
STORAGE_BUF(0, read, int, evaluated_offsets_buf[])
STORAGE_BUF(1, read, uint, curves_cyclic_buf[]) /* Actually bool (1 byte). */
STORAGE_BUF(2, write, int, indirection_buf[])
PUSH_CONSTANT(int, curves_start)
PUSH_CONSTANT(int, curves_count)
PUSH_CONSTANT(bool, is_ribbon_topology)
PUSH_CONSTANT(bool, use_cyclic)
COMPUTE_SOURCE("draw_curves_topology_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_data)
LOCAL_GROUP_SIZE(CURVES_PER_THREADGROUP)
/* Offsets giving the start and end of the curve. */
STORAGE_BUF(EVALUATED_POINT_SLOT, read, int, evaluated_points_by_curve_buf[])
STORAGE_BUF(POINTS_BY_CURVES_SLOT, read, int, points_by_curve_buf[])
STORAGE_BUF(CURVE_RESOLUTION_SLOT, read, uint, curves_resolution_buf[])
STORAGE_BUF(CURVE_TYPE_SLOT, read, uint, curves_type_buf[])     /* Actually int8_t. */
STORAGE_BUF(CURVE_CYCLIC_SLOT, read, uint, curves_cyclic_buf[]) /* Actually bool (1 byte). */
/* Bezier handles (if needed). */
STORAGE_BUF(HANDLES_POS_LEFT_SLOT, read, float, handles_positions_left_buf[])
STORAGE_BUF(HANDLES_POS_RIGHT_SLOT, read, float, handles_positions_right_buf[])
STORAGE_BUF(BEZIER_OFFSETS_SLOT, read, int, bezier_offsets_buf[])
/* Nurbs (alias of other buffers). */
// STORAGE_BUF(CURVES_ORDER_SLOT, read, uint, curves_order_buf[])  /* Actually int8_t. */
// STORAGE_BUF(BASIS_CACHE_SLOT, read, float, basis_cache_buf[])
// STORAGE_BUF(CONTROL_WEIGHTS_SLOT, read, float, control_weights_buf[])
// STORAGE_BUF(BASIS_CACHE_OFFSET_SLOT, read, int, basis_cache_offset_buf[])
PUSH_CONSTANT(int, curves_start)
PUSH_CONSTANT(int, curves_count)
PUSH_CONSTANT(bool, use_point_weight)
PUSH_CONSTANT(bool, use_cyclic)
/** IMPORTANT: For very dumb reasons, on GL the default specialization is compiled and used for
 * creating the shader interface. If this happens to optimize out some push_constants that are
 * valid in other specialization, we will never be able to set them. So choose the specialization
 * that uses all push_constants. */
SPECIALIZATION_CONSTANT(int, evaluated_type, 3) /* CURVE_TYPE_NURBS */
TYPEDEF_SOURCE("draw_attribute_shader_shared.hh")
COMPUTE_SOURCE("draw_curves_interpolation_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_interpolate_position)
ADDITIONAL_INFO(draw_curves_data)
/* Attributes. */
STORAGE_BUF(POINT_POSITIONS_SLOT, read, float, positions_buf[])
STORAGE_BUF(POINT_RADII_SLOT, read, float, radii_buf[])
/* Outputs. */
STORAGE_BUF(EVALUATED_POS_RAD_SLOT, read_write, float4, evaluated_positions_radii_buf[])
PUSH_CONSTANT(float4x4, transform)
COMPUTE_FUNCTION("evaluate_position_radius")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_interpolate_float4_attribute)
ADDITIONAL_INFO(draw_curves_data)
STORAGE_BUF(POINT_ATTR_SLOT, read, StoredFloat4, attribute_float4_buf[])
STORAGE_BUF(EVALUATED_ATTR_SLOT, read_write, StoredFloat4, evaluated_float4_buf[])
COMPUTE_FUNCTION("evaluate_attribute_float4")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_interpolate_float3_attribute)
ADDITIONAL_INFO(draw_curves_data)
STORAGE_BUF(POINT_ATTR_SLOT, read, StoredFloat3, attribute_float3_buf[])
STORAGE_BUF(EVALUATED_ATTR_SLOT, read_write, StoredFloat3, evaluated_float3_buf[])
COMPUTE_FUNCTION("evaluate_attribute_float3")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_interpolate_float2_attribute)
ADDITIONAL_INFO(draw_curves_data)
STORAGE_BUF(POINT_ATTR_SLOT, read, StoredFloat2, attribute_float2_buf[])
STORAGE_BUF(EVALUATED_ATTR_SLOT, read_write, StoredFloat2, evaluated_float2_buf[])
COMPUTE_FUNCTION("evaluate_attribute_float2")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_interpolate_float_attribute)
ADDITIONAL_INFO(draw_curves_data)
STORAGE_BUF(POINT_ATTR_SLOT, read, StoredFloat, attribute_float_buf[])
STORAGE_BUF(EVALUATED_ATTR_SLOT, read_write, StoredFloat, evaluated_float_buf[])
COMPUTE_FUNCTION("evaluate_attribute_float")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_evaluate_length_intercept)
LOCAL_GROUP_SIZE(CURVES_PER_THREADGROUP)
STORAGE_BUF(EVALUATED_POINT_SLOT, read, int, evaluated_points_by_curve_buf[])
STORAGE_BUF(EVALUATED_POS_RAD_SLOT, read, float4, evaluated_positions_radii_buf[])
STORAGE_BUF(EVALUATED_TIME_SLOT, read_write, float, evaluated_time_buf[])
STORAGE_BUF(CURVES_LENGTH_SLOT, write, float, curves_length_buf[])
PUSH_CONSTANT(int, curves_start)
PUSH_CONSTANT(int, curves_count)
PUSH_CONSTANT(bool, use_cyclic)
COMPUTE_FUNCTION("evaluate_length_intercept")
COMPUTE_SOURCE("draw_curves_length_intercept_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_curves_test)
STORAGE_BUF(0, write, float, result_pos_buf[])
STORAGE_BUF(1, write, int4, result_indices_buf[])
VERTEX_SOURCE("draw_curves_test.glsl")
FRAGMENT_SOURCE("draw_curves_test.glsl")
ADDITIONAL_INFO(draw_curves_infos)
ADDITIONAL_INFO(draw_curves)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
