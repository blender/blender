/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "gpu_srgb_to_framebuffer_space_info.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_srgb_to_framebuffer_space)
PUSH_CONSTANT(bool, srgbTarget)
DEFINE("blender_srgb_to_framebuffer_space(a) a")
GPU_SHADER_CREATE_END()
