/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_step_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, radius)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_morphological_step.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_step_dilate)
ADDITIONAL_INFO(compositor_morphological_step_shared)
DEFINE_VALUE("OPERATOR(a, b)", "max(a, b)")
DEFINE_VALUE("LIMIT", "-FLT_MAX")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_step_erode)
ADDITIONAL_INFO(compositor_morphological_step_shared)
DEFINE_VALUE("OPERATOR(a, b)", "min(a, b)")
DEFINE_VALUE("LIMIT", "FLT_MAX")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
