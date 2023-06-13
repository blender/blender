/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_shader_3D_polyline_iface, "interp")
    .smooth(Type::VEC4, "final_color")
    .smooth(Type::FLOAT, "clip")
    .no_perspective(Type::FLOAT, "smoothline");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline)
    .define("SMOOTH_WIDTH", "1.0")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC2, "viewportSize")
    .push_constant(Type::FLOAT, "lineWidth")
    .push_constant(Type::BOOL, "lineSmooth")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(gpu_shader_3D_polyline_iface)
    .geometry_layout(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
    .geometry_out(gpu_shader_3D_polyline_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("gpu_shader_3D_polyline_vert.glsl")
    .geometry_source("gpu_shader_3D_polyline_geom.glsl")
    .fragment_source("gpu_shader_3D_polyline_frag.glsl")
    .additional_info("gpu_srgb_to_framebuffer_space");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_no_geom)
    .define("SMOOTH_WIDTH", "1.0")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC2, "viewportSize")
    .push_constant(Type::FLOAT, "lineWidth")
    .push_constant(Type::BOOL, "lineSmooth")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(gpu_shader_3D_polyline_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("gpu_shader_3D_polyline_vert_no_geom.glsl")
    .fragment_source("gpu_shader_3D_polyline_frag.glsl")
    .additional_info("gpu_srgb_to_framebuffer_space");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color)
    .do_static_compilation(true)
    .define("UNIFORM")
    .push_constant(Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .define("UNIFORM")
    .push_constant(Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline_no_geom");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped)
    .do_static_compilation(true)
    /* TODO(fclem): Put in a UBO to fit the 128byte requirement. */
    .push_constant(Type::MAT4, "ModelMatrix")
    .push_constant(Type::VEC4, "ClipPlane")
    .define("CLIP")
    .additional_info("gpu_shader_3D_polyline_uniform_color");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_uniform_color_clipped_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    /* TODO(fclem): Put in an UBO to fit the 128byte requirement. */
    .push_constant(Type::MAT4, "ModelMatrix")
    .push_constant(Type::VEC4, "ClipPlane")
    .define("CLIP")
    .additional_info("gpu_shader_3D_polyline_uniform_color_no_geom");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color)
    .do_static_compilation(true)
    .define("FLAT")
    .vertex_in(1, Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_flat_color_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .define("FLAT")
    .vertex_in(1, Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline_no_geom");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color)
    .do_static_compilation(true)
    .define("SMOOTH")
    .vertex_in(1, Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline");

GPU_SHADER_CREATE_INFO(gpu_shader_3D_polyline_smooth_color_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .define("SMOOTH")
    .vertex_in(1, Type::VEC4, "color")
    .additional_info("gpu_shader_3D_polyline_no_geom");
