/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_sun_beams)
    .local_group_size(16, 16)
    .push_constant(Type::VEC2, "source")
    .push_constant(Type::FLOAT, "max_ray_length")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_sun_beams.glsl")
    .do_static_compilation(true);
