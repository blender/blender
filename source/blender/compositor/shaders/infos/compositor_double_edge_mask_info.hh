/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_double_edge_mask_compute_boundary)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(BOOL, include_all_inner_edges)
PUSH_CONSTANT(BOOL, include_edges_of_image)
SAMPLER(0, FLOAT_2D, inner_mask_tx)
SAMPLER(1, FLOAT_2D, outer_mask_tx)
IMAGE(0, GPU_RG16I, WRITE, INT_2D, inner_boundary_img)
IMAGE(1, GPU_RG16I, WRITE, INT_2D, outer_boundary_img)
COMPUTE_SOURCE("compositor_double_edge_mask_compute_boundary.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_double_edge_mask_compute_gradient)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, inner_mask_tx)
SAMPLER(1, FLOAT_2D, outer_mask_tx)
SAMPLER(2, INT_2D, flooded_inner_boundary_tx)
SAMPLER(3, INT_2D, flooded_outer_boundary_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_double_edge_mask_compute_gradient.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
