/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_boundary)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SINT_16_16, write, iimage2D, boundary_img)
COMPUTE_SOURCE("compositor_inpaint_compute_boundary.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_inpaint_fill_region)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, max_distance)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, isampler2D, flooded_boundary_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, filled_region_img)
IMAGE(1, SFLOAT_16, write, image2D, distance_to_boundary_img)
IMAGE(2, SFLOAT_16, write, image2D, smoothing_radius_img)
COMPUTE_SOURCE("compositor_inpaint_fill_region.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_inpaint_compute_region)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, max_distance)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, inpainted_region_tx)
SAMPLER(2, sampler2D, distance_to_boundary_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_inpaint_compute_region.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
