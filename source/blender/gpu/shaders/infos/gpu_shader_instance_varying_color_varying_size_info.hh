/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_instance_varying_color_varying_size)
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, color)
VERTEX_IN(2, FLOAT, size)
VERTEX_IN(3, MAT4, InstanceModelMatrix)
VERTEX_OUT(flat_color_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ViewProjectionMatrix)
VERTEX_SOURCE("gpu_shader_instance_variying_size_variying_color_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_flat_color_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
