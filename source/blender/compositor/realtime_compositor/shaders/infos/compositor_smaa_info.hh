/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_smaa_edge_detection)
    .local_group_size(16, 16)
    .define("SMAA_GLSL_3")
    .define("SMAA_RT_METRICS",
            "vec4(1.0 / vec2(textureSize(input_tx, 0)), vec2(textureSize(input_tx, 0)))")
    .define("SMAA_LUMA_WEIGHT", "vec4(luminance_coefficients, 0.0)")
    .define("SMAA_THRESHOLD", "smaa_threshold")
    .define("SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR", "smaa_local_contrast_adaptation_factor")
    .push_constant(Type::VEC3, "luminance_coefficients")
    .push_constant(Type::FLOAT, "smaa_threshold")
    .push_constant(Type::FLOAT, "smaa_local_contrast_adaptation_factor")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "edges_img")
    .compute_source("compositor_smaa_edge_detection.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_smaa_blending_weight_calculation)
    .local_group_size(16, 16)
    .define("SMAA_GLSL_3")
    .define("SMAA_RT_METRICS",
            "vec4(1.0 / vec2(textureSize(edges_tx, 0)), vec2(textureSize(edges_tx, 0)))")
    .define("SMAA_CORNER_ROUNDING", "smaa_corner_rounding")
    .push_constant(Type::INT, "smaa_corner_rounding")
    .sampler(0, ImageType::FLOAT_2D, "edges_tx")
    .sampler(1, ImageType::FLOAT_2D, "area_tx")
    .sampler(2, ImageType::FLOAT_2D, "search_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "weights_img")
    .compute_source("compositor_smaa_blending_weight_calculation.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_shared)
    .local_group_size(16, 16)
    .define("SMAA_GLSL_3")
    .define("SMAA_RT_METRICS",
            "vec4(1.0 / vec2(textureSize(input_tx, 0)), vec2(textureSize(input_tx, 0)))")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "weights_tx")
    .compute_source("compositor_smaa_neighborhood_blending.glsl");

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_color)
    .additional_info("compositor_smaa_neighborhood_blending_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_smaa_neighborhood_blending_float)
    .additional_info("compositor_smaa_neighborhood_blending_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .do_static_compilation(true);
