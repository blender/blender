/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

/* We leverage hardware interpolation to compute distance along the line. */
GPU_SHADER_INTERFACE_INFO(gpu_shader_line_dashed_interface, "")
    .no_perspective(Type::VEC2, "stipple_start") /* In screen space */
    .flat(Type::VEC2, "stipple_pos");            /* In screen space */

GPU_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(flat_color_iface)
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC2, "viewport_size")
    .push_constant(Type::FLOAT, "dash_width")
    .push_constant(Type::FLOAT, "udash_factor") /* if > 1.0, solid line. */
    /* TODO(fclem): Remove this. And decide to discard if color2 alpha is 0. */
    .push_constant(Type::INT, "colors_len") /* Enabled if > 0, 1 for solid line. */
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::VEC4, "color2")
    .vertex_out(gpu_shader_line_dashed_interface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("gpu_shader_3D_line_dashed_uniform_color_vert.glsl")
    .fragment_source("gpu_shader_2D_line_dashed_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color_clipped)
    .push_constant(Type::MAT4, "ModelMatrix")
    .additional_info("gpu_shader_3D_line_dashed_uniform_color")
    .additional_info("gpu_clip_planes")
    .do_static_compilation(true);
