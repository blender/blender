/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_icon)
    .vertex_out(smooth_icon_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::VEC4, "rect_icon")
    .push_constant(Type::VEC4, "rect_geom")
    .push_constant(Type::FLOAT, "text_width")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .vertex_source("gpu_shader_icon_vert.glsl")
    .fragment_source("gpu_shader_icon_frag.glsl")
    .do_static_compilation(true);
