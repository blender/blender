/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_pick)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "first_layer_tx")
    .image(0, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_cryptomatte_pick.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_matte)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "identifiers_count")
    .push_constant(Type::FLOAT, "identifiers", 32)
    .sampler(0, ImageType::FLOAT_2D, "layer_tx")
    .image(0, GPU_R16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "matte_img")
    .compute_source("compositor_cryptomatte_matte.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_image)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "matte_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_cryptomatte_image.glsl")
    .do_static_compilation(true);
