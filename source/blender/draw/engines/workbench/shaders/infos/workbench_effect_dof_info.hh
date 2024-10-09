/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_dof)
/* TODO(fclem): Split resources per stage. */
SAMPLER(0, FLOAT_2D, inputCocTex)
SAMPLER(1, FLOAT_2D, maxCocTilesTex)
SAMPLER(2, FLOAT_2D, sceneColorTex)
SAMPLER(3, FLOAT_2D, sceneDepthTex)
SAMPLER(4, FLOAT_2D, backgroundTex)
SAMPLER(5, FLOAT_2D, halfResColorTex)
SAMPLER(6, FLOAT_2D, blurTex)
SAMPLER(7, FLOAT_2D, noiseTex)
PUSH_CONSTANT(VEC2, invertedViewportSize)
PUSH_CONSTANT(VEC2, nearFar)
PUSH_CONSTANT(VEC3, dofParams)
PUSH_CONSTANT(FLOAT, noiseOffset)
FRAGMENT_SOURCE("workbench_effect_dof_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_prepare)
DEFINE("PREPARE")
FRAGMENT_OUT(0, VEC4, halfResColor)
FRAGMENT_OUT(1, VEC2, normalizedCoc)
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_downsample)
DEFINE("DOWNSAMPLE")
FRAGMENT_OUT(0, VEC4, outColor)
FRAGMENT_OUT(1, VEC2, outCocs)
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur1)
DEFINE("BLUR1")
DEFINE_VALUE("NUM_SAMPLES", "49")
UNIFORM_BUF(1, vec4, samples[49])
FRAGMENT_OUT(0, VEC4, blurColor)
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur2)
DEFINE("BLUR2")
FRAGMENT_OUT(0, VEC4, finalColor)
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_effect_dof_resolve)
DEFINE("RESOLVE")
FRAGMENT_OUT_DUAL(0, VEC4, finalColorAdd, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, finalColorMul, SRC_1)
ADDITIONAL_INFO(workbench_effect_dof)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
