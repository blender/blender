/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_sample_pixel_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float2, coordinates_u)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sample_pixel)
ADDITIONAL_INFO(compositor_sample_pixel_shared)
COMPUTE_SOURCE("compositor_sample_pixel.glsl")
DEFINE_VALUE("SAMPLER_FUNCTION", "texture")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_sample_pixel_bicubic)
ADDITIONAL_INFO(compositor_sample_pixel_shared)
COMPUTE_SOURCE("compositor_sample_pixel.glsl")
DEFINE_VALUE("SAMPLER_FUNCTION", "texture_bicubic")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
