/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_shared)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "radius")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_kuwahara_classic.glsl");

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic)
    .additional_info("compositor_kuwahara_classic_shared")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table)
    .additional_info("compositor_kuwahara_classic_shared")
    .define("SUMMED_AREA_TABLE")
    .sampler(0, ImageType::FLOAT_2D, "table_tx")
    .sampler(1, ImageType::FLOAT_2D, "squared_table_tx")
    .do_static_compilation(true);
