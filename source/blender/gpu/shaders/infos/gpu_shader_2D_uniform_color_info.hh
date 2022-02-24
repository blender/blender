/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_uniform_color)
    .vertex_in(0, Type::VEC2, "pos")
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color")
    .vertex_source("gpu_shader_2D_vert.glsl")
    .fragment_source("gpu_shader_uniform_color_frag.glsl")
    .additional_info("gpu_srgb_to_framebuffer_space")
    .do_static_compilation(true);
