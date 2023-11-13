/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "incomplete_x_prologues_img")
    .image(1, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "incomplete_y_prologues_img")
    .compute_source("compositor_summed_area_table_compute_incomplete_prologues.glsl");

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_identity)
    .additional_info("compositor_summed_area_table_compute_incomplete_prologues_shared")
    .define("OPERATION(value)", "value")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_square)
    .additional_info("compositor_summed_area_table_compute_incomplete_prologues_shared")
    .define("OPERATION(value)", "value * value")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_x_prologues)
    .local_group_size(16)
    .sampler(0, ImageType::FLOAT_2D, "incomplete_x_prologues_tx")
    .image(0, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "complete_x_prologues_img")
    .image(1, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "complete_x_prologues_sum_img")
    .compute_source("compositor_summed_area_table_compute_complete_x_prologues.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_y_prologues)
    .local_group_size(16)
    .sampler(0, ImageType::FLOAT_2D, "incomplete_y_prologues_tx")
    .sampler(1, ImageType::FLOAT_2D, "complete_x_prologues_sum_tx")
    .image(0, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "complete_y_prologues_img")
    .compute_source("compositor_summed_area_table_compute_complete_y_prologues.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_shared)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "complete_x_prologues_tx")
    .sampler(2, ImageType::FLOAT_2D, "complete_y_prologues_tx")
    .image(0, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_summed_area_table_compute_complete_blocks.glsl");

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_identity)
    .additional_info("compositor_summed_area_table_compute_complete_blocks_shared")
    .define("OPERATION(value)", "value")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_square)
    .additional_info("compositor_summed_area_table_compute_complete_blocks_shared")
    .define("OPERATION(value)", "value * value")
    .do_static_compilation(true);
