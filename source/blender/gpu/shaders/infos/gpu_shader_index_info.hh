/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_points)
    .local_group_size(16, 16, 1)
    .push_constant(Type::INT, "elements_per_curve")
    .push_constant(Type::INT, "ncurves")
    .storage_buf(0, Qualifier::WRITE, "uint", "out_indices[]")
    .compute_source("gpu_shader_index_2d_array_points.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_lines)
    .local_group_size(16, 16, 1)
    .push_constant(Type::INT, "elements_per_curve")
    .push_constant(Type::INT, "ncurves")
    .storage_buf(0, Qualifier::WRITE, "uint", "out_indices[]")
    .compute_source("gpu_shader_index_2d_array_lines.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_index_2d_array_tris)
    .local_group_size(16, 16, 1)
    .push_constant(Type::INT, "elements_per_curve")
    .push_constant(Type::INT, "ncurves")
    .storage_buf(0, Qualifier::WRITE, "uint", "out_indices[]")
    .compute_source("gpu_shader_index_2d_array_tris.glsl")
    .do_static_compilation(true);
