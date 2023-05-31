/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_bokeh_image)
    .local_group_size(16, 16)
    .push_constant(Type::FLOAT, "exterior_angle")
    .push_constant(Type::FLOAT, "rotation")
    .push_constant(Type::FLOAT, "roundness")
    .push_constant(Type::FLOAT, "catadioptric")
    .push_constant(Type::FLOAT, "lens_shift")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .compute_source("compositor_bokeh_image.glsl")
    .do_static_compilation(true);
