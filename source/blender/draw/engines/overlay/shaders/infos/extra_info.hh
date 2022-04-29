/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Extra shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_iface, "")
    .no_perspective(Type::VEC2, "edgePos")
    .flat(Type::VEC2, "edgeStart")
    .flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_extra)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::INT, "vclass")
    /* Instance attributes. */
    .vertex_in(2, Type::MAT4, "inst_obmat")
    .vertex_in(3, Type::VEC4, "color")
    .vertex_out(overlay_extra_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("extra_vert.glsl")
    .fragment_source("extra_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_select)
    .do_static_compilation(true)
    .define("SELECT_EDGES")
    .additional_info("overlay_extra");

GPU_SHADER_CREATE_INFO(overlay_extra_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_extra_select_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_select", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Irradiance Grid
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_grid_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_extra_grid)
    .do_static_compilation(true)
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .push_constant(Type::MAT4, "gridModelMatrix")
    .push_constant(Type::BOOL, "isTransform")
    .vertex_out(overlay_extra_grid_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("extra_lightprobe_grid_vert.glsl")
    .fragment_source("gpu_shader_point_varying_color_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_grid_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_grid", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ground-lines
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_extra_groundline)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    /* Instance attributes. */
    .vertex_in(1, Type::VEC3, "inst_pos")
    .vertex_out(overlay_extra_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("extra_groundline_vert.glsl")
    .fragment_source("extra_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_groundline_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_groundline", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra wires
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_wire_iface, "")
    .no_perspective(Type::VEC2, "stipple_coord")
    .flat(Type::VEC2, "stipple_start")
    .flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_extra_wire)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "color")
    /* If colorid is equal to 0 (i.e: Not specified) use color attribute and stippling. */
    .vertex_in(2, Type::INT, "colorid")
    .vertex_out(overlay_extra_wire_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("extra_wire_vert.glsl")
    .fragment_source("extra_wire_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_select)
    .do_static_compilation(true)
    .define("SELECT_EDGES")
    .additional_info("overlay_extra_wire", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object)
    .do_static_compilation(true)
    .define("OBJECT_WIRE")
    .additional_info("overlay_extra_wire", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_select_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_wire_select", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_wire_object", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_wire", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra points
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_point_iface, "")
    .flat(Type::VEC4, "radii")
    .flat(Type::VEC4, "fillColor")
    .flat(Type::VEC4, "outlineColor");

GPU_SHADER_CREATE_INFO(overlay_extra_point)
    .do_static_compilation(true)
    /* TODO(fclem): Move the vertex shader to Overlay engine and remove this bypass. */
    .define("blender_srgb_to_framebuffer_space(a)", "a")
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::VEC4, "color")
    .vertex_out(overlay_extra_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("extra_point_vert.glsl")
    .fragment_source("gpu_shader_point_varying_color_varying_outline_aa_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_point_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_point", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface, "").smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::VEC4, "color")
    .vertex_out(overlay_extra_loose_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("extra_loose_point_vert.glsl")
    .fragment_source("extra_loose_point_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_loose_point", "drw_clipped");

/** \} */
