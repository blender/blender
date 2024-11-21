/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_tone_map_photoreceptor)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(VEC4, global_adaptation_level)
PUSH_CONSTANT(FLOAT, contrast)
PUSH_CONSTANT(FLOAT, intensity)
PUSH_CONSTANT(FLOAT, chromatic_adaptation)
PUSH_CONSTANT(FLOAT, light_adaptation)
PUSH_CONSTANT(VEC3, luminance_coefficients)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_tone_map_photoreceptor.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
