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

#  include "gpu_srgb_to_framebuffer_space_info.hh"
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image_common)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
SAMPLER(0, sampler2D, image)
VERTEX_SOURCE("gpu_shader_3D_image_vert.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image)
ADDITIONAL_INFO(gpu_shader_3D_image_common)
FRAGMENT_SOURCE("gpu_shader_image_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image_color)
ADDITIONAL_INFO(gpu_shader_3D_image_common)
PUSH_CONSTANT(float4, color)
FRAGMENT_SOURCE("gpu_shader_image_color_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
