/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(smooth_uv_iface, "").smooth(Type::VEC2, "uv");

GPU_SHADER_CREATE_INFO(gpu_shader_2D_area_borders)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(smooth_uv_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "rect")
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::FLOAT, "scale")
    .push_constant(Type::INT, "cornerLen")
    .vertex_source("gpu_shader_2D_area_borders_vert.glsl")
    .fragment_source("gpu_shader_2D_area_borders_frag.glsl")
    .do_static_compilation(true);
