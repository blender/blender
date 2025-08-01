/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_distance_threshold)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, radius)
PUSH_CONSTANT(int, distance)
PUSH_CONSTANT(float, inset)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_morphological_distance_threshold.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
