/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"

#  include "overlay_shader_shared.hh"
#endif

#include "overlay_common_infos.hh"

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_grid_iface, vertex_out)
SMOOTH(float3, pos)
SMOOTH(float2, coord)
GPU_SHADER_NAMED_INTERFACE_END(vertex_out)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_grid_iface_flat, vertex_out_flat)
FLAT(float, alpha)
FLAT(float, emphasis)
GPU_SHADER_NAMED_INTERFACE_END(vertex_out_flat)

GPU_SHADER_INTERFACE_INFO(overlay_grid_iface_misc)
FLAT(float2, edge_start)
NO_PERSPECTIVE(float2, edge_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_grid_next)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_grid_iface)
VERTEX_OUT(overlay_grid_iface_flat)
VERTEX_OUT(overlay_grid_iface_misc)
FRAGMENT_OUT(0, float4, out_color)
FRAGMENT_OUT(1, float4, line_output)
UNIFORM_BUF(3, OVERLAY_GridData, grid_buf)
PUSH_CONSTANT(int, grid_flag)
PUSH_CONSTANT(int, grid_iter)
VERTEX_SOURCE("overlay_grid_vert.glsl")
FRAGMENT_SOURCE("overlay_grid_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_grid_background)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float4, ucolor)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_tiled_image_borders_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
DEFINE_VALUE("tile_pos", "float3(0.0f, 0.0f, 1.0f)") /* Background is placed at z=1. */
PUSH_CONSTANT(float3, tile_scale)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_grid_image)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float4, ucolor)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_tiled_image_borders_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, float3, tile_pos_buf[])
DEFINE_VALUE("tile_pos", "tile_pos_buf[gl_InstanceID]")
BUILTINS(BuiltinBits::INSTANCE_ID)
DEFINE_VALUE("tile_scale", "float3(1.0f)");
GPU_SHADER_CREATE_END()
