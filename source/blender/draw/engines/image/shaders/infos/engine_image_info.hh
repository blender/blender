/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(image_engine_color_iface)
SMOOTH(float2, uv_screen)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(image_engine_color_shader)
VERTEX_IN(0, int2, pos)
VERTEX_OUT(image_engine_color_iface)
FRAGMENT_OUT(0, float4, out_color)
PUSH_CONSTANT(float4, shuffle)
PUSH_CONSTANT(float2, far_near_distances)
PUSH_CONSTANT(int2, offset)
PUSH_CONSTANT(int, draw_flags)
PUSH_CONSTANT(bool, is_image_premultiplied)
SAMPLER(0, sampler2D, image_tx)
SAMPLER(1, sampler2DDepth, depth_tx)
VERTEX_SOURCE("image_engine_color_vert.glsl")
FRAGMENT_SOURCE("image_engine_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(image_engine_depth_iface)
SMOOTH(float2, uv_image)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(image_engine_depth_shader)
VERTEX_IN(0, int2, pos)
VERTEX_IN(1, float2, uv)
VERTEX_OUT(image_engine_depth_iface)
PUSH_CONSTANT(float4, min_max_uv)
VERTEX_SOURCE("image_engine_depth_vert.glsl")
FRAGMENT_SOURCE("image_engine_depth_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
