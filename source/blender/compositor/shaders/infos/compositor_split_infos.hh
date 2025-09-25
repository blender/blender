/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_split)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, position)
PUSH_CONSTANT(float2, normal)
SAMPLER(0, sampler2D, first_image_tx)
SAMPLER(1, sampler2D, second_image_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_split.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
