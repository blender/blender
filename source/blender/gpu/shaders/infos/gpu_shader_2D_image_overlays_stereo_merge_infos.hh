/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_stereo_merge)
VERTEX_IN(0, float2, pos)
FRAGMENT_OUT(0, float4, overlayColor)
FRAGMENT_OUT(1, float4, imageColor)
SAMPLER(0, sampler2D, imageTexture)
SAMPLER(1, sampler2D, overlayTexture)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(int, stereoDisplaySettings)
VERTEX_SOURCE("gpu_shader_2D_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_image_overlays_stereo_merge_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
