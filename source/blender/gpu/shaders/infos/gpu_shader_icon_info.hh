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
#  define DO_CORNER_MASKING
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_icon)
DEFINE("DO_CORNER_MASKING")
VERTEX_OUT(smooth_icon_interp_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC4, finalColor)
PUSH_CONSTANT(VEC4, rect_icon)
PUSH_CONSTANT(VEC4, rect_geom)
PUSH_CONSTANT(FLOAT, text_width)
SAMPLER(0, FLOAT_2D, image)
VERTEX_SOURCE("gpu_shader_icon_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_icon_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_icon_multi)
VERTEX_IN(0, VEC2, pos)
VERTEX_OUT(flat_color_smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
UNIFORM_BUF(0, MultiIconCallData, multi_icon_data)
SAMPLER(0, FLOAT_2D, image)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_icon_multi_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_icon_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
