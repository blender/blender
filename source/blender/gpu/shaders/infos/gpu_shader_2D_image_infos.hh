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

#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_common)
VERTEX_IN(0, float2, pos)
VERTEX_IN(1, float2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
SAMPLER(0, sampler2D, image)
VERTEX_SOURCE("gpu_shader_2D_image_vert.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
GPU_SHADER_CREATE_END()
