/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  define HAIR_PHASE_SUBDIV
#  define HAIR_SHADER
#  define DRW_HAIR_INFO
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_hair_refine_compute)
LOCAL_GROUP_SIZE(1, 1)
STORAGE_BUF(0, WRITE, vec4, posTime[])
SAMPLER(0, FLOAT_BUFFER, hairPointBuffer)
SAMPLER(1, UINT_BUFFER, hairStrandBuffer)
SAMPLER(2, UINT_BUFFER, hairStrandSegBuffer)
PUSH_CONSTANT(MAT4, hairDupliMatrix)
PUSH_CONSTANT(BOOL, hairCloseTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(INT, hairThicknessRes)
PUSH_CONSTANT(INT, hairStrandsRes)
PUSH_CONSTANT(INT, hairStrandOffset)
COMPUTE_SOURCE("common_hair_refine_comp.glsl")
DEFINE("HAIR_PHASE_SUBDIV")
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
