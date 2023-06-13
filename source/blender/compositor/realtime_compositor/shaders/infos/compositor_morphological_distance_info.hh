/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_shared)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "radius")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_morphological_distance.glsl");

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_dilate)
    .additional_info("compositor_morphological_distance_shared")
    .define("OPERATOR(a, b)", "max(a, b)")
    .define("LIMIT", "FLT_MIN")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_erode)
    .additional_info("compositor_morphological_distance_shared")
    .define("OPERATOR(a, b)", "min(a, b)")
    .define("LIMIT", "FLT_MAX")
    .do_static_compilation(true);
