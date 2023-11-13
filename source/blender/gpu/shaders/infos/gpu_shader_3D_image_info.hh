/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image_common)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC2, "texCoord")
    .vertex_out(smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .vertex_source("gpu_shader_3D_image_vert.glsl");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image)
    .additional_info("gpu_shader_3D_image_common")
    .fragment_source("gpu_shader_image_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_3D_image_color)
    .additional_info("gpu_shader_3D_image_common")
    .push_constant(Type::VEC4, "color")
    .fragment_source("gpu_shader_image_color_frag.glsl")
    .do_static_compilation(true);
