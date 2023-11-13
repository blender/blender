/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_antialiasing)
    .do_static_compilation(true)
    .sampler(0, ImageType::DEPTH_2D, "depthTex")
    .sampler(1, ImageType::FLOAT_2D, "colorTex")
    .sampler(2, ImageType::FLOAT_2D, "lineTex")
    .push_constant(Type::BOOL, "doSmoothLines")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("overlay_antialiasing_frag.glsl")
    .additional_info("draw_fullscreen", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_xray_fade)
    .do_static_compilation(true)
    .sampler(0, ImageType::DEPTH_2D, "depthTex")
    .sampler(1, ImageType::DEPTH_2D, "xrayDepthTex")
    .push_constant(Type::FLOAT, "opacity")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("overlay_xray_fade_frag.glsl")
    .additional_info("draw_fullscreen");
