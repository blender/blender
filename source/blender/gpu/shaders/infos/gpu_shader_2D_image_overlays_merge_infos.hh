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

/* Cycles display driver fallback shader. */
GPU_SHADER_CREATE_INFO(gpu_shader_cycles_display_fallback)
VERTEX_IN(0, float2, pos)
VERTEX_IN(1, float2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float2, fullscreen)
SAMPLER(0, sampler2D, image_texture)
VERTEX_SOURCE("gpu_shader_display_fallback_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_display_fallback_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
