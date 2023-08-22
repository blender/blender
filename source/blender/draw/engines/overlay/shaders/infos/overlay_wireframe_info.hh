/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_wireframe_iface, "")
    .flat(Type::VEC2, "edgeStart")
    .smooth(Type::VEC4, "finalColor")
    .no_perspective(Type::VEC2, "edgePos");

GPU_SHADER_CREATE_INFO(overlay_wireframe)
    .do_static_compilation(true)
    .push_constant(Type::FLOAT, "wireStepParam")
    .push_constant(Type::FLOAT, "wireOpacity")
    .push_constant(Type::BOOL, "useColoring")
    .push_constant(Type::BOOL, "isTransform")
    .push_constant(Type::BOOL, "isObjectColor")
    .push_constant(Type::BOOL, "isRandomColor")
    .push_constant(Type::BOOL, "isHair")
    .push_constant(Type::MAT4, "hairDupliMatrix")
    /* Scene Depth texture copy for manual depth test. */
    .sampler(0, ImageType::DEPTH_2D, "depthTex")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_in(2, Type::FLOAT, "wd") /* wire-data. */
    .vertex_out(overlay_wireframe_iface)
    .vertex_source("overlay_wireframe_vert.glsl")
    .fragment_source("overlay_wireframe_frag.glsl")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput")
    .depth_write(DepthWrite::ANY)
    .additional_info("draw_mesh", "draw_object_infos", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_wireframe_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_wireframe", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_wireframe_custom_depth)
    .do_static_compilation(true)
    .define("CUSTOM_DEPTH_BIAS")
    .additional_info("overlay_wireframe");

GPU_SHADER_CREATE_INFO(overlay_wireframe_custom_depth_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_wireframe_custom_depth", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_wireframe_select)
    .do_static_compilation(true)
    .define("SELECT_EDGES")
    .additional_info("overlay_wireframe");

GPU_SHADER_CREATE_INFO(overlay_wireframe_select_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_wireframe_select", "drw_clipped");
