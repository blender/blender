/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_scale)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, scale)
PUSH_CONSTANT(float, max_radius)
SAMPLER(0, sampler2D, radius_tx)
IMAGE(0, SFLOAT_16, write, image2D, radius_img)
COMPUTE_SOURCE("compositor_defocus_radius_from_scale.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_depth)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, f_stop)
PUSH_CONSTANT(float, max_radius)
PUSH_CONSTANT(float, focal_length)
PUSH_CONSTANT(float, pixels_per_meter)
PUSH_CONSTANT(float, distance_to_image_of_focus)
SAMPLER(0, sampler2D, depth_tx)
IMAGE(0, SFLOAT_16, write, image2D, radius_img)
COMPUTE_SOURCE("compositor_defocus_radius_from_depth.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_defocus_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, search_radius)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, weights_tx)
SAMPLER(2, sampler2D, radius_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_defocus_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
