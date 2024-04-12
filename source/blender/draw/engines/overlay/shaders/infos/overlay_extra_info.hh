/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
    .typedef_source("overlay_shader_shared.h")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::INT, "vclass")
    /* Instance attributes. */
    .vertex_in(2, Type::VEC4, "color")
    .vertex_in(3, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_extra_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("overlay_extra_vert.glsl")
    .fragment_source("overlay_extra_frag.glsl")
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
    .vertex_source("overlay_extra_lightprobe_grid_vert.glsl")
    .fragment_source("overlay_point_varying_color_frag.glsl")
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
    .vertex_source("overlay_extra_groundline_vert.glsl")
    .fragment_source("overlay_extra_frag.glsl")
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
    .vertex_source("overlay_extra_wire_vert.glsl")
    .fragment_source("overlay_extra_wire_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_select)
    .do_static_compilation(true)
    .define("SELECT_EDGES")
    .additional_info("overlay_extra_wire");

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object)
    .do_static_compilation(true)
    .define("OBJECT_WIRE")
    .additional_info("overlay_extra_wire");

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
    .push_constant(Type::VEC4, "ucolor")
    .vertex_out(overlay_extra_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_extra_point_vert.glsl")
    .fragment_source("overlay_point_varying_color_varying_outline_aa_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_point_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_point", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface, "").smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::VEC4, "ucolor")
    .vertex_out(overlay_extra_loose_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("overlay_extra_loose_point_vert.glsl")
    .fragment_source("overlay_extra_loose_point_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_extra_loose_point", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Path
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_motion_path_line_iface, "interp").smooth(Type::VEC4, "color");
GPU_SHADER_INTERFACE_INFO(overlay_motion_path_line_flat_iface, "interp_flat")
    .flat(Type::VEC2, "ss_pos");

GPU_SHADER_CREATE_INFO(overlay_motion_path_line)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::IVEC4, "mpathLineSettings")
    .push_constant(Type::BOOL, "selected")
    .push_constant(Type::VEC3, "customColorPre")
    .push_constant(Type::VEC3, "customColorPost")
    .push_constant(Type::INT, "lineThickness") /* In pixels. */
    .push_constant(Type::MAT4, "camera_space_matrix")
    .vertex_out(overlay_motion_path_line_iface)
    .vertex_out(overlay_motion_path_line_flat_iface)
    .geometry_out(overlay_motion_path_line_iface)
    .geometry_layout(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_motion_path_line_vert.glsl")
    .geometry_source("overlay_motion_path_line_geom.glsl")
    .fragment_source("overlay_motion_path_line_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::IVEC4, "mpathLineSettings")
    .push_constant(Type::BOOL, "selected")
    .push_constant(Type::VEC3, "customColorPre")
    .push_constant(Type::VEC3, "customColorPost")
    .push_constant(Type::INT, "lineThickness") /* In pixels. */
    .vertex_out(overlay_motion_path_line_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_motion_path_line_vert_no_geom.glsl")
    .fragment_source("overlay_motion_path_line_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_motion_path_line", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_clipped_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .additional_info("overlay_motion_path_line_no_geom", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_motion_path_point_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_motion_path_point)
    .do_static_compilation(true)
    .typedef_source("overlay_shader_shared.h")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::UINT, "flag")
    .push_constant(Type::IVEC4, "mpathPointSettings")
    .push_constant(Type::BOOL, "showKeyFrames")
    .push_constant(Type::VEC3, "customColorPre")
    .push_constant(Type::VEC3, "customColorPost")
    .push_constant(Type::MAT4, "camera_space_matrix")
    .vertex_out(overlay_motion_path_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_motion_path_point_vert.glsl")
    .fragment_source("overlay_point_varying_color_frag.glsl")
    .additional_info("draw_view", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_motion_path_point_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_motion_path_point", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Empty
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_image_iface, "").smooth(Type::VEC2, "uvs");

GPU_SHADER_CREATE_INFO(overlay_image)
    .do_static_compilation(true)
    .push_constant(Type::BOOL, "depthSet")
    .push_constant(Type::BOOL, "isCameraBackground")
    .push_constant(Type::BOOL, "imgPremultiplied")
    .push_constant(Type::BOOL, "imgAlphaBlend")
    .push_constant(Type::VEC4, "ucolor")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(overlay_image_iface)
    .sampler(0, ImageType::FLOAT_2D, "imgTexture")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_image_vert.glsl")
    .fragment_source("overlay_image_frag.glsl")
    .additional_info("draw_mesh");

GPU_SHADER_CREATE_INFO(overlay_image_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_image", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPencil Canvas
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas)
    .do_static_compilation(true)
    .vertex_out(overlay_extra_iface)
    .push_constant(Type::VEC4, "color")
    .push_constant(Type::VEC3, "xAxis")
    .push_constant(Type::VEC3, "yAxis")
    .push_constant(Type::VEC3, "origin")
    .push_constant(Type::INT, "halfLineCount")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_source("overlay_edit_gpencil_canvas_vert.glsl")
    .fragment_source("overlay_extra_frag.glsl")
    .additional_info("draw_mesh", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_gpencil_canvas", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_particle_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_particle)
    .sampler(0, ImageType::FLOAT_1D, "weightTex")
    .push_constant(Type::VEC4, "ucolor") /* Draw-size packed in alpha. */
    .vertex_in(0, Type::VEC3, "part_pos")
    .vertex_in(1, Type::VEC4, "part_rot")
    .vertex_in(2, Type::FLOAT, "part_val")
    .vertex_out(overlay_particle_iface)
    .vertex_source("overlay_particle_vert.glsl")
    .additional_info("draw_globals");

GPU_SHADER_CREATE_INFO(overlay_particle_dot)
    .do_static_compilation(true)
    .define("USE_DOTS")
    .define("vclass", "0")
    .define("pos", "vec3(0.0)")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .fragment_source("overlay_particle_frag.glsl")
    .additional_info("overlay_particle", "draw_mesh");

GPU_SHADER_CREATE_INFO(overlay_particle_dot_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_particle_dot", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_particle_shape)
    .do_static_compilation(true)
    /* Instantiated Attrs. */
    .vertex_in(3, Type::VEC3, "pos")
    .vertex_in(4, Type::INT, "vclass")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("overlay_varying_color.glsl")
    .additional_info("overlay_particle", "draw_modelmat", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(overlay_particle_shape_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_particle_shape", "drw_clipped");

/** \} */
