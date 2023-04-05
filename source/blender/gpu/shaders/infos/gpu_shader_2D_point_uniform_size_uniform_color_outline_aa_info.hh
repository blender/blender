/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_outline_aa)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(smooth_radii_outline_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::VEC4, "outlineColor")
    .push_constant(Type::FLOAT, "size")
    .push_constant(Type::FLOAT, "outlineWidth")
    .vertex_source("gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl")
    .fragment_source("gpu_shader_point_uniform_color_outline_aa_frag.glsl")
    .do_static_compilation(true);
