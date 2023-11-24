/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_boundary)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RG16I, Qualifier::WRITE, ImageType::INT_2D, "boundary_img")
    .compute_source("compositor_inpaint_compute_boundary.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_inpaint_fill_region)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "max_distance")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::INT_2D, "flooded_boundary_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "filled_region_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "distance_to_boundary_img")
    .image(2, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "smoothing_radius_img")
    .compute_source("compositor_inpaint_fill_region.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_region)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "max_distance")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "inpainted_region_tx")
    .sampler(2, ImageType::FLOAT_2D, "distance_to_boundary_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_inpaint_compute_region.glsl")
    .do_static_compilation(true);
