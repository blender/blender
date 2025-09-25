/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_fullscreen_infos.hh"

#  include "overlay_shader_shared.hh"
#endif

#include "overlay_common_infos.hh"

GPU_SHADER_CREATE_INFO(overlay_antialiasing)
DO_STATIC_COMPILATION()
SAMPLER(0, sampler2DDepth, depth_tx)
SAMPLER(1, sampler2D, color_tx)
SAMPLER(2, sampler2D, line_tx)
PUSH_CONSTANT(bool, do_smooth_lines)
FRAGMENT_OUT(0, float4, frag_color)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
FRAGMENT_SOURCE("overlay_antialiasing_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_xray_fade)
DO_STATIC_COMPILATION()
SAMPLER(0, sampler2DDepth, depth_tx)
SAMPLER(1, sampler2DDepth, xray_depth_tx)
PUSH_CONSTANT(float, opacity)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_SOURCE("overlay_xray_fade_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
SAMPLER(2, sampler2DDepth, xray_depth_txInfront)
SAMPLER(3, sampler2DDepth, depth_txInfront)
GPU_SHADER_CREATE_END()
