/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"

#  define HAIR_PHASE_SUBDIV
#  define HAIR_SHADER
#  define DRW_HAIR_INFO
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_hair_refine_compute)
LOCAL_GROUP_SIZE(1, 1)
STORAGE_BUF(0, write, float4, posTime[])
/* Per strands data. */
SAMPLER(1, usamplerBuffer, hairStrandBuffer)
SAMPLER(2, usamplerBuffer, hairStrandSegBuffer)
COMPUTE_SOURCE("draw_hair_refine_comp.glsl")
DEFINE("HAIR_PHASE_SUBDIV")
ADDITIONAL_INFO(draw_hair)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
