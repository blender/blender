/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_mask_iface, "")
    .flat(Type::VEC3, "faceset_color")
    .smooth(Type::FLOAT, "mask_color")
    .smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_sculpt_mask)
    .do_static_compilation(true)
    .push_constant(Type::FLOAT, "maskOpacity")
    .push_constant(Type::FLOAT, "faceSetsOpacity")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "fset")
    .vertex_in(2, Type::FLOAT, "msk")
    .vertex_out(overlay_sculpt_mask_iface)
    .vertex_source("overlay_sculpt_mask_vert.glsl")
    .fragment_source("overlay_sculpt_mask_frag.glsl")
    .fragment_out(0, Type::VEC4, "fragColor")
    .additional_info("draw_mesh", "draw_object_infos", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_sculpt_mask_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_sculpt_mask", "drw_clipped");
