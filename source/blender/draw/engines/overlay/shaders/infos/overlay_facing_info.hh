/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_facing)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_source("overlay_facing_vert.glsl")
    .fragment_source("overlay_facing_frag.glsl")
    .fragment_out(0, Type::VEC4, "fragColor")
    .additional_info("draw_mesh", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_facing_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_facing", "drw_clipped");
