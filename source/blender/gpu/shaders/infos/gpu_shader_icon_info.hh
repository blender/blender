/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_icon)
    .define("DO_CORNER_MASKING")
    .vertex_out(smooth_icon_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "finalColor")
    .push_constant(Type::VEC4, "rect_icon")
    .push_constant(Type::VEC4, "rect_geom")
    .push_constant(Type::FLOAT, "text_width")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .vertex_source("gpu_shader_icon_vert.glsl")
    .fragment_source("gpu_shader_icon_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_icon_multi)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(flat_color_smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .uniform_buf(0, "MultiIconCallData", "multi_icon_data")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .typedef_source("GPU_shader_shared.h")
    .vertex_source("gpu_shader_icon_multi_vert.glsl")
    .fragment_source("gpu_shader_icon_frag.glsl")
    .do_static_compilation(true);
