/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_fullscreen_infos.hh"

#  define SMAA_GLSL_3
#  define SMAA_STAGE 1
#  define SMAA_PRESET_HIGH
#  define SMAA_NO_DISCARD
#  define SMAA_RT_METRICS viewport_metrics
#  define SMAA_LUMA_WEIGHT float4(1.0f, 1.0f, 1.0f, 1.0f)
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name TAA
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_taa)
SAMPLER(0, sampler2D, color_buffer)
PUSH_CONSTANT_ARRAY(float, samplesWeights, 9)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_SOURCE("workbench_effect_taa_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name SMAA
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_smaa_iface)
SMOOTH(float2, uvs)
SMOOTH(float2, pixcoord)
SMOOTH(float4, offset0)
SMOOTH(float4, offset1)
SMOOTH(float4, offset2)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS", "viewport_metrics")
DEFINE("SMAA_PRESET_HIGH")
DEFINE_VALUE("SMAA_LUMA_WEIGHT", "float4(1.0f, 1.0f, 1.0f, 1.0f)")
DEFINE("SMAA_NO_DISCARD")
VERTEX_OUT(workbench_smaa_iface)
PUSH_CONSTANT(float4, viewport_metrics)
VERTEX_SOURCE("workbench_effect_smaa_vert.glsl")
FRAGMENT_SOURCE("workbench_effect_smaa_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_0)
DEFINE_VALUE("SMAA_STAGE", "0")
SAMPLER(0, sampler2D, color_tx)
FRAGMENT_OUT(0, float2, out_edges)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_1)
DEFINE_VALUE("SMAA_STAGE", "1")
SAMPLER(0, sampler2D, edges_tx)
SAMPLER(1, sampler2D, area_tx)
SAMPLER(2, sampler2D, search_tx)
FRAGMENT_OUT(0, float4, out_weights)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_2)
DEFINE_VALUE("SMAA_STAGE", "2")
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, blend_tx)
PUSH_CONSTANT(float, mix_factor)
PUSH_CONSTANT(float, taa_accumulated_weight)
FRAGMENT_OUT(0, float4, out_color)
ADDITIONAL_INFO(workbench_smaa)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
