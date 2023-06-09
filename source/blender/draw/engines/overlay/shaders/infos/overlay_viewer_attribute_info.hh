/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_viewer_attribute_iface, "").smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_common).push_constant(Type::FLOAT, "opacity");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_mesh)
    .do_static_compilation(true)
    .vertex_source("overlay_viewer_attribute_mesh_vert.glsl")
    .fragment_source("overlay_viewer_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "attribute_value")
    .vertex_out(overlay_viewer_attribute_iface)
    .additional_info("overlay_viewer_attribute_common", "draw_mesh");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_mesh_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_viewer_attribute_mesh", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_pointcloud)
    .do_static_compilation(true)
    .vertex_source("overlay_viewer_attribute_pointcloud_vert.glsl")
    .fragment_source("overlay_viewer_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .sampler(3, ImageType::FLOAT_BUFFER, "attribute_tx")
    .vertex_out(overlay_viewer_attribute_iface)
    .additional_info("overlay_viewer_attribute_common", "draw_pointcloud");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_pointcloud_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_viewer_attribute_pointcloud", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_curve)
    .do_static_compilation(true)
    .vertex_source("overlay_viewer_attribute_curve_vert.glsl")
    .fragment_source("overlay_viewer_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "attribute_value")
    .vertex_out(overlay_viewer_attribute_iface)
    .additional_info("overlay_viewer_attribute_common", "draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_curve_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_viewer_attribute_curve", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_curves)
    .do_static_compilation(true)
    .vertex_source("overlay_viewer_attribute_curves_vert.glsl")
    .fragment_source("overlay_viewer_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .sampler(0, ImageType::FLOAT_BUFFER, "color_tx")
    .push_constant(Type::BOOL, "is_point_domain")
    .vertex_out(overlay_viewer_attribute_iface)
    .additional_info("overlay_viewer_attribute_common", "draw_hair");

GPU_SHADER_CREATE_INFO(overlay_viewer_attribute_curves_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_viewer_attribute_curves", "drw_clipped");
