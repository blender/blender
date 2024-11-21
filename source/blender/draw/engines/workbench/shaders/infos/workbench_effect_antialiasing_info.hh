/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_fullscreen_info.hh"

#  define SMAA_GLSL_3
#  define SMAA_STAGE 1
#  define SMAA_PRESET_HIGH
#  define SMAA_NO_DISCARD
#  define SMAA_RT_METRICS viewportMetrics
#  define SMAA_LUMA_WEIGHT float4(1.0, 1.0, 1.0, 1.0)
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name TAA
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_taa)
SAMPLER(0, FLOAT_2D, colorBuffer)
PUSH_CONSTANT_ARRAY(FLOAT, samplesWeights, 9)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_SOURCE("workbench_effect_taa_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name SMAA
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_smaa_iface)
SMOOTH(VEC2, uvs)
SMOOTH(VEC2, pixcoord)
SMOOTH(VEC4, offset[3])
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS", "viewportMetrics")
DEFINE("SMAA_PRESET_HIGH")
DEFINE_VALUE("SMAA_LUMA_WEIGHT", "float4(1.0, 1.0, 1.0, 1.0)")
DEFINE("SMAA_NO_DISCARD")
VERTEX_OUT(workbench_smaa_iface)
PUSH_CONSTANT(VEC4, viewportMetrics)
VERTEX_SOURCE("workbench_effect_smaa_vert.glsl")
FRAGMENT_SOURCE("workbench_effect_smaa_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_0)
DEFINE_VALUE("SMAA_STAGE", "0")
SAMPLER(0, FLOAT_2D, colorTex)
FRAGMENT_OUT(0, VEC2, out_edges)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_1)
DEFINE_VALUE("SMAA_STAGE", "1")
SAMPLER(0, FLOAT_2D, edgesTex)
SAMPLER(1, FLOAT_2D, areaTex)
SAMPLER(2, FLOAT_2D, searchTex)
FRAGMENT_OUT(0, VEC4, out_weights)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_2)
DEFINE_VALUE("SMAA_STAGE", "2")
SAMPLER(0, FLOAT_2D, colorTex)
SAMPLER(1, FLOAT_2D, blendTex)
PUSH_CONSTANT(FLOAT, mixFactor)
PUSH_CONSTANT(FLOAT, taaAccumulatedWeight)
FRAGMENT_OUT(0, VEC4, out_color)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
