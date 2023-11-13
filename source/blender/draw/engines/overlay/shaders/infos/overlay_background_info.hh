/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_background)
    .do_static_compilation(true)
    .typedef_source("overlay_shader_shared.h")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .push_constant(Type::INT, "bgType")
    .push_constant(Type::VEC4, "colorOverride")
    .fragment_source("overlay_background_frag.glsl")
    .fragment_out(0, Type::VEC4, "fragColor")
    .additional_info("draw_fullscreen", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_clipbound)
    .do_static_compilation(true)
    .push_constant(Type::VEC4, "ucolor")
    .push_constant(Type::VEC3, "boundbox", 8)
    .vertex_source("overlay_clipbound_vert.glsl")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("overlay_uniform_color_frag.glsl")
    .additional_info("draw_view");
