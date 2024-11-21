/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"
#include "select_defines.hh"

/* -------------------------------------------------------------------- */
/** \name Select ID for Edit Mesh Selection
 * \{ */

GPU_SHADER_INTERFACE_INFO(select_id_iface)
FLAT(INT, select_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(select_id_flat)
PUSH_CONSTANT(FLOAT, sizeVertex)
PUSH_CONSTANT(INT, offset)
PUSH_CONSTANT(FLOAT, retopologyOffset)
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, INT, index)
VERTEX_OUT(select_id_iface)
FRAGMENT_OUT(0, UINT, fragColor)
VERTEX_SOURCE("select_id_vert.glsl")
FRAGMENT_SOURCE("select_id_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_uniform)
DEFINE("UNIFORM_ID")
PUSH_CONSTANT(FLOAT, sizeVertex)
PUSH_CONSTANT(INT, select_id)
PUSH_CONSTANT(FLOAT, retopologyOffset)
VERTEX_IN(0, VEC3, pos)
FRAGMENT_OUT(0, UINT, fragColor)
VERTEX_SOURCE("select_id_vert.glsl")
FRAGMENT_SOURCE("select_id_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_flat_clipped)
ADDITIONAL_INFO(select_id_flat)
ADDITIONAL_INFO(drw_clipped)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(select_id_uniform_clipped)
ADDITIONAL_INFO(select_id_uniform)
ADDITIONAL_INFO(drw_clipped)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Used to patch overlay shaders. */
GPU_SHADER_CREATE_INFO(select_id_patch)
TYPEDEF_SOURCE("select_shader_shared.hh")
VERTEX_OUT(select_id_iface)
/* Need to make sure the depth & stencil comparison runs before the fragment shader. */
EARLY_FRAGMENT_TEST(true)
UNIFORM_BUF(SELECT_DATA, SelectInfoData, select_info_buf)
/* Select IDs for instanced draw-calls not using #PassMain. */
STORAGE_BUF(SELECT_ID_IN, READ, int, in_select_buf[])
/* Stores the result of the whole selection drawing. Content depends on selection mode. */
STORAGE_BUF(SELECT_ID_OUT, READ_WRITE, uint, out_select_buf[])
GPU_SHADER_CREATE_END()

/** \} */

GPU_SHADER_CREATE_INFO(select_debug_fullscreen)
ADDITIONAL_INFO(draw_fullscreen)
FRAGMENT_SOURCE("select_debug_frag.glsl")
SAMPLER(0, UINT_2D, image)
FRAGMENT_OUT(0, VEC4, fragColor)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
