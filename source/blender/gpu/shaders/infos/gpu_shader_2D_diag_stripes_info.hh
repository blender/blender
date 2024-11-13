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

GPU_SHADER_CREATE_INFO(gpu_shader_2D_diag_stripes)
VERTEX_IN(0, VEC2, pos)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC4, color1)
PUSH_CONSTANT(VEC4, color2)
PUSH_CONSTANT(INT, size1)
PUSH_CONSTANT(INT, size2)
VERTEX_SOURCE("gpu_shader_2D_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_diag_stripes_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
