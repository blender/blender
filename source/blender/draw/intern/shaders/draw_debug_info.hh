/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Debug print
 *
 * Allows print() function to have logging support inside shaders.
 * \{ */

GPU_SHADER_CREATE_INFO(draw_debug_print)
    .define("DRW_DEBUG_PRINT")
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(DRW_DEBUG_PRINT_SLOT, Qualifier::READ_WRITE, "uint", "drw_debug_print_buf[]");

GPU_SHADER_INTERFACE_INFO(draw_debug_print_display_iface, "").flat(Type::UINT, "char_index");

GPU_SHADER_CREATE_INFO(draw_debug_print_display)
    .do_static_compilation(true)
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(DRW_DEBUG_PRINT_SLOT, Qualifier::READ, "uint", "drw_debug_print_buf[]")
    .vertex_out(draw_debug_print_display_iface)
    .fragment_out(0, Type::VEC4, "out_color")
    .push_constant(Type::VEC2, "viewport_size")
    .vertex_source("draw_debug_print_display_vert.glsl")
    .fragment_source("draw_debug_print_display_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 *
 * Allows to draw lines and points just like the DRW_debug module functions.
 * \{ */

GPU_SHADER_CREATE_INFO(draw_debug_draw)
    .define("DRW_DEBUG_DRAW")
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(DRW_DEBUG_DRAW_SLOT,
                 Qualifier::READ_WRITE,
                 "DRWDebugVert",
                 "drw_debug_verts_buf[]");

GPU_SHADER_INTERFACE_INFO(draw_debug_draw_display_iface, "interp").flat(Type::VEC4, "color");

GPU_SHADER_CREATE_INFO(draw_debug_draw_display)
    .do_static_compilation(true)
    .typedef_source("draw_shader_shared.hh")
    .storage_buf(DRW_DEBUG_DRAW_SLOT, Qualifier::READ, "DRWDebugVert", "drw_debug_verts_buf[]")
    .vertex_out(draw_debug_draw_display_iface)
    .fragment_out(0, Type::VEC4, "out_color")
    .push_constant(Type::MAT4, "persmat")
    .vertex_source("draw_debug_draw_display_vert.glsl")
    .fragment_source("draw_debug_draw_display_frag.glsl");

/** \} */
