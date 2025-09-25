/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_NAMED_INTERFACE_INFO(nodelink_iface, interp)
SMOOTH(float4, final_color)
SMOOTH(float2, line_uv)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_NAMED_INTERFACE_INFO(nodelink_iface_flat, interp_flat)
FLAT(float, line_length)
FLAT(float, line_thickness)
FLAT(float, dash_length)
FLAT(float, dash_factor)
FLAT(float, dash_alpha)
FLAT(float, aspect)
FLAT(int, has_back_link)
FLAT(int, is_main_line)
GPU_SHADER_NAMED_INTERFACE_END(interp_flat)

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)
VERTEX_IN(0, float2, uv)
VERTEX_IN(1, float2, pos)
VERTEX_IN(2, float2, expand)
VERTEX_OUT(nodelink_iface)
VERTEX_OUT(nodelink_iface_flat)
FRAGMENT_OUT(0, float4, out_color)
STORAGE_BUF(0, read, NodeLinkData, link_data_buf[])
UNIFORM_BUF(0, NodeLinkUniformData, link_uniforms)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_nodelink_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_nodelink_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DEFINE("USE_INSTANCE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
