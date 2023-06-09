/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(fullscreen_blit_iface, "").smooth(Type::VEC4, "uvcoordsvar");

GPU_SHADER_CREATE_INFO(fullscreen_blit)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(fullscreen_blit_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::VEC2, "fullscreen")
    .push_constant(Type::VEC2, "size")
    .push_constant(Type::VEC2, "dst_offset")
    .push_constant(Type::VEC2, "src_offset")
    .push_constant(Type::INT, "mip")
    .sampler(0, ImageType::FLOAT_2D, "imageTexture", Frequency::PASS)
    .vertex_source("gpu_shader_fullscreen_blit_vert.glsl")
    .fragment_source("gpu_shader_fullscreen_blit_frag.glsl")
    .do_static_compilation(true);
