/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_antialiasing)
DO_STATIC_COMPILATION()
SAMPLER(0, DEPTH_2D, depthTex)
SAMPLER(1, FLOAT_2D, colorTex)
SAMPLER(2, FLOAT_2D, lineTex)
PUSH_CONSTANT(BOOL, doSmoothLines)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_SOURCE("overlay_antialiasing_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_xray_fade)
DO_STATIC_COMPILATION()
SAMPLER(0, DEPTH_2D, depthTex)
SAMPLER(1, DEPTH_2D, xrayDepthTex)
PUSH_CONSTANT(FLOAT, opacity)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_SOURCE("overlay_xray_fade_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
GPU_SHADER_CREATE_END()
