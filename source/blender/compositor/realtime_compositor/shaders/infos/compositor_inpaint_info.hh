/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_boundary)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "boundary_img")
    .compute_source("compositor_inpaint_compute_boundary.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_region)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "distance")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "flooded_boundary_tx")
    .sampler(2, ImageType::FLOAT_1D, "gaussian_weights_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_inpaint_compute_region.glsl")
    .do_static_compilation(true);
