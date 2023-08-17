/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Outline Pre-pass
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_outline_prepass_iface, "interp").flat(Type::UINT, "ob_id");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass)
    .push_constant(Type::BOOL, "isTransform")
    .vertex_out(overlay_outline_prepass_iface)
    /* Using uint because 16bit uint can contain more ids than int. */
    .fragment_out(0, Type::UINT, "out_object_id")
    .fragment_source("overlay_outline_prepass_frag.glsl")
    .additional_info("draw_resource_handle", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_mesh)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_source("overlay_outline_prepass_vert.glsl")
    .additional_info("draw_mesh", "overlay_outline_prepass")
    .additional_info("draw_object_infos");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_mesh_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_mesh", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_outline_prepass_wire_iface, "vert").flat(Type::VEC3, "pos");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_curves)
    .do_static_compilation(true)
    .vertex_source("overlay_outline_prepass_curves_vert.glsl")
    .additional_info("draw_hair", "overlay_outline_prepass")
    .additional_info("draw_object_infos");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_curves_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_curves", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire_common)
    .vertex_in(0, Type::VEC3, "pos")
    .additional_info("draw_mesh", "overlay_outline_prepass")
    .additional_info("draw_object_infos");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_wire_common")
    .define("USE_GEOM")
    .vertex_out(overlay_outline_prepass_wire_iface)
    .geometry_layout(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::LINE_STRIP, 2)
    .geometry_out(overlay_outline_prepass_iface)
    .vertex_source("overlay_outline_prepass_vert.glsl")
    .geometry_source("overlay_outline_prepass_geom.glsl");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_wire_common")
    .vertex_source("overlay_outline_prepass_vert_no_geom.glsl");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_wire", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_outline_prepass_gpencil_flat_iface, "gp_interp_flat")
    .flat(Type::VEC2, "aspect")
    .flat(Type::VEC4, "sspos");
GPU_SHADER_INTERFACE_INFO(overlay_outline_prepass_gpencil_noperspective_iface,
                          "gp_interp_noperspective")
    .no_perspective(Type::VEC2, "thickness")
    .no_perspective(Type::FLOAT, "hardness");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_gpencil)
    .do_static_compilation(true)
    .push_constant(Type::BOOL, "isTransform")
    .vertex_out(overlay_outline_prepass_iface)
    .vertex_out(overlay_outline_prepass_gpencil_flat_iface)
    .vertex_out(overlay_outline_prepass_gpencil_noperspective_iface)
    .vertex_source("overlay_outline_prepass_gpencil_vert.glsl")
    .push_constant(Type::BOOL, "gpStrokeOrder3d") /* TODO(fclem): Move to a GPencil object UBO. */
    .push_constant(Type::VEC4, "gpDepthPlane")    /* TODO(fclem): Move to a GPencil object UBO. */
    /* Using uint because 16bit uint can contain more ids than int. */
    .fragment_out(0, Type::UINT, "out_object_id")
    .fragment_source("overlay_outline_prepass_gpencil_frag.glsl")
    .depth_write(DepthWrite::ANY)
    .additional_info("draw_gpencil", "draw_resource_handle", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_gpencil_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_gpencil", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_pointcloud)
    .do_static_compilation(true)
    .vertex_source("overlay_outline_prepass_pointcloud_vert.glsl")
    .additional_info("draw_pointcloud", "overlay_outline_prepass")
    .additional_info("draw_object_infos");

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_pointcloud_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_outline_prepass_pointcloud", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outline Rendering
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_outline_detect)
    .do_static_compilation(true)
    .push_constant(Type::FLOAT, "alphaOcclu")
    .push_constant(Type::BOOL, "isXrayWires")
    .push_constant(Type::BOOL, "doAntiAliasing")
    .push_constant(Type::BOOL, "doThickOutlines")
    .sampler(0, ImageType::UINT_2D, "outlineId")
    .sampler(1, ImageType::DEPTH_2D, "outlineDepth")
    .sampler(2, ImageType::DEPTH_2D, "sceneDepth")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .fragment_source("overlay_outline_detect_frag.glsl")
    .additional_info("draw_fullscreen", "draw_view", "draw_globals");

/** \} */
