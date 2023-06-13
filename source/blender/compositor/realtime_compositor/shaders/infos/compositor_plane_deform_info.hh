/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_plane_deform)
    .local_group_size(16, 16)
    .push_constant(Type::MAT4, "homography_matrix")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "mask_img")
    .compute_source("compositor_plane_deform.glsl")
    .do_static_compilation(true);
