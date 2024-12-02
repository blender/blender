/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(image_engine_color_iface)
SMOOTH(VEC2, uv_screen)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(image_engine_color_shader)
VERTEX_IN(0, IVEC2, pos)
VERTEX_OUT(image_engine_color_iface)
FRAGMENT_OUT(0, VEC4, out_color)
PUSH_CONSTANT(VEC4, shuffle)
PUSH_CONSTANT(VEC2, far_near_distances)
PUSH_CONSTANT(IVEC2, offset)
PUSH_CONSTANT(INT, draw_flags)
PUSH_CONSTANT(BOOL, is_image_premultiplied)
SAMPLER(0, FLOAT_2D, image_tx)
SAMPLER(1, DEPTH_2D, depth_tx)
VERTEX_SOURCE("image_engine_color_vert.glsl")
FRAGMENT_SOURCE("image_engine_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(image_engine_depth_iface)
SMOOTH(VEC2, uv_image)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(image_engine_depth_shader)
VERTEX_IN(0, IVEC2, pos)
VERTEX_IN(1, VEC2, uv)
VERTEX_OUT(image_engine_depth_iface)
PUSH_CONSTANT(VEC4, min_max_uv)
VERTEX_SOURCE("image_engine_depth_vert.glsl")
FRAGMENT_SOURCE("image_engine_depth_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
