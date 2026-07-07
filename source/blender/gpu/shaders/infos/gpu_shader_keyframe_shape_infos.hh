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

GPU_SHADER_INTERFACE_INFO(keyframe_shape_iface)
FLAT(float4, finalColor)
FLAT(float4, finalOutlineColor)
FLAT(float4, radii)
FLAT(float4, thresholds)
FLAT(uint, finalFlags)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_keyframe_shape)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_IN(0, float4, color)
VERTEX_IN(1, float4, outlineColor)
VERTEX_IN(2, float2, pos)
VERTEX_IN(3, float, size)
VERTEX_IN(4, uint, flags)
VERTEX_OUT(keyframe_shape_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float2, ViewportSize)
PUSH_CONSTANT(float, outline_scale)
VERTEX_SOURCE("gpu_shader_keyframe_shape_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_keyframe_shape_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
