/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_directional_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, iterations)
PUSH_CONSTANT(float2, origin)
PUSH_CONSTANT(float2, delta_translation)
PUSH_CONSTANT(float, delta_rotation_sin)
PUSH_CONSTANT(float, delta_rotation_cos)
PUSH_CONSTANT(float, delta_scale)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_directional_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
