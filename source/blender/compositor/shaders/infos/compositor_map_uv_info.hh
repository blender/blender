/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_map_uv_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, uv_tx)
IMAGE(0, GPU_RGBA16F, write, image2D, output_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_map_uv_anisotropic)
ADDITIONAL_INFO(compositor_map_uv_shared)
COMPUTE_SOURCE("compositor_map_uv_anisotropic.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_map_uv_nearest_neighbour)
ADDITIONAL_INFO(compositor_map_uv_shared)
COMPUTE_SOURCE("compositor_map_uv_nearest_neighbour.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
