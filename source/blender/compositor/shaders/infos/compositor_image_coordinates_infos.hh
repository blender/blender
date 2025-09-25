/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_image_coordinates_uniform)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_image_coordinates_uniform.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_image_coordinates_normalized)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_image_coordinates_normalized.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_image_coordinates_pixel)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, SFLOAT_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_image_coordinates_pixel.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
