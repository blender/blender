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
#  include "gpu_srgb_to_framebuffer_space_info.hh"
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_aa)
VERTEX_IN(0, VEC2, pos)
VERTEX_OUT(smooth_radii_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC4, color)
PUSH_CONSTANT(FLOAT, size)
VERTEX_SOURCE("gpu_shader_2D_point_uniform_size_aa_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_point_uniform_color_aa_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
