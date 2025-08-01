/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_gamma_correct_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_gamma_correct.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_gamma_correct)
ADDITIONAL_INFO(compositor_gamma_correct_shared)
DEFINE_VALUE("FUNCTION(x)", "(x * x)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_gamma_uncorrect)
ADDITIONAL_INFO(compositor_gamma_correct_shared)
DEFINE_VALUE("FUNCTION(x)", "sqrt(x)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
