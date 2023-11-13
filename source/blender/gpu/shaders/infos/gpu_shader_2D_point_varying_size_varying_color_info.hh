/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_varying_size_varying_color)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::FLOAT, "size")
    .vertex_in(2, Type::VEC4, "color")
    .vertex_out(smooth_color_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .vertex_source("gpu_shader_2D_point_varying_size_varying_color_vert.glsl")
    .fragment_source("gpu_shader_point_varying_color_frag.glsl")
    .do_static_compilation(true);
