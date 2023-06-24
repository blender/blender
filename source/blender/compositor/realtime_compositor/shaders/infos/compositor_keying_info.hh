/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_keying_extract_chroma)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_keying_extract_chroma.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_keying_replace_chroma)
    .local_group_size(16, 16)
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "new_chroma_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_keying_replace_chroma.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_keying_compute_matte)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "key_balance")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "key_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_keying_compute_matte.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_keying_tweak_matte)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "compute_edges")
    .push_constant(Type::BOOL, "apply_core_matte")
    .push_constant(Type::BOOL, "apply_garbage_matte")
    .push_constant(Type::INT, "edge_search_radius")
    .push_constant(Type::FLOAT, "edge_tolerance")
    .push_constant(Type::FLOAT, "black_level")
    .push_constant(Type::FLOAT, "white_level")
    .sampler(0, ImageType::FLOAT_2D, "input_matte_tx")
    .sampler(1, ImageType::FLOAT_2D, "garbage_matte_tx")
    .sampler(2, ImageType::FLOAT_2D, "core_matte_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_matte_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_edges_img")
    .compute_source("compositor_keying_tweak_matte.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_keying_compute_image)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "despill_factor")
    .push_constant(Type::FLOAT, "despill_balance")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "matte_tx")
    .sampler(2, ImageType::FLOAT_2D, "key_tx")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_keying_compute_image.glsl")
    .do_static_compilation(true);
