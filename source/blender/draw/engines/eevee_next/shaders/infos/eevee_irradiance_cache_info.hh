/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(eeve_debug_surfel_iface, "")
    .smooth(Type::VEC3, "P")
    .flat(Type::INT, "surfel_index");

GPU_SHADER_CREATE_INFO(eevee_debug_surfels)
    .additional_info("eevee_shared", "draw_view")
    .vertex_source("eevee_debug_surfels_vert.glsl")
    .vertex_out(eeve_debug_surfel_iface)
    .fragment_source("eevee_debug_surfels_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .storage_buf(0, Qualifier::READ, "DebugSurfel", "surfels_buf[]")
    .push_constant(Type::FLOAT, "surfel_radius")
    .do_static_compilation(true);
