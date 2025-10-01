/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  include "overlay_common_infos.hh"
#  include "overlay_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_background)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
SAMPLER(0, sampler2D, color_buffer)
SAMPLER(1, sampler2DDepth, depth_buffer)
PUSH_CONSTANT(int, bg_type)
PUSH_CONSTANT(float4, color_override)
PUSH_CONSTANT(float, vignette_aperture)
PUSH_CONSTANT(float, vignette_falloff)
PUSH_CONSTANT(bool, vignette_enabled)
FRAGMENT_SOURCE("overlay_background_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_clipbound)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float4, ucolor)
PUSH_CONSTANT_ARRAY(float3, boundbox, 8)
VERTEX_SOURCE("overlay_clipbound_vert.glsl")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()
