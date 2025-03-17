/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "eevee_common_info.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_lut)
LOCAL_GROUP_SIZE(LUT_WORKGROUP_SIZE, LUT_WORKGROUP_SIZE, 1)
PUSH_CONSTANT(INT, table_type)
PUSH_CONSTANT(IVEC3, table_extent)
IMAGE(0, GPU_RGBA32F, READ_WRITE, FLOAT_3D, table_img)
ADDITIONAL_INFO(eevee_shared)
COMPUTE_SOURCE("eevee_lut_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
