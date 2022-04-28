/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* We use the normalized local position to avoid precision loss during interpolation. */
GPU_SHADER_INTERFACE_INFO(overlay_grid_iface, "").smooth(Type::VEC3, "local_pos");

GPU_SHADER_CREATE_INFO(overlay_grid)
    .do_static_compilation(true)
    .typedef_source("overlay_shader_shared.h")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(overlay_grid_iface)
    .fragment_out(0, Type::VEC4, "out_color")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .uniform_buf(3, "OVERLAY_GridData", "grid_buf")
    .push_constant(Type::VEC3, "plane_axes")
    .push_constant(Type::INT, "grid_flag")
    .vertex_source("grid_vert.glsl")
    .fragment_source("grid_frag.glsl")
    .additional_info("draw_view", "draw_globals");
