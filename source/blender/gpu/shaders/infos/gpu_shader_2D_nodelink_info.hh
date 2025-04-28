/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(nodelink_iface)
SMOOTH(float4, finalColor)
SMOOTH(float2, lineUV)
FLAT(float, lineLength)
FLAT(float, lineThickness)
FLAT(float, dashLength)
FLAT(float, dashFactor)
FLAT(int, hasBackLink)
FLAT(float, dashAlpha)
FLAT(int, isMainLine)
FLAT(float, aspect)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)
VERTEX_IN(0, float2, uv)
VERTEX_IN(1, float2, pos)
VERTEX_IN(2, float2, expand)
VERTEX_OUT(nodelink_iface)
FRAGMENT_OUT(0, float4, fragColor)
UNIFORM_BUF_FREQ(0, NodeLinkData, node_link_data, PASS)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_nodelink_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_nodelink_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink_inst)
VERTEX_IN(0, float2, uv)
VERTEX_IN(1, float2, pos)
VERTEX_IN(2, float2, expand)
VERTEX_IN(3, float2, P0)
VERTEX_IN(4, float2, P1)
VERTEX_IN(5, float2, P2)
VERTEX_IN(6, float2, P3)
VERTEX_IN(7, uint4, colid_doarrow)
VERTEX_IN(8, float4, start_color)
VERTEX_IN(9, float4, end_color)
VERTEX_IN(10, uint2, domuted)
VERTEX_IN(11, float, dim_factor)
VERTEX_IN(12, float, thickness)
VERTEX_IN(13, float3, dash_params)
VERTEX_IN(14, int, has_back_link)
VERTEX_OUT(nodelink_iface)
FRAGMENT_OUT(0, float4, fragColor)
UNIFORM_BUF_FREQ(0, NodeLinkInstanceData, node_link_data, PASS)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_nodelink_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_nodelink_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DEFINE("USE_INSTANCE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
