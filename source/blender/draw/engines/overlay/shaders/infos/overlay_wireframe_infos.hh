/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "gpu_index_load_infos.hh"
#  include "overlay_common_infos.hh"

#  define CUSTOM_DEPTH_BIAS_CONST
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_wireframe_iface)
SMOOTH(float4, final_color)
FLAT(float2, edge_start)
NO_PERSPECTIVE(float2, edge_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_base)
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(float, wire_step_param)
PUSH_CONSTANT(float, wire_opacity)
PUSH_CONSTANT(bool, use_coloring)
PUSH_CONSTANT(bool, is_transform)
PUSH_CONSTANT(int, color_type)
PUSH_CONSTANT(bool, is_hair)
PUSH_CONSTANT(float4x4, hair_dupli_matrix)
/* Scene Depth texture copy for manual depth test. */
SAMPLER(0, sampler2DDepth, depth_tx)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_IN(2, float, wd) /* wire-data. */
VERTEX_OUT(overlay_wireframe_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
DEPTH_WRITE(DepthWrite::ANY)
SPECIALIZATION_CONSTANT(bool, use_custom_depth_bias, true)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_wireframe, overlay_wireframe_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_wireframe_selectable, overlay_wireframe_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_wireframe_clipped, overlay_wireframe, drw_clipped)
CREATE_INFO_VARIANT(overlay_wireframe_selectable_clipped, overlay_wireframe_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_wireframe_curve_base)
DEFINE("CURVES")
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(float, wire_opacity)
PUSH_CONSTANT(bool, use_coloring)
PUSH_CONSTANT(bool, is_transform)
PUSH_CONSTANT(int, color_type)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_wireframe_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_wireframe_curve, overlay_wireframe_curve_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_wireframe_curve_selectable, overlay_wireframe_curve_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_wireframe_curve_clipped, overlay_wireframe_curve, drw_clipped)
CREATE_INFO_VARIANT(overlay_wireframe_curve_selectable_clipped, overlay_wireframe_curve_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_INTERFACE_INFO(overlay_wireframe_points_iface)
FLAT(float4, final_color)
FLAT(float4, final_color_inner)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_points_base)
DEFINE("POINTS")
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(bool, use_coloring)
PUSH_CONSTANT(bool, is_transform)
PUSH_CONSTANT(int, color_type)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_wireframe_points_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_wireframe_points, overlay_wireframe_points_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_wireframe_points_selectable, overlay_wireframe_points_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_wireframe_points_clipped, overlay_wireframe_points, drw_clipped)
CREATE_INFO_VARIANT(overlay_wireframe_points_selectable_clipped, overlay_wireframe_points_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_iface_wireframe)
SMOOTH(float, selection_fac)
FLAT(float2, stipple_start)
NO_PERSPECTIVE(float, edge_coord)
NO_PERSPECTIVE(float2, stipple_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_uv)
DO_STATIC_COMPILATION()
DEFINE("WIREFRAME")
STORAGE_BUF_FREQ(0, read, float, au[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
DEFINE_VALUE("line_style", "4u" /* OVERLAY_UV_LINE_STYLE_SHADOW */)
DEFINE_VALUE("dash_length", "1" /* Not used by this line style */)
DEFINE_VALUE("use_edge_select", "false")
PUSH_CONSTANT(bool, do_smooth_wire)
PUSH_CONSTANT(float, alpha)
VERTEX_OUT(overlay_edit_uv_iface_wireframe)
FRAGMENT_OUT(0, float4, frag_color)
/* Note: Reuse edit mode shader as it is mostly the same. */
VERTEX_SOURCE("overlay_edit_uv_edges_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_uv_edges_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()
