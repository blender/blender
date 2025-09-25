/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "workbench_shader_shared.hh"

#  include "draw_view_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  define PREPARE
#  define DOWNSAMPLE
#  define BLUR1
#  define BLUR2
#  define RESOLVE
#  define NUM_SAMPLES 49
#endif

#include "gpu_shader_create_info.hh"

/*
 * NOTE: Keep the sampler bind points consistent between the steps.
 *
 * SAMPLER(0, sampler2D, input_coc_tx)
 * SAMPLER(1, sampler2D, scene_color_tx)
 * SAMPLER(2, sampler2D, scene_depth_tx)
 * SAMPLER(3, sampler2D, half_res_color_tx)
 * SAMPLER(4, sampler2D, blur_tx)
 * SAMPLER(5, sampler2D, noise_tx)
 */

GPU_SHADER_CREATE_INFO(workbench_effect_dof)
PUSH_CONSTANT(float2, inverted_viewport_size)
PUSH_CONSTANT(float2, near_far)
PUSH_CONSTANT(float3, dof_params)
PUSH_CONSTANT(float, noise_offset)
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_prepare)
SAMPLER(1, sampler2D, scene_color_tx)
SAMPLER(2, sampler2D, scene_depth_tx)
FRAGMENT_OUT(0, float4, halfResColor)
FRAGMENT_OUT(1, float2, normalizedCoc)
FRAGMENT_SOURCE("workbench_effect_dof_prepare_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_downsample)
SAMPLER(0, sampler2D, input_coc_tx)
SAMPLER(1, sampler2D, scene_color_tx)
FRAGMENT_OUT(0, float4, outColor)
FRAGMENT_OUT(1, float2, outCocs)
FRAGMENT_SOURCE("workbench_effect_dof_downsample_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur1)
DEFINE_VALUE("NUM_SAMPLES", "49")
SAMPLER(0, sampler2D, input_coc_tx)
SAMPLER(3, sampler2D, half_res_color_tx)
SAMPLER(5, sampler2D, noise_tx)
UNIFORM_BUF(1, float4, samples[49])
FRAGMENT_OUT(0, float4, blurColor)
FRAGMENT_SOURCE("workbench_effect_dof_blur1_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur2)
SAMPLER(0, sampler2D, input_coc_tx)
SAMPLER(4, sampler2D, blur_tx)
FRAGMENT_OUT(0, float4, final_color)
FRAGMENT_SOURCE("workbench_effect_dof_blur2_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_resolve)
SAMPLER(2, sampler2D, scene_depth_tx)
SAMPLER(3, sampler2D, half_res_color_tx)
FRAGMENT_OUT_DUAL(0, float4, final_colorAdd, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, final_colorMul, SRC_1)
FRAGMENT_SOURCE("workbench_effect_dof_resolve_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
