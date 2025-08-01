/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, weights_tx)
COMPUTE_SOURCE("compositor_symmetric_separable_blur.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_float)
ADDITIONAL_INFO(compositor_symmetric_separable_blur_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_float4)
ADDITIONAL_INFO(compositor_symmetric_separable_blur_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
