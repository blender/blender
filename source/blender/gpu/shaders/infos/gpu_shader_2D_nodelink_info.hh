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
SMOOTH(VEC4, finalColor)
SMOOTH(VEC2, lineUV)
FLAT(FLOAT, lineLength)
FLAT(FLOAT, lineThickness)
FLAT(FLOAT, dashLength)
FLAT(FLOAT, dashFactor)
FLAT(INT, hasBackLink)
FLAT(FLOAT, dashAlpha)
FLAT(INT, isMainLine)
FLAT(FLOAT, aspect)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)
VERTEX_IN(0, VEC2, uv)
VERTEX_IN(1, VEC2, pos)
VERTEX_IN(2, VEC2, expand)
VERTEX_OUT(nodelink_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
UNIFORM_BUF_FREQ(0, NodeLinkData, node_link_data, PASS)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_nodelink_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_nodelink_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink_inst)
VERTEX_IN(0, VEC2, uv)
VERTEX_IN(1, VEC2, pos)
VERTEX_IN(2, VEC2, expand)
VERTEX_IN(3, VEC2, P0)
VERTEX_IN(4, VEC2, P1)
VERTEX_IN(5, VEC2, P2)
VERTEX_IN(6, VEC2, P3)
VERTEX_IN(7, UVEC4, colid_doarrow)
VERTEX_IN(8, VEC4, start_color)
VERTEX_IN(9, VEC4, end_color)
VERTEX_IN(10, UVEC2, domuted)
VERTEX_IN(11, FLOAT, dim_factor)
VERTEX_IN(12, FLOAT, thickness)
VERTEX_IN(13, VEC3, dash_params)
VERTEX_IN(14, INT, has_back_link)
VERTEX_OUT(nodelink_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
UNIFORM_BUF_FREQ(0, NodeLinkInstanceData, node_link_data, PASS)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_nodelink_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_nodelink_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DEFINE("USE_INSTANCE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
