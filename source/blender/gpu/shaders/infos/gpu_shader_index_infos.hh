/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_points)
LOCAL_GROUP_SIZE(16, 16, 1)
PUSH_CONSTANT(int, elements_per_curve)
PUSH_CONSTANT(int, ncurves)
STORAGE_BUF(0, write, uint, out_indices[])
COMPUTE_SOURCE("gpu_shader_index_2d_array_points.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_lines)
LOCAL_GROUP_SIZE(16, 16, 1)
PUSH_CONSTANT(int, elements_per_curve)
PUSH_CONSTANT(int, ncurves)
STORAGE_BUF(0, write, uint, out_indices[])
COMPUTE_SOURCE("gpu_shader_index_2d_array_lines.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_tris)
LOCAL_GROUP_SIZE(16, 16, 1)
PUSH_CONSTANT(int, elements_per_curve)
PUSH_CONSTANT(int, ncurves)
STORAGE_BUF(0, write, uint, out_indices[])
COMPUTE_SOURCE("gpu_shader_index_2d_array_tris.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
