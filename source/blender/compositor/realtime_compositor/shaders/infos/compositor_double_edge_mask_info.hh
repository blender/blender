/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_double_edge_mask_compute_boundary)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "include_all_inner_edges")
    .push_constant(Type::BOOL, "include_edges_of_image")
    .sampler(0, ImageType::FLOAT_2D, "inner_mask_tx")
    .sampler(1, ImageType::FLOAT_2D, "outer_mask_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "inner_boundary_img")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "outer_boundary_img")
    .compute_source("compositor_double_edge_mask_compute_boundary.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_double_edge_mask_compute_gradient)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "inner_mask_tx")
    .sampler(1, ImageType::FLOAT_2D, "outer_mask_tx")
    .sampler(2, ImageType::FLOAT_2D, "flooded_inner_boundary_tx")
    .sampler(3, ImageType::FLOAT_2D, "flooded_outer_boundary_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_double_edge_mask_compute_gradient.glsl")
    .do_static_compilation(true);
