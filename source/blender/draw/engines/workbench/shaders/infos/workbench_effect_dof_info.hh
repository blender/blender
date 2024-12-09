/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "workbench_shader_shared.h"

#  include "draw_fullscreen_info.hh"
#  include "draw_view_info.hh"

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
 * SAMPLER(0, FLOAT_2D, inputCocTex)
 * SAMPLER(1, FLOAT_2D, sceneColorTex)
 * SAMPLER(2, FLOAT_2D, sceneDepthTex)
 * SAMPLER(3, FLOAT_2D, halfResColorTex)
 * SAMPLER(4, FLOAT_2D, blurTex)
 * SAMPLER(5, FLOAT_2D, noiseTex)
 */

GPU_SHADER_CREATE_INFO(workbench_effect_dof)
PUSH_CONSTANT(VEC2, invertedViewportSize)
PUSH_CONSTANT(VEC2, nearFar)
PUSH_CONSTANT(VEC3, dofParams)
PUSH_CONSTANT(FLOAT, noiseOffset)
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_prepare)
SAMPLER(1, FLOAT_2D, sceneColorTex)
SAMPLER(2, FLOAT_2D, sceneDepthTex)
FRAGMENT_OUT(0, VEC4, halfResColor)
FRAGMENT_OUT(1, VEC2, normalizedCoc)
FRAGMENT_SOURCE("workbench_effect_dof_prepare_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_downsample)
SAMPLER(0, FLOAT_2D, inputCocTex)
SAMPLER(1, FLOAT_2D, sceneColorTex)
FRAGMENT_OUT(0, VEC4, outColor)
FRAGMENT_OUT(1, VEC2, outCocs)
FRAGMENT_SOURCE("workbench_effect_dof_downsample_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur1)
DEFINE_VALUE("NUM_SAMPLES", "49")
SAMPLER(0, FLOAT_2D, inputCocTex)
SAMPLER(3, FLOAT_2D, halfResColorTex)
SAMPLER(5, FLOAT_2D, noiseTex)
UNIFORM_BUF(1, vec4, samples[49])
FRAGMENT_OUT(0, VEC4, blurColor)
FRAGMENT_SOURCE("workbench_effect_dof_blur1_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur2)
SAMPLER(0, FLOAT_2D, inputCocTex)
SAMPLER(4, FLOAT_2D, blurTex)
FRAGMENT_OUT(0, VEC4, finalColor)
FRAGMENT_SOURCE("workbench_effect_dof_blur2_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_resolve)
SAMPLER(1, FLOAT_2D, sceneColorTex)
SAMPLER(2, FLOAT_2D, sceneDepthTex)
SAMPLER(3, FLOAT_2D, halfResColorTex)
FRAGMENT_OUT_DUAL(0, VEC4, finalColorAdd, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, finalColorMul, SRC_1)
FRAGMENT_SOURCE("workbench_effect_dof_resolve_frag.glsl")
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
