/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(depth_2d_update_iface)
SMOOTH(float2, texCoord_interp)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(depth_2d_update_info_base)
VERTEX_IN(0, float2, pos)
VERTEX_OUT(depth_2d_update_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float2, extent)
PUSH_CONSTANT(float2, offset)
PUSH_CONSTANT(float2, size)
PUSH_CONSTANT(int, mip)
DEPTH_WRITE(DepthWrite::ANY)
VERTEX_SOURCE("depth_2d_update_vert.glsl");
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(depth_2d_update_float)
METAL_BACKEND_ONLY()
FRAGMENT_SOURCE("depth_2d_update_float_frag.glsl")
SAMPLER(0, sampler2D, source_data)
ADDITIONAL_INFO(depth_2d_update_info_base)
DO_STATIC_COMPILATION()
DEPTH_WRITE(DepthWrite::ANY);
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(depth_2d_update_int24)
METAL_BACKEND_ONLY()
FRAGMENT_SOURCE("depth_2d_update_int24_frag.glsl")
ADDITIONAL_INFO(depth_2d_update_info_base)
SAMPLER(0, isampler2D, source_data)
DO_STATIC_COMPILATION()
DEPTH_WRITE(DepthWrite::ANY);
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(depth_2d_update_int32)
METAL_BACKEND_ONLY()
FRAGMENT_SOURCE("depth_2d_update_int32_frag.glsl")
ADDITIONAL_INFO(depth_2d_update_info_base)
SAMPLER(0, isampler2D, source_data)
DO_STATIC_COMPILATION()
DEPTH_WRITE(DepthWrite::ANY);
GPU_SHADER_CREATE_END()
