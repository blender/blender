/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(image_engine_color_iface, "").smooth(Type::VEC2, "uv_screen");

GPU_SHADER_CREATE_INFO(image_engine_color_shader)
    .vertex_in(0, Type::IVEC2, "pos")
    .vertex_out(image_engine_color_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::VEC4, "shuffle")
    .push_constant(Type::VEC2, "farNearDistances")
    .push_constant(Type::IVEC2, "offset")
    .push_constant(Type::INT, "drawFlags")
    .push_constant(Type::BOOL, "imgPremultiplied")
    .sampler(0, ImageType::FLOAT_2D, "imageTexture")
    .sampler(1, ImageType::DEPTH_2D, "depth_texture")
    .vertex_source("image_engine_color_vert.glsl")
    .fragment_source("image_engine_color_frag.glsl")
    .additional_info("draw_modelmat")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(image_engine_depth_iface, "").smooth(Type::VEC2, "uv_image");

GPU_SHADER_CREATE_INFO(image_engine_depth_shader)
    .vertex_in(0, Type::IVEC2, "pos")
    .vertex_in(1, Type::VEC2, "uv")
    .vertex_out(image_engine_depth_iface)
    .push_constant(Type::VEC4, "min_max_uv")
    .vertex_source("image_engine_depth_vert.glsl")
    .fragment_source("image_engine_depth_frag.glsl")
    .additional_info("draw_modelmat")
    .depth_write(DepthWrite::ANY)
    .do_static_compilation(true);
