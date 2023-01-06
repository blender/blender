/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_test)
    .typedef_source("GPU_shader_shared.h")
    .fragment_out(0, Type::UVEC4, "out_test")
    .additional_info("draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpu_math_test)
    .fragment_source("gpu_math_test.glsl")
    .additional_info("gpu_shader_test")
    .do_static_compilation(true);
