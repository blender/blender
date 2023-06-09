/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_cavity_common)
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::FLOAT_2D, "normalBuffer")
    .uniform_buf(WB_WORLD_SLOT, "WorldData", "world_data")
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_effect_cavity_frag.glsl")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(workbench_effect_cavity)
    .do_static_compilation(true)
    .define("USE_CAVITY")
    .uniform_buf(3, "vec4", "samples_coords[512]")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(2, ImageType::FLOAT_2D, "cavityJitter")
    .additional_info("workbench_effect_cavity_common");

GPU_SHADER_CREATE_INFO(workbench_effect_curvature)
    .do_static_compilation(true)
    .define("USE_CURVATURE")
    .sampler(1, ImageType::UINT_2D, "objectIdBuffer")
    .additional_info("workbench_effect_cavity_common");

GPU_SHADER_CREATE_INFO(workbench_effect_cavity_curvature)
    .do_static_compilation(true)
    .define("USE_CAVITY")
    .define("USE_CURVATURE")
    .uniform_buf(3, "vec4", "samples_coords[512]")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(2, ImageType::FLOAT_2D, "cavityJitter")
    .sampler(3, ImageType::UINT_2D, "objectIdBuffer")
    .additional_info("workbench_effect_cavity_common");
