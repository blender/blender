/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_1D, "weights_tx")
    .sampler(2, ImageType::FLOAT_1D, "falloffs_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_morphological_distance_feather.glsl");

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_dilate)
    .additional_info("compositor_morphological_distance_feather_shared")
    .define("COMPARE(x, y)", "x > y")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_feather_erode)
    .additional_info("compositor_morphological_distance_feather_shared")
    .define("COMPARE(x, y)", "x < y")
    .do_static_compilation(true);
