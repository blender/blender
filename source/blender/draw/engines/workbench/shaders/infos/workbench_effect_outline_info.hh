/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_fullscreen_info.hh"

#  include "workbench_shader_shared.h"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_outline)
TYPEDEF_SOURCE("workbench_shader_shared.h")
FRAGMENT_SOURCE("workbench_effect_outline_frag.glsl")
SAMPLER(0, UINT_2D, objectIdBuffer)
UNIFORM_BUF(WB_WORLD_SLOT, WorldData, world_data)
FRAGMENT_OUT(0, VEC4, fragColor)
ADDITIONAL_INFO(draw_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
