/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_motion_blur_max_velocity_dilate)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "shutter_speed")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .storage_buf(0, Qualifier::READ_WRITE, "uint", "tile_indirection_buf[]")
    .compute_source("compositor_motion_blur_max_velocity_dilate.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_motion_blur)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "samples_count")
    .push_constant(Type::FLOAT, "shutter_speed")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_2D, "depth_tx")
    .sampler(2, ImageType::FLOAT_2D, "velocity_tx")
    .sampler(3, ImageType::FLOAT_2D, "max_velocity_tx")
    .storage_buf(0, Qualifier::READ, "uint", "tile_indirection_buf[]")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_motion_blur.glsl")
    .do_static_compilation(true);
