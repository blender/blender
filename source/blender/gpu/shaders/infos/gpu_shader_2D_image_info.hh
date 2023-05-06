/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_common)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::VEC2, "texCoord")
    .vertex_out(smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .vertex_source("gpu_shader_2D_image_vert.glsl");
