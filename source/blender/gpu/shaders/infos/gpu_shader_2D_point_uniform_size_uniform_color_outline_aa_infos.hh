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

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_outline_aa)
VERTEX_IN(0, float2, pos)
VERTEX_OUT(smooth_radii_outline_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float4, outlineColor)
PUSH_CONSTANT(float, size)
PUSH_CONSTANT(float, outlineWidth)
VERTEX_SOURCE("gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_point_uniform_color_outline_aa_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
