/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_fullscreen_infos.hh"

#  include "workbench_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_outline)
TYPEDEF_SOURCE("workbench_shader_shared.hh")
FRAGMENT_SOURCE("workbench_effect_outline_frag.glsl")
SAMPLER(0, usampler2D, object_id_buffer)
UNIFORM_BUF(WB_WORLD_SLOT, WorldData, world_data)
FRAGMENT_OUT(0, float4, frag_color)
ADDITIONAL_INFO(gpu_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
