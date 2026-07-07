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
#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_3D_smooth_color)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float4, color)
VERTEX_OUT(smooth_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_3D_smooth_color_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_3D_smooth_color_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_smooth_color_clipped)
ADDITIONAL_INFO(gpu_shader_3D_smooth_color)
ADDITIONAL_INFO(gpu_clip_planes)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
