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

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_stereo_merge)
VERTEX_IN(0, VEC2, pos)
FRAGMENT_OUT(0, VEC4, overlayColor)
FRAGMENT_OUT(1, VEC4, imageColor)
SAMPLER(0, FLOAT_2D, imageTexture)
SAMPLER(1, FLOAT_2D, overlayTexture)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(INT, stereoDisplaySettings)
VERTEX_SOURCE("gpu_shader_2D_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_image_overlays_stereo_merge_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
