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

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(keyframe_shape_iface)
FLAT(VEC4, finalColor)
FLAT(VEC4, finalOutlineColor)
FLAT(VEC4, radii)
FLAT(VEC4, thresholds)
FLAT(UINT, finalFlags)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_keyframe_shape)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_IN(0, VEC4, color)
VERTEX_IN(1, VEC4, outlineColor)
VERTEX_IN(2, VEC2, pos)
VERTEX_IN(3, FLOAT, size)
VERTEX_IN(4, UINT, flags)
VERTEX_OUT(keyframe_shape_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC2, ViewportSize)
PUSH_CONSTANT(FLOAT, outline_scale)
VERTEX_SOURCE("gpu_shader_keyframe_shape_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_keyframe_shape_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
