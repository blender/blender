/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_shared)
    .local_group_size(16, 16)
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_kuwahara_classic.glsl");

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_shared)
    .additional_info("compositor_kuwahara_classic_shared")
    .sampler(0, ImageType::FLOAT_2D, "input_tx");

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_constant_size)
    .additional_info("compositor_kuwahara_classic_convolution_shared")
    .push_constant(Type::INT, "size")
    .define("CONSTANT_SIZE")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_variable_size)
    .additional_info("compositor_kuwahara_classic_convolution_shared")
    .sampler(1, ImageType::FLOAT_2D, "size_tx")
    .define("VARIABLE_SIZE")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_shared)
    .additional_info("compositor_kuwahara_classic_shared")
    .define("SUMMED_AREA_TABLE")
    .sampler(0, ImageType::FLOAT_2D, "table_tx")
    .sampler(1, ImageType::FLOAT_2D, "squared_table_tx");

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_constant_size)
    .additional_info("compositor_kuwahara_classic_summed_area_table_shared")
    .push_constant(Type::INT, "size")
    .define("CONSTANT_SIZE")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_variable_size)
    .additional_info("compositor_kuwahara_classic_summed_area_table_shared")
    .sampler(2, ImageType::FLOAT_2D, "size_tx")
    .define("VARIABLE_SIZE")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_compute_structure_tensor)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "structure_tensor_img")
    .compute_source("compositor_kuwahara_anisotropic_compute_structure_tensor.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_shared)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "eccentricity")
    .push_constant(Type::FLOAT, "sharpness")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "structure_tensor_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_kuwahara_anisotropic.glsl");

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_constant_size)
    .additional_info("compositor_kuwahara_anisotropic_shared")
    .define("CONSTANT_SIZE")
    .push_constant(Type::FLOAT, "size")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_variable_size)
    .additional_info("compositor_kuwahara_anisotropic_shared")
    .define("VARIABLE_SIZE")
    .sampler(2, ImageType::FLOAT_2D, "size_tx")
    .do_static_compilation(true);
