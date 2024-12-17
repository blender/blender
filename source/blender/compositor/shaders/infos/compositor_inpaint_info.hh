/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_boundary)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RG16I, WRITE, INT_2D, boundary_img)
COMPUTE_SOURCE("compositor_inpaint_compute_boundary.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_inpaint_fill_region)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, max_distance)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, INT_2D, flooded_boundary_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, filled_region_img)
IMAGE(1, GPU_R16F, WRITE, FLOAT_2D, distance_to_boundary_img)
IMAGE(2, GPU_R16F, WRITE, FLOAT_2D, smoothing_radius_img)
COMPUTE_SOURCE("compositor_inpaint_fill_region.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_region)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, max_distance)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, inpainted_region_tx)
SAMPLER(2, FLOAT_2D, distance_to_boundary_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_inpaint_compute_region.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
