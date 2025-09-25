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

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_icon_shared)
VERTEX_OUT(icon_interp_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float4, finalColor)
PUSH_CONSTANT(float4, rect_icon)
PUSH_CONSTANT(float4, rect_geom)
PUSH_CONSTANT(float, text_width)
SAMPLER(0, sampler2D, image)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_icon)
COMPILATION_CONSTANT(bool, do_corner_masking, true)
VERTEX_SOURCE("gpu_shader_icon_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_icon_frag.glsl")
ADDITIONAL_INFO(gpu_shader_icon_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_icon_multi)
COMPILATION_CONSTANT(bool, do_corner_masking, false)
VERTEX_IN(0, float2, pos)
UNIFORM_BUF(0, MultiIconCallData, multi_icon_data)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_icon_multi_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_icon_frag.glsl")
ADDITIONAL_INFO(gpu_shader_icon_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
