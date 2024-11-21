/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_merge)
VERTEX_IN(0, VEC2, pos)
VERTEX_IN(1, VEC2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(BOOL, display_transform)
PUSH_CONSTANT(BOOL, overlay)
PUSH_CONSTANT(BOOL, use_hdr)
/* Sampler slots should match OCIO's. */
SAMPLER(0, FLOAT_2D, image_texture)
SAMPLER(1, FLOAT_2D, overlays_texture)
VERTEX_SOURCE("gpu_shader_2D_image_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_image_overlays_merge_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Cycles display driver fallback shader. */
GPU_SHADER_CREATE_INFO(gpu_shader_cycles_display_fallback)
VERTEX_IN(0, VEC2, pos)
VERTEX_IN(1, VEC2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(VEC2, fullscreen)
SAMPLER(0, FLOAT_2D, image_texture)
VERTEX_SOURCE("gpu_shader_display_fallback_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_display_fallback_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
