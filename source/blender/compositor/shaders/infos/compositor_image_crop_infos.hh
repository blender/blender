/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_image_crop_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int2, lower_bound)
SAMPLER(0, sampler2D, input_tx)
COMPUTE_SOURCE("compositor_image_crop.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_image_crop_float)
ADDITIONAL_INFO(compositor_image_crop_shared)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_image_crop_float4)
ADDITIONAL_INFO(compositor_image_crop_shared)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
