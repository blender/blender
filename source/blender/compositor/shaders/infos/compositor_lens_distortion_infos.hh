/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* Radial Lens Distortion. */

GPU_SHADER_CREATE_INFO(compositor_radial_lens_distortion_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float3, chromatic_distortion)
PUSH_CONSTANT(float, scale)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_radial_lens_distortion.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_radial_lens_distortion)
ADDITIONAL_INFO(compositor_radial_lens_distortion_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_radial_lens_distortion_jitter)
ADDITIONAL_INFO(compositor_radial_lens_distortion_shared)
DEFINE("JITTER")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* Horizontal Lens Distortion. */

GPU_SHADER_CREATE_INFO(compositor_horizontal_lens_distortion)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, dispersion)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_horizontal_lens_distortion.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
