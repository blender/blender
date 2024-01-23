/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_map_uv_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "uv_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img");

GPU_SHADER_CREATE_INFO(compositor_map_uv_anisotropic)
    .additional_info("compositor_map_uv_shared")
    .push_constant(Type::FLOAT, "gradient_attenuation_factor")
    .compute_source("compositor_map_uv_anisotropic.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_map_uv_nearest_neighbour)
    .additional_info("compositor_map_uv_shared")
    .compute_source("compositor_map_uv_nearest_neighbour.glsl")
    .do_static_compilation(true);
