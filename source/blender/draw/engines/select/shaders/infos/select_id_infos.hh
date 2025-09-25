/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"
#include "select_defines.hh"

/* -------------------------------------------------------------------- */
/** \name Select ID for Edit Mesh Selection
 * \{ */

GPU_SHADER_INTERFACE_INFO(select_id_iface)
FLAT(int, select_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(select_id_flat)
PUSH_CONSTANT(float, vertex_size)
PUSH_CONSTANT(int, offset)
PUSH_CONSTANT(float, retopology_offset)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, int, index)
VERTEX_OUT(select_id_iface)
FRAGMENT_OUT(0, uint, frag_color)
VERTEX_SOURCE("select_id_vert.glsl")
FRAGMENT_SOURCE("select_id_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_view)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_uniform)
DEFINE("UNIFORM_ID")
PUSH_CONSTANT(float, vertex_size)
PUSH_CONSTANT(int, select_id)
PUSH_CONSTANT(float, retopology_offset)
VERTEX_IN(0, float3, pos)
FRAGMENT_OUT(0, uint, frag_color)
VERTEX_SOURCE("select_id_vert.glsl")
FRAGMENT_SOURCE("select_id_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_view)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_flat_clipped)
ADDITIONAL_INFO(select_id_flat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(drw_clipped)
DEFINE("USE_WORLD_CLIP_PLANES")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_uniform_clipped)
ADDITIONAL_INFO(select_id_uniform)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(drw_clipped)
DEFINE("USE_WORLD_CLIP_PLANES")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */

GPU_SHADER_CREATE_INFO(select_debug_fullscreen)
ADDITIONAL_INFO(gpu_fullscreen)
FRAGMENT_SOURCE("select_debug_frag.glsl")
SAMPLER(0, usampler2D, image)
FRAGMENT_OUT(0, float4, frag_color)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
