/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_merge_infront)
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .fragment_source("workbench_merge_infront_frag.glsl")
    .additional_info("draw_fullscreen")
    .depth_write(DepthWrite::ANY)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_next_merge_depth)
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .fragment_source("workbench_next_merge_depth_frag.glsl")
    .additional_info("draw_fullscreen")
    .depth_write(DepthWrite::ANY)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_extract_stencil)
    .fragment_out(0, Type::UINT, "out_stencil_value")
    .fragment_source("workbench_extract_stencil.glsl")
    .additional_info("draw_fullscreen")
    .do_static_compilation(true);
