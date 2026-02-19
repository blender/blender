/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_threshold_seeds)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, mask_tx)
IMAGE(0, SINT_16_16, write, iimage2D, masked_pixels_img)
IMAGE(1, SINT_16_16, write, iimage2D, unmasked_pixels_img)
COMPUTE_SOURCE("compositor_morphological_distance_threshold_seeds.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_threshold)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, distance_offset)
PUSH_CONSTANT(float, falloff_size)
SAMPLER(0, sampler2D, mask_tx)
SAMPLER(1, isampler2D, flooded_masked_pixels_tx)
SAMPLER(2, isampler2D, flooded_unmasked_pixels_tx)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_morphological_distance_threshold.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
