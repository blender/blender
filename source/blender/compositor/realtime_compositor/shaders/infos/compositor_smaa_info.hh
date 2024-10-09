/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_smaa_edge_detection)
LOCAL_GROUP_SIZE(16, 16)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS",
             "vec4(1.0 / vec2(textureSize(input_tx, 0)), vec2(textureSize(input_tx, 0)))")
DEFINE_VALUE("SMAA_LUMA_WEIGHT", "vec4(luminance_coefficients, 0.0)")
DEFINE_VALUE("SMAA_THRESHOLD", "smaa_threshold")
DEFINE_VALUE("SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR", "smaa_local_contrast_adaptation_factor")
PUSH_CONSTANT(VEC3, luminance_coefficients)
PUSH_CONSTANT(FLOAT, smaa_threshold)
PUSH_CONSTANT(FLOAT, smaa_local_contrast_adaptation_factor)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, edges_img)
COMPUTE_SOURCE("compositor_smaa_edge_detection.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_smaa_blending_weight_calculation)
LOCAL_GROUP_SIZE(16, 16)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS",
             "vec4(1.0 / vec2(textureSize(edges_tx, 0)), vec2(textureSize(edges_tx, 0)))")
DEFINE_VALUE("SMAA_CORNER_ROUNDING", "smaa_corner_rounding")
PUSH_CONSTANT(INT, smaa_corner_rounding)
SAMPLER(0, FLOAT_2D, edges_tx)
SAMPLER(1, FLOAT_2D, area_tx)
SAMPLER(2, FLOAT_2D, search_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, weights_img)
COMPUTE_SOURCE("compositor_smaa_blending_weight_calculation.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_shared)
LOCAL_GROUP_SIZE(16, 16)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS",
             "vec4(1.0 / vec2(textureSize(input_tx, 0)), vec2(textureSize(input_tx, 0)))")
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, weights_tx)
COMPUTE_SOURCE("compositor_smaa_neighborhood_blending.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_float4)
ADDITIONAL_INFO(compositor_smaa_neighborhood_blending_shared)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_float2)
ADDITIONAL_INFO(compositor_smaa_neighborhood_blending_shared)
IMAGE(0, GPU_RG16F, WRITE, FLOAT_2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_float)
ADDITIONAL_INFO(compositor_smaa_neighborhood_blending_shared)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
