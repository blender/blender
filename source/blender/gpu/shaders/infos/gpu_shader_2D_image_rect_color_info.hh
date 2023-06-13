/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_rect_color)
    .vertex_out(smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::VEC4, "rect_icon")
    .push_constant(Type::VEC4, "rect_geom")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .vertex_source("gpu_shader_2D_image_rect_vert.glsl")
    .fragment_source("gpu_shader_image_color_frag.glsl")
    .do_static_compilation(true);
