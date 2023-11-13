/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_transparent_resolve)
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::FLOAT_2D, "transparentAccum")
    .sampler(1, ImageType::FLOAT_2D, "transparentRevealage")
    .fragment_source("workbench_transparent_resolve_frag.glsl")
    .additional_info("draw_fullscreen")
    .do_static_compilation(true);
