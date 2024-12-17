/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_z_combine_simple)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(BOOL, use_alpha)
SAMPLER(0, FLOAT_2D, first_tx)
SAMPLER(1, FLOAT_2D, first_z_tx)
SAMPLER(2, FLOAT_2D, second_tx)
SAMPLER(3, FLOAT_2D, second_z_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, combined_img)
IMAGE(1, GPU_R16F, WRITE, FLOAT_2D, combined_z_img)
COMPUTE_SOURCE("compositor_z_combine_simple.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_z_combine_compute_mask)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, first_z_tx)
SAMPLER(1, FLOAT_2D, second_z_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, mask_img)
COMPUTE_SOURCE("compositor_z_combine_compute_mask.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_z_combine_from_mask)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(BOOL, use_alpha)
SAMPLER(0, FLOAT_2D, first_tx)
SAMPLER(1, FLOAT_2D, first_z_tx)
SAMPLER(2, FLOAT_2D, second_tx)
SAMPLER(3, FLOAT_2D, second_z_tx)
SAMPLER(4, FLOAT_2D, mask_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, combined_img)
IMAGE(1, GPU_R16F, WRITE, FLOAT_2D, combined_z_img)
COMPUTE_SOURCE("compositor_z_combine_from_mask.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
