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

GPU_SHADER_INTERFACE_INFO(smooth_normal_iface)
SMOOTH(float3, normal)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_simple_lighting)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_OUT(smooth_normal_iface)
FRAGMENT_OUT(0, float4, fragColor)
UNIFORM_BUF_FREQ(0, SimpleLightingData, simple_lighting_data, PASS)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float3x3, NormalMatrix)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_3D_normal_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_simple_lighting_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
