/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_deriche_gaussian_blur)
    .local_group_size(128, 2)
    .push_constant(Type::VEC4, "causal_feedforward_coefficients")
    .push_constant(Type::VEC4, "non_causal_feedforward_coefficients")
    .push_constant(Type::VEC4, "feedback_coefficients")
    .push_constant(Type::FLOAT, "causal_boundary_coefficient")
    .push_constant(Type::FLOAT, "non_causal_boundary_coefficient")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "causal_output_img")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "non_causal_output_img")
    .compute_source("compositor_deriche_gaussian_blur.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_deriche_gaussian_blur_sum)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "causal_input_tx")
    .sampler(1, ImageType::FLOAT_2D, "non_causal_input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_deriche_gaussian_blur_sum.glsl")
    .do_static_compilation(true);
