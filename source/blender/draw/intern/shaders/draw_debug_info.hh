/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

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
STORAGE_BUF(DRW_DEBUG_DRAW_SLOT, READ_WRITE, DRWDebugVert, drw_debug_verts_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(draw_debug_draw_display_iface, interp)
FLAT(VEC4, color)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(draw_debug_draw_display)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("draw_shader_shared.hh")
STORAGE_BUF(DRW_DEBUG_DRAW_SLOT, READ, DRWDebugVert, drw_debug_verts_buf[])
VERTEX_OUT(draw_debug_draw_display_iface)
FRAGMENT_OUT(0, VEC4, out_color)
PUSH_CONSTANT(MAT4, persmat)
VERTEX_SOURCE("draw_debug_draw_display_vert.glsl")
FRAGMENT_SOURCE("draw_debug_draw_display_frag.glsl")
GPU_SHADER_CREATE_END()

/** \} */
