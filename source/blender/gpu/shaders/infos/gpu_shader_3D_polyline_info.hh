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
#  include "gpu_index_load_info.hh"
#  include "gpu_srgb_to_framebuffer_space_info.hh"
#  define SMOOTH_WIDTH 1.0
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_shader_3D_polyline_iface)
SMOOTH(VEC4, final_color)
SMOOTH(FLOAT, clip)
NO_PERSPECTIVE(FLOAT, smoothline)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline)
DEFINE_VALUE("SMOOTH_WIDTH", "1.0")
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC2, viewportSize)
PUSH_CONSTANT(FLOAT, lineWidth)
PUSH_CONSTANT(BOOL, lineSmooth)
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_POS_BUF_SLOT, READ, float, pos[], GEOMETRY)
PUSH_CONSTANT(IVEC2, gpu_attr_0)
PUSH_CONSTANT(IVEC3, gpu_vert_stride_count_offset)
PUSH_CONSTANT(INT, gpu_attr_0_len)
PUSH_CONSTANT(BOOL, gpu_attr_0_fetch_int)
PUSH_CONSTANT(BOOL, gpu_attr_1_fetch_unorm8)
VERTEX_OUT(gpu_shader_3D_polyline_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("gpu_shader_3D_polyline_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_3D_polyline_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
ADDITIONAL_INFO(gpu_index_buffer_load)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color)
DO_STATIC_COMPILATION()
DEFINE("UNIFORM")
PUSH_CONSTANT(VEC4, color)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped)
DO_STATIC_COMPILATION()
/* TODO(fclem): Put in a UBO to fit the 128byte requirement. */
PUSH_CONSTANT(MAT4, ModelMatrix)
PUSH_CONSTANT(VEC4, ClipPlane)
DEFINE("CLIP")
ADDITIONAL_INFO(gpu_shader_3D_polyline_uniform_color)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color)
DO_STATIC_COMPILATION()
DEFINE("FLAT")
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_COL_BUF_SLOT, READ, float, color[], GEOMETRY)
PUSH_CONSTANT(IVEC2, gpu_attr_1)
PUSH_CONSTANT(INT, gpu_attr_1_len)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color)
DO_STATIC_COMPILATION()
DEFINE("SMOOTH")
STORAGE_BUF_FREQ(GPU_SSBO_POLYLINE_COL_BUF_SLOT, READ, float, color[], GEOMETRY)
PUSH_CONSTANT(IVEC2, gpu_attr_1)
PUSH_CONSTANT(INT, gpu_attr_1_len)
ADDITIONAL_INFO(gpu_shader_3D_polyline)
GPU_SHADER_CREATE_END()
