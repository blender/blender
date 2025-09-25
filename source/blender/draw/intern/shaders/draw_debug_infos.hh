/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_shader_shared.hh"
#  define DRW_DEBUG_DRAW
#endif

#include "draw_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 *
 * Allows to draw lines and points just like the DRW_debug module functions.
 * \{ */

GPU_SHADER_CREATE_INFO(draw_debug_draw)
DEFINE("DRW_DEBUG_DRAW")
TYPEDEF_SOURCE("draw_shader_shared.hh")
STORAGE_BUF(DRW_DEBUG_DRAW_SLOT, read_write, DRWDebugVertPair, drw_debug_lines_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(draw_debug_draw_display_iface)
NO_PERSPECTIVE(float2, edge_pos)
FLAT(float2, edge_start)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(draw_debug_draw_display)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("draw_shader_shared.hh")
STORAGE_BUF(DRW_DEBUG_DRAW_SLOT, read, DRWDebugVertPair, in_debug_lines_buf[])
STORAGE_BUF(DRW_DEBUG_DRAW_FEEDBACK_SLOT, read_write, DRWDebugVertPair, out_debug_lines_buf[])
VERTEX_OUT(draw_debug_draw_display_iface)
FRAGMENT_OUT(0, float4, out_color)
FRAGMENT_OUT(1, float4, out_line_data)
PUSH_CONSTANT(float4x4, persmat)
PUSH_CONSTANT(float2, size_viewport)
VERTEX_SOURCE("draw_debug_draw_display_vert.glsl")
FRAGMENT_SOURCE("draw_debug_draw_display_frag.glsl")
GPU_SHADER_CREATE_END()

/** \} */
