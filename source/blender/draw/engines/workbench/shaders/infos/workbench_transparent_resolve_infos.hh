/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_fullscreen_infos.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_transparent_resolve)
FRAGMENT_OUT(0, float4, frag_color)
SAMPLER(0, sampler2D, transparent_accum)
SAMPLER(1, sampler2D, transparent_revealage)
FRAGMENT_SOURCE("workbench_transparent_resolve_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
