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

GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_stroke_vert_iface, interp)
SMOOTH(float4, mColor)
SMOOTH(float2, mTexCoord)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
STORAGE_BUF_FREQ(0, read, GreasePencilStrokeData, gp_vert_data[], GEOMETRY)
VERTEX_OUT(gpencil_stroke_vert_iface)
FRAGMENT_OUT(0, float4, fragColor)
UNIFORM_BUF(0, GPencilStrokeData, gpencil_stroke_data)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float4x4, ProjectionMatrix)
FRAGMENT_SOURCE("gpu_shader_gpencil_stroke_frag.glsl")
VERTEX_SOURCE("gpu_shader_gpencil_stroke_vert.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
