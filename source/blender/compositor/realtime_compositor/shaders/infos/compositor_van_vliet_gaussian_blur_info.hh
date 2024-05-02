/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_van_vliet_gaussian_blur)
    .local_group_size(64, 4)
    .push_constant(Type::VEC2, "first_feedback_coefficients")
    .push_constant(Type::VEC2, "first_causal_feedforward_coefficients")
    .push_constant(Type::VEC2, "first_non_causal_feedforward_coefficients")
    .push_constant(Type::VEC2, "second_feedback_coefficients")
    .push_constant(Type::VEC2, "second_causal_feedforward_coefficients")
    .push_constant(Type::VEC2, "second_non_causal_feedforward_coefficients")
    .push_constant(Type::FLOAT, "first_causal_boundary_coefficient")
    .push_constant(Type::FLOAT, "first_non_causal_boundary_coefficient")
    .push_constant(Type::FLOAT, "second_causal_boundary_coefficient")
    .push_constant(Type::FLOAT, "second_non_causal_boundary_coefficient")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "first_causal_output_img")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "first_non_causal_output_img")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "second_causal_output_img")
    .image(3, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "second_non_causal_output_img")
    .compute_source("compositor_van_vliet_gaussian_blur.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_van_vliet_gaussian_blur_sum)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "first_causal_input_tx")
    .sampler(1, ImageType::FLOAT_2D, "first_non_causal_input_tx")
    .sampler(2, ImageType::FLOAT_2D, "second_causal_input_tx")
    .sampler(3, ImageType::FLOAT_2D, "second_non_causal_input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_van_vliet_gaussian_blur_sum.glsl")
    .do_static_compilation(true);
