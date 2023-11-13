/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_3D_point_varying_size_varying_color)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "color")
    .vertex_in(2, Type::FLOAT, "size")
    .vertex_out(smooth_color_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .vertex_source("gpu_shader_3D_point_varying_size_varying_color_vert.glsl")
    .fragment_source("gpu_shader_point_varying_color_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_3D_point_uniform_size_uniform_color_aa)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(smooth_radii_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::FLOAT, "size")
    .vertex_source("gpu_shader_3D_point_uniform_size_aa_vert.glsl")
    .fragment_source("gpu_shader_point_uniform_color_aa_frag.glsl")
    .additional_info("gpu_srgb_to_framebuffer_space")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_3D_point_uniform_size_uniform_color_aa_clipped)
    .additional_info("gpu_shader_3D_point_uniform_size_uniform_color_aa")
    .additional_info("gpu_clip_planes")
    .do_static_compilation(true);
