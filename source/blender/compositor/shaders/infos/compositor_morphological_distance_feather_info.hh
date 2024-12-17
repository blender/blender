/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, weights_tx)
SAMPLER(2, FLOAT_2D, falloffs_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_morphological_distance_feather.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_dilate)
ADDITIONAL_INFO(compositor_morphological_distance_feather_shared)
DEFINE_VALUE("FUNCTION(x)", "x")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_erode)
ADDITIONAL_INFO(compositor_morphological_distance_feather_shared)
DEFINE_VALUE("FUNCTION(x)", "1.0 - x")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
