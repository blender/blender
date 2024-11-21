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

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_varying_size_varying_color)
VERTEX_IN(0, VEC2, pos)
VERTEX_IN(1, FLOAT, size)
VERTEX_IN(2, VEC4, color)
VERTEX_OUT(smooth_color_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_2D_point_varying_size_varying_color_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_point_varying_color_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
