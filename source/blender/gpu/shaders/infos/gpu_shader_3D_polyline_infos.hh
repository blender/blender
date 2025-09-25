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
#  include "gpu_index_load_infos.hh"
#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#  define SMOOTH_WIDTH 1.0f
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_shader_3D_polyline_iface)
SMOOTH(float4, final_color)
SMOOTH(float, clip)
NO_PERSPECTIVE(float, smoothline)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline)
DEFINE_VALUE("SMOOTH_WIDTH", "1.0f")
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float2, viewportSize)
PUSH_CONSTANT(float, lineWidth)
PUSH_CONSTANT(bool, lineSmooth)
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_POS_BUF_SLOT, read, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int3, gpu_vert_stride_count_offset)
PUSH_CONSTANT(int, gpu_attr_0_len)
PUSH_CONSTANT(bool, gpu_attr_0_fetch_int)
PUSH_CONSTANT(bool, gpu_attr_1_fetch_unorm8)
VERTEX_OUT(gpu_shader_3D_polyline_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("gpu_shader_3D_polyline_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_3D_polyline_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
ADDITIONAL_INFO(gpu_index_buffer_load)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color)
DO_STATIC_COMPILATION()
DEFINE("UNIFORM")
PUSH_CONSTANT(float4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped)
DO_STATIC_COMPILATION()
/* TODO(fclem): Put in a UBO to fit the 128byte requirement. */
PUSH_CONSTANT(float4x4, ModelMatrix)
PUSH_CONSTANT(float4, ClipPlane)
DEFINE("CLIP")
ADDITIONAL_INFO(gpu_shader_3D_polyline_uniform_color)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color)
DO_STATIC_COMPILATION()
DEFINE("FLAT")
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_COL_BUF_SLOT, read, float, color[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int, gpu_attr_1_len)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color)
DO_STATIC_COMPILATION()
DEFINE("SMOOTH")
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_COL_BUF_SLOT, read, float, color[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int, gpu_attr_1_len)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()
