/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_van_vliet_gaussian_blur)
LOCAL_GROUP_SIZE(64, 4)
PUSH_CONSTANT(VEC2, first_feedback_coefficients)
PUSH_CONSTANT(VEC2, first_causal_feedforward_coefficients)
PUSH_CONSTANT(VEC2, first_non_causal_feedforward_coefficients)
PUSH_CONSTANT(VEC2, second_feedback_coefficients)
PUSH_CONSTANT(VEC2, second_causal_feedforward_coefficients)
PUSH_CONSTANT(VEC2, second_non_causal_feedforward_coefficients)
PUSH_CONSTANT(FLOAT, first_causal_boundary_coefficient)
PUSH_CONSTANT(FLOAT, first_non_causal_boundary_coefficient)
PUSH_CONSTANT(FLOAT, second_causal_boundary_coefficient)
PUSH_CONSTANT(FLOAT, second_non_causal_boundary_coefficient)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, first_causal_output_img)
IMAGE(1, GPU_RGBA16F, WRITE, FLOAT_2D, first_non_causal_output_img)
IMAGE(2, GPU_RGBA16F, WRITE, FLOAT_2D, second_causal_output_img)
IMAGE(3, GPU_RGBA16F, WRITE, FLOAT_2D, second_non_causal_output_img)
COMPUTE_SOURCE("compositor_van_vliet_gaussian_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_van_vliet_gaussian_blur_sum)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, first_causal_input_tx)
SAMPLER(1, FLOAT_2D, first_non_causal_input_tx)
SAMPLER(2, FLOAT_2D, second_causal_input_tx)
SAMPLER(3, FLOAT_2D, second_non_causal_input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_van_vliet_gaussian_blur_sum.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
