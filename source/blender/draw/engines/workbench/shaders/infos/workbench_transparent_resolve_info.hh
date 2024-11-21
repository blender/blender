/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_fullscreen_info.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_transparent_resolve)
FRAGMENT_OUT(0, VEC4, fragColor)
SAMPLER(0, FLOAT_2D, transparentAccum)
SAMPLER(1, FLOAT_2D, transparentRevealage)
FRAGMENT_SOURCE("workbench_transparent_resolve_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
