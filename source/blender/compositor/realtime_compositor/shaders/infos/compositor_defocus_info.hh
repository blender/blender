/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_scale)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "scale")
    .push_constant(Type::FLOAT, "max_radius")
    .sampler(0, ImageType::FLOAT_2D, "radius_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "radius_img")
    .compute_source("compositor_defocus_radius_from_scale.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_defocus_radius_from_depth)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "f_stop")
    .push_constant(Type::FLOAT, "max_radius")
    .push_constant(Type::FLOAT, "focal_length")
    .push_constant(Type::FLOAT, "pixels_per_meter")
    .push_constant(Type::FLOAT, "distance_to_image_of_focus")
    .sampler(0, ImageType::FLOAT_2D, "depth_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "radius_img")
    .compute_source("compositor_defocus_radius_from_depth.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_defocus_blur)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "gamma_correct")
    .push_constant(Type::INT, "search_radius")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "weights_tx")
    .sampler(2, ImageType::FLOAT_2D, "radius_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_defocus_blur.glsl")
    .do_static_compilation(true);
