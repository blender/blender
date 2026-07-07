/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_relative_to_pixel_float)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, reference_size)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_relative_to_pixel_float.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_relative_to_pixel_float_per_dimension)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, reference_size)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_relative_to_pixel_float_per_dimension.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_relative_to_pixel_vector)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, reference_size)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_relative_to_pixel_vector.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
