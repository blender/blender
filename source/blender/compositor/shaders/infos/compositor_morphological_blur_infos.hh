/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16, read_write, image2D, blurred_input_img)
COMPUTE_SOURCE("compositor_morphological_blur.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_dilate)
ADDITIONAL_INFO(compositor_morphological_blur_shared)
DEFINE_VALUE("OPERATOR(x, y)", "max(x, y)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_morphological_blur_erode)
ADDITIONAL_INFO(compositor_morphological_blur_shared)
DEFINE_VALUE("OPERATOR(x, y)", "min(x, y)")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
