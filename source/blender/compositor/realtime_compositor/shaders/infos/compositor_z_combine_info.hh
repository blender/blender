/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_z_combine_simple)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "use_alpha")
    .sampler(0, ImageType::FLOAT_2D, "first_tx")
    .sampler(1, ImageType::FLOAT_2D, "first_z_tx")
    .sampler(2, ImageType::FLOAT_2D, "second_tx")
    .sampler(3, ImageType::FLOAT_2D, "second_z_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "combined_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "combined_z_img")
    .compute_source("compositor_z_combine_simple.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_z_combine_compute_mask)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "use_alpha")
    .sampler(0, ImageType::FLOAT_2D, "first_tx")
    .sampler(1, ImageType::FLOAT_2D, "first_z_tx")
    .sampler(2, ImageType::FLOAT_2D, "second_z_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "mask_img")
    .compute_source("compositor_z_combine_compute_mask.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_z_combine_from_mask)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "use_alpha")
    .sampler(0, ImageType::FLOAT_2D, "first_tx")
    .sampler(1, ImageType::FLOAT_2D, "first_z_tx")
    .sampler(2, ImageType::FLOAT_2D, "second_tx")
    .sampler(3, ImageType::FLOAT_2D, "second_z_tx")
    .sampler(4, ImageType::FLOAT_2D, "mask_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "combined_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "combined_z_img")
    .compute_source("compositor_z_combine_from_mask.glsl")
    .do_static_compilation(true);
