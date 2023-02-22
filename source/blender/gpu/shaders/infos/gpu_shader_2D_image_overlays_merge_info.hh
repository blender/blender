/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_merge)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::VEC2, "texCoord")
    .vertex_out(smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::BOOL, "display_transform")
    .push_constant(Type::BOOL, "overlay")
    /* Sampler slots should match OCIO's. */
    .sampler(0, ImageType::FLOAT_2D, "image_texture")
    .sampler(1, ImageType::FLOAT_2D, "overlays_texture")
    .vertex_source("gpu_shader_2D_image_vert.glsl")
    .fragment_source("gpu_shader_image_overlays_merge_frag.glsl")
    .do_static_compilation(true);

/* Cycles display driver fallback shader. */
GPU_SHADER_CREATE_INFO(gpu_shader_cycles_display_fallback)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::VEC2, "texCoord")
    .vertex_out(smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::VEC2, "fullscreen")
    .sampler(0, ImageType::FLOAT_2D, "image_texture")
    .vertex_source("gpu_shader_display_fallback_vert.glsl")
    .fragment_source("gpu_shader_display_fallback_frag.glsl")
    .do_static_compilation(true);
