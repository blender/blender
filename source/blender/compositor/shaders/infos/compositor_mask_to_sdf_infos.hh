/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_mask_to_sdf_compute_boundary)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, Int2D, mask_tx)
IMAGE(0, SINT_16_16, write, Int2D, boundary_img)
COMPUTE_SOURCE("compositor_mask_to_sdf_compute_boundary.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_mask_to_sdf_compute_distance)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, Int2D, mask_tx)
SAMPLER(1, Int2D, flooded_boundary_tx)
IMAGE(0, SFLOAT_16, write, Float2D, distance_img)
COMPUTE_SOURCE("compositor_mask_to_sdf_compute_distance.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
