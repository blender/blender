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

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_rect_color)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC4, color)
PUSH_CONSTANT(VEC4, rect_icon)
PUSH_CONSTANT(VEC4, rect_geom)
SAMPLER(0, FLOAT_2D, image)
VERTEX_SOURCE("gpu_shader_2D_image_rect_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_image_color_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
