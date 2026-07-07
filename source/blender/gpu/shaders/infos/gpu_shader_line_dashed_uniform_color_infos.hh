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
#  include "gpu_clip_planes_infos.hh"
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

/* We leverage hardware interpolation to compute distance along the line. */
GPU_SHADER_INTERFACE_INFO(gpu_shader_line_dashed_interface)
NO_PERSPECTIVE(float2, stipple_start) /* In screen space */
FLAT(float2, stipple_pos)
GPU_SHADER_INTERFACE_END() /* In screen space */

GPU_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(flat_color_iface)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float2, viewport_size)
PUSH_CONSTANT(float, dash_width)
PUSH_CONSTANT(float, udash_factor) /* if > 1.0f, solid line. */
/* TODO(fclem): Remove this. And decide to discard if color2 alpha is 0. */
PUSH_CONSTANT(int, colors_len) /* Enabled if > 0, 1 for solid line. */
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float4, color2)
VERTEX_OUT(gpu_shader_line_dashed_interface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("gpu_shader_3D_line_dashed_uniform_color_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_line_dashed_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color_clipped)
PUSH_CONSTANT(float4x4, ModelMatrix)
ADDITIONAL_INFO(gpu_shader_3D_line_dashed_uniform_color)
ADDITIONAL_INFO(gpu_clip_planes)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
