/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#  include "draw_view_info.hh"
#  include "gpu_shader_fullscreen_info.hh"

#  include "overlay_common_info.hh"
#  include "overlay_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_background)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
SAMPLER(0, FLOAT_2D, colorBuffer)
SAMPLER(1, DEPTH_2D, depthBuffer)
PUSH_CONSTANT(int, bgType)
PUSH_CONSTANT(float4, colorOverride)
FRAGMENT_SOURCE("overlay_background_frag.glsl")
FRAGMENT_OUT(0, float4, fragColor)
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_clipbound)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float4, ucolor)
PUSH_CONSTANT_ARRAY(float3, boundbox, 8)
VERTEX_SOURCE("overlay_clipbound_vert.glsl")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()
