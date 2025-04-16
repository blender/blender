/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"
#  include "gpu_index_load_info.hh"
#  include "overlay_common_info.hh"

#  define CUSTOM_DEPTH_BIAS_CONST
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_wireframe_iface)
SMOOTH(float4, finalColor)
FLAT(float2, edgeStart)
NO_PERSPECTIVE(float2, edgePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_base)
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(float, wireStepParam)
PUSH_CONSTANT(float, wireOpacity)
PUSH_CONSTANT(bool, useColoring)
PUSH_CONSTANT(bool, isTransform)
PUSH_CONSTANT(int, colorType)
PUSH_CONSTANT(bool, isHair)
PUSH_CONSTANT(float4x4, hairDupliMatrix)
/* Scene Depth texture copy for manual depth test. */
SAMPLER(0, DEPTH_2D, depthTex)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_IN(2, float, wd) /* wire-data. */
VERTEX_OUT(overlay_wireframe_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
DEPTH_WRITE(DepthWrite::ANY)
SPECIALIZATION_CONSTANT(bool, use_custom_depth_bias, true)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_wireframe, overlay_wireframe_base)

GPU_SHADER_CREATE_INFO(overlay_wireframe_curve_base)
DEFINE("CURVES")
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(float, wireOpacity)
PUSH_CONSTANT(bool, useColoring)
PUSH_CONSTANT(bool, isTransform)
PUSH_CONSTANT(int, colorType)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_wireframe_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_wireframe_curve, overlay_wireframe_curve_base)

GPU_SHADER_INTERFACE_INFO(overlay_wireframe_points_iface)
FLAT(float4, finalColor)
FLAT(float4, finalColorInner)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_points_base)
DEFINE("POINTS")
PUSH_CONSTANT(float, ndc_offset_factor)
PUSH_CONSTANT(bool, useColoring)
PUSH_CONSTANT(bool, isTransform)
PUSH_CONSTANT(int, colorType)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_wireframe_points_iface)
VERTEX_SOURCE("overlay_wireframe_vert.glsl")
FRAGMENT_SOURCE("overlay_wireframe_frag.glsl")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_wireframe_points, overlay_wireframe_points_base)

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_iface_wireframe)
SMOOTH(float, selectionFac)
FLAT(float2, stippleStart)
NO_PERSPECTIVE(float, edgeCoord)
NO_PERSPECTIVE(float2, stipplePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_uv)
DO_STATIC_COMPILATION()
DEFINE("WIREFRAME")
STORAGE_BUF_FREQ(0, READ, float, au[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
DEFINE_VALUE("lineStyle", "4u" /* OVERLAY_UV_LINE_STYLE_SHADOW */)
DEFINE_VALUE("dashLength", "1" /* Not used by this line style */)
DEFINE_VALUE("use_edge_select", "false")
PUSH_CONSTANT(bool, doSmoothWire)
PUSH_CONSTANT(float, alpha)
VERTEX_OUT(overlay_edit_uv_iface_wireframe)
FRAGMENT_OUT(0, float4, fragColor)
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
