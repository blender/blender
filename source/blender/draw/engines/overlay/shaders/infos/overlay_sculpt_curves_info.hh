/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_selection_iface, "")
    .smooth(Type::FLOAT, "mask_weight");

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)
    .do_static_compilation(true)
    .push_constant(Type::BOOL, "is_point_domain")
    .push_constant(Type::FLOAT, "selection_opacity")
    .sampler(0, ImageType::FLOAT_BUFFER, "selection_tx")
    .vertex_out(overlay_sculpt_curves_selection_iface)
    .vertex_source("overlay_sculpt_curves_selection_vert.glsl")
    .fragment_source("overlay_sculpt_curves_selection_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .additional_info("draw_hair", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_sculpt_curves_selection", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_cage_iface, "")
    .no_perspective(Type::VEC2, "edgePos")
    .flat(Type::VEC2, "edgeStart")
    .smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::FLOAT, "selection")
    .vertex_out(overlay_sculpt_curves_cage_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .push_constant(Type::FLOAT, "opacity")
    .vertex_source("overlay_sculpt_curves_cage_vert.glsl")
    .fragment_source("overlay_extra_frag.glsl")
    .additional_info("draw_modelmat", "draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_sculpt_curves_cage", "drw_clipped");
