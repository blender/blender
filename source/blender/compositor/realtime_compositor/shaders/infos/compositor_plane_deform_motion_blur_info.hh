/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_plane_deform_motion_blur)
    .local_group_size(16, 16)
    .push_constant(Type::INT, "number_of_motion_blur_samples")
    .uniform_buf(0, "mat4", "homography_matrices[64]")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "mask_img")
    .compute_source("compositor_plane_deform_motion_blur.glsl")
    .do_static_compilation(true);
