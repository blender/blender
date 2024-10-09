/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_scale)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, scale)
PUSH_CONSTANT(FLOAT, max_radius)
SAMPLER(0, FLOAT_2D, radius_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, radius_img)
COMPUTE_SOURCE("compositor_defocus_radius_from_scale.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_depth)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, f_stop)
PUSH_CONSTANT(FLOAT, max_radius)
PUSH_CONSTANT(FLOAT, focal_length)
PUSH_CONSTANT(FLOAT, pixels_per_meter)
PUSH_CONSTANT(FLOAT, distance_to_image_of_focus)
SAMPLER(0, FLOAT_2D, depth_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, radius_img)
COMPUTE_SOURCE("compositor_defocus_radius_from_depth.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_defocus_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(BOOL, gamma_correct)
PUSH_CONSTANT(INT, search_radius)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, weights_tx)
SAMPLER(2, FLOAT_2D, radius_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_defocus_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
